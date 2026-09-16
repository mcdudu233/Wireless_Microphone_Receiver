#include "logger.h"
#include "config.h"
#include "module/rf.h"
#include "module/screen.h"
#include "module/audio/buffer.h"
#include "module/audio/decoder.h"
#include "tool/device.h"
#include "ui/ui.h"
#include "ui/ui_bt.h"
#include "ui/ui_main.h"
#include "ui/ui_setting.h"

#include "string"
#include "freertos/idf_additions.h"

// 缓存的设备
static DeviceManager deviceManager;

// 计算音频传输速度
static uint32_t transmitSpeed = 0;
static uint32_t transmitSpeedData = 0;

#ifdef BUILD_DEBUG
struct RfDebugSnapshot
{
  bool ready;
  uint32_t packets;
  uint32_t bytes;
  uint32_t max_burst;
  uint32_t mailbox_peak;
  uint32_t budget_hits;
  uint32_t max_drain_us;
  uint32_t ble_packets;
  uint32_t ble_bytes;
  uint8_t level_l;
  uint8_t level_r;
};

static portMUX_TYPE rf_debug_mux = portMUX_INITIALIZER_UNLOCKED;
static RfDebugSnapshot rf_debug_snapshot = {};

static void rfDebugHandle(void *arg)
{
  (void)arg;
  // 错开解码诊断，避免两个长日志同时占用串口。
  vTaskDelay(pdMS_TO_TICKS(750));
  while (true)
  {
    RfDebugSnapshot snapshot = {};
    portENTER_CRITICAL(&rf_debug_mux);
    if (rf_debug_snapshot.ready)
    {
      snapshot = rf_debug_snapshot;
      rf_debug_snapshot.ready = false;
    }
    portEXIT_CRITICAL(&rf_debug_mux);
    if (snapshot.ready)
    {
      if (snapshot.ble_packets > 0)
      {
        LOGGER_INFO("Audio RX BLE packets=%lu bytes=%lu level_l=%u level_r=%u",
                    static_cast<unsigned long>(snapshot.ble_packets),
                    static_cast<unsigned long>(snapshot.ble_bytes),
                    static_cast<unsigned int>(snapshot.level_l), static_cast<unsigned int>(snapshot.level_r));
      }
      else
      {
        LOGGER_INFO("Audio RX WiFi packets=%lu bytes=%lu max_burst=%lu mailbox_peak=%lu budget_hits=%lu max_drain_us=%lu level_l=%u level_r=%u",
                    static_cast<unsigned long>(snapshot.packets), static_cast<unsigned long>(snapshot.bytes),
                    static_cast<unsigned long>(snapshot.max_burst), static_cast<unsigned long>(snapshot.mailbox_peak),
                    static_cast<unsigned long>(snapshot.budget_hits),
                    static_cast<unsigned long>(snapshot.max_drain_us),
                    static_cast<unsigned int>(snapshot.level_l), static_cast<unsigned int>(snapshot.level_r));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
#endif

/*****************************
          WIFI协议
*****************************/
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/api.h"
static bool wifiIsOpen = false;
static bool wifiIsClosing = false;
// socket互斥锁:socket会被rf任务(收发)与协议切换任务(重建/释放)并发访问,
// netconn在另一线程netconn_recv期间被delete会造成lwIP内存破坏导致卡死
static SemaphoreHandle_t wifiSocketMutex = nullptr;
static bool netifInitialized = false;
static bool eventLoopInitialized = false;
static uint32_t wifiIP;
static esp_netif_t *wifiNetIF;
static esp_event_handler_instance_t wifiHandlerInstance1;
static esp_event_handler_instance_t wifiHandlerInstance2;
static bool socketIsOpen = false;
static netconn *socketSendInstance = NULL;
static netconn *socketReceiveInstance = NULL;

// RAW netconn默认仅有6项邮箱。音频AMPDU会成批抵达，先在连接尚未收包时换成大邮箱，
// 防止tcpip线程因邮箱满而在应用层之前静默丢弃音频分片。
static bool wifi_expand_receive_mailbox(netconn *connection)
{
  if (connection == nullptr || !sys_mbox_valid(&connection->recvmbox))
  {
    return false;
  }

  sys_mbox_t replacement = nullptr;
  if (sys_mbox_new(&replacement, RF_WIFI_RAW_RX_MBOX_SIZE) != ERR_OK)
  {
    return false;
  }
  sys_mbox_t original = connection->recvmbox;
  connection->recvmbox = replacement;
  sys_mbox_free(&original);
#if LWIP_SO_RCVBUF
  netconn_set_recvbufsize(connection, RF_WIFI_RAW_RX_MBOX_SIZE * 1600);
#endif
  return true;
}

// 发送数据 需要 netbuf_new (会被rf任务/切换任务/界面回调并发调用,由互斥锁串行化)
static bool wifi_send(const Device &device, netbuf *buf)
{
  if (buf == NULL)
  {
    return false;
  }
  bool sent = false;
  if (xSemaphoreTake(wifiSocketMutex, pdMS_TO_TICKS(500)) == pdTRUE)
  {
    if (socketSendInstance != NULL && device.isWifiConnected())
    {
      // 双栈 lwIP 的 ip_addr_t 含 type 字段，必须初始化(type=0 即 IPv4)，
      // 否则未初始化的 type 会让 raw_sendto 走错协议栈导致发送失败
      ip_addr_t socketDestination = {};
      socketDestination.u_addr.ip4.addr = htonl(device.getWifiIP());
      err_t err = netconn_sendto(socketSendInstance, buf, &socketDestination, WIFI_NO_PORT);
      if (err == ERR_OK)
      {
        sent = true;
      }
      else
      {
        LOGGER_WARN("WiFi Socket send failed: %d", err);
      }
    }
    else
    {
      LOGGER_WARN("WiFi Socket send failed, device haven't connect to WiFi.");
    }
    xSemaphoreGive(wifiSocketMutex);
  }
  else
  {
    LOGGER_WARN("WiFi Socket send failed: socket is busy.");
  }
  netbuf_delete(buf);
  return sent;
}

// 读取数据 需要 netbuf_delete
static bool wifi_receive(Device **device, netbuf **buf)
{
  *buf = NULL;
  if (xSemaphoreTake(wifiSocketMutex, 0) != pdTRUE)
  {
    // socket正在重建/释放时直接跳过本轮,避免与netconn_delete竞争
    return false;
  }
  if (socketReceiveInstance == NULL)
  {
    xSemaphoreGive(wifiSocketMutex);
    return false;
  }
  err_t err = netconn_recv(socketReceiveInstance, buf);
  xSemaphoreGive(wifiSocketMutex);
  if (err == ERR_WOULDBLOCK)
  {
    return false;
  }
  else if (err != ERR_OK)
  {
    LOGGER_WARN("WiFi Socket receive failed: %d", err);
    return false;
  }
  // 解析数据包 receiveBuffer->addr.addr
  (*device) = deviceManager.getDeviceByWifiIP(htonl((*buf)->addr.u_addr.ip4.addr));
  if ((*device) == nullptr)
  {
      LOGGER_WARN("WiFi Socket can't get device by unknowed IP address");
    netbuf_delete(*buf);
    *buf = NULL;
    return false;
  }
  return true;
}

// 以下两个_locked函数在持有wifiSocketMutex时执行,内部可直接return
static void wifi_socket_close_locked()
{
  if (socketIsOpen)
  {
    socketIsOpen = false;
    if (socketSendInstance != NULL)
    {
      netconn_delete(socketSendInstance);
      socketSendInstance = NULL;
    }
    if (socketReceiveInstance != NULL)
    {
      netconn_delete(socketReceiveInstance);
      socketReceiveInstance = NULL;
    }
  }
}

static bool wifi_socket_open_locked(uint32_t localIP)
{
  // 创建
  socketSendInstance = netconn_new_with_proto_and_callback(NETCONN_RAW, WIFI_IP_PROTOCOL, NULL);
  if (socketSendInstance == NULL)
  {
    LOGGER_WARN("WiFi Socket unable to create!");
    return false;
  }
  socketReceiveInstance = netconn_new_with_proto_and_callback(NETCONN_RAW, WIFI_IP_PROTOCOL, NULL);
  if (socketReceiveInstance == NULL)
  {
    netconn_delete(socketSendInstance);
    socketSendInstance = NULL;
    LOGGER_WARN("WiFi Socket unable to create!");
    return false;
  }
  if (!wifi_expand_receive_mailbox(socketReceiveInstance))
  {
    netconn_delete(socketSendInstance);
    socketSendInstance = NULL;
    netconn_delete(socketReceiveInstance);
    socketReceiveInstance = NULL;
    LOGGER_WARN("WiFi Socket unable to expand RAW receive mailbox!");
    return false;
  }

  // 绑定到指定地址
  ip_addr_t local_ip = {};
  local_ip.u_addr.ip4.addr = localIP;
  err_t ret = netconn_bind(socketSendInstance, &local_ip, WIFI_NO_PORT);
  if (ret != ERR_OK)
  {
    netconn_delete(socketSendInstance);
    socketSendInstance = NULL;
    netconn_delete(socketReceiveInstance);
    socketReceiveInstance = NULL;
    LOGGER_WARN("WiFi Socket netconn bind failed: %d", ret);
    return false;
  }
  ret = netconn_bind(socketReceiveInstance, IP_ADDR_ANY, WIFI_NO_PORT);
  if (ret != ERR_OK)
  {
    netconn_delete(socketSendInstance);
    socketSendInstance = NULL;
    netconn_delete(socketReceiveInstance);
    socketReceiveInstance = NULL;
    LOGGER_WARN("WiFi Socket netconn bind failed: %d", ret);
    return false;
  }

  // 设置非阻塞模式
  netconn_set_nonblocking(socketReceiveInstance, true);

  socketIsOpen = true;
  return true;
}

static bool wifi_socket_close()
{
  // 与收发路径互斥:防止在rf任务netconn_recv执行期间释放netconn
  if (xSemaphoreTake(wifiSocketMutex, portMAX_DELAY) != pdTRUE)
  {
    LOGGER_WARN("WiFi socket close: mutex take failed.");
    return false;
  }
  wifi_socket_close_locked();
  xSemaphoreGive(wifiSocketMutex);
  LOGGER_INFO("WiFi Socket is shutdown.");
  return true;
}

static bool wifi_socket_open(uint32_t localIP)
{
  if (socketIsOpen)
  {
    wifi_socket_close();
  }
  if (xSemaphoreTake(wifiSocketMutex, portMAX_DELAY) != pdTRUE)
  {
    LOGGER_WARN("WiFi socket open: mutex take failed.");
    return false;
  }
  const bool ok = wifi_socket_open_locked(localIP);
  xSemaphoreGive(wifiSocketMutex);
  if (!ok)
  {
    LOGGER_WARN("WiFi Socket open failed.");
    return false;
  }
  LOGGER_INFO("WiFi Socket is started, raw RX mailbox=%u.", RF_WIFI_RAW_RX_MBOX_SIZE);
  return true;
}

static void wifi_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  // 关闭流程中的在途事件一律忽略,避免在WiFi释放期间更新设备状态
  if (wifiIsClosing)
  {
    return;
  }
  if (event_base == WIFI_EVENT)
  {
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
      wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
      // Device *device = deviceManager.getDeviceByWifiMAC(event->mac);
      LOGGER_INFO("WiFi station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
      wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
      Device *device = deviceManager.getDeviceByWifiMAC(event->mac);
      if (device != nullptr)
      {
        device->setWifiConnected(false);
        LOGGER_INFO("WiFi station " MACSTR " leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
      }
      else
      {
        LOGGER_WARN("WiFi can't find device by MAC " MACSTR, MAC2STR(event->mac));
      }
    }
  }
  else if (event_base == IP_EVENT)
  {
    if (event_id == IP_EVENT_AP_STAIPASSIGNED)
    {
      ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
      // 记录映射
      deviceManager.bindWifiMACToIP(event->mac, htonl(event->ip.addr)); // 转换IP字节序
      Device *device = deviceManager.getDeviceByWifiMAC(event->mac);
      if (device != nullptr)
      {
        device->setWifiConnected(true);
        LOGGER_INFO("WiFi station " MACSTR " IP is assigned", MAC2STR(event->mac));
        logger::memory("after WiFi station IP");
      }
      else
      {
        LOGGER_WARN("WiFi can't find device by MAC " MACSTR, MAC2STR(event->mac));
      }
    }
  }
}

static bool wifi_close()
{
  if (wifiIsOpen)
  {
    wifiIsClosing = true;
    // socket释放持互斥锁执行,与rf任务的收发路径互斥
    wifi_socket_close();
    wifiIsOpen = false;

    // 先注销事件回调再停止WiFi:注销后esp_wifi_stop派发的事件不再进入回调,
    // 事件任务中在途的回调由wifiIsClosing标志拦截
    esp_err_t err;
    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiHandlerInstance1);
    if (err != ESP_OK)
    {
      LOGGER_WARN("WiFi esp_event_handler_instance_unregister failed! Reason=%s", esp_err_to_name(err));
    }
    err = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, wifiHandlerInstance2);
    if (err != ESP_OK)
    {
      LOGGER_WARN("WiFi esp_event_handler_instance_unregister failed! Reason=%s", esp_err_to_name(err));
    }

    // 逐步释放并容忍个别步骤出错,保证清理完整执行到底
    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
      LOGGER_WARN("WiFi esp_wifi_stop failed! Reason=%s", esp_err_to_name(err));
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT)
    {
      LOGGER_WARN("WiFi esp_wifi_deinit failed! Reason=%s", esp_err_to_name(err));
    }
    esp_netif_destroy(wifiNetIF);
    wifiIsClosing = false;
  }
  LOGGER_INFO("WiFi is shutdown.");
  return true;
}

static bool wifi_open()
{
  if (wifiIsOpen)
  {
    wifi_close();
  }

  esp_err_t err = ESP_OK;
  // 创建网络接口和默认事件循环；它们是进程级资源，只初始化一次。
  if (!netifInitialized)
  {
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
      LOGGER_ERROR("WiFi esp_netif_init failed! Reason=%s", esp_err_to_name(err));
      return false;
    }
    netifInitialized = true;
  }
  if (!eventLoopInitialized)
  {
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
      LOGGER_ERROR("WiFi esp_event_loop_create_default failed! Reason=%s", esp_err_to_name(err));
      return false;
    }
    eventLoopInitialized = true;
  }
  wifiNetIF = esp_netif_create_default_wifi_ap();
  // 网络接口已创建,之后的失败路径统一走wifi_close()完整清理
  wifiIsOpen = true;

  // 初始化 WiFi
  static wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&wifi_init_config);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_wifi_init failed! Reason=%s", esp_err_to_name(err));
    wifi_close();
    return false;
  }

  // 注册事件
  err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handle, NULL, &wifiHandlerInstance1);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_event_handler_instance_register failed! Reason=%s", esp_err_to_name(err));
    wifi_close();
    return false;
  }
  err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handle, NULL, &wifiHandlerInstance2);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_event_handler_instance_register failed! Reason=%s", esp_err_to_name(err));
    wifi_close();
    return false;
  }

  // 启动 WiFi AP
  static wifi_config_t wifi_config = {
      .ap = {
          .ssid = WIFI_NAME,
          .password = WIFI_PASSWORD,
          .ssid_len = strlen(WIFI_NAME),
          .channel = WIFI_CHANNEL,
          .authmode = WIFI_AUTH_WPA2_PSK,
          .ssid_hidden = 1,
          .max_connection = RF_MAX_CONNECTION,
          .pmf_cfg = {
              .required = true,
          },
          .gtk_rekey_interval = true,
      },
  };
  err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_wifi_set_mode failed! Reason=%s", esp_err_to_name(err));
    wifi_close();
    return false;
  }
  err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_wifi_set_config failed! Reason=%s", esp_err_to_name(err));
    wifi_close();
    return false;
  }
  err = esp_wifi_start();
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_wifi_start failed! Reason=%s", esp_err_to_name(err));
    wifi_close();
    return false;
  }

  // 获取当前IP地址
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(wifiNetIF, &ip) == ESP_OK)
  {
    wifiIP = ip.ip.addr;
  }

  LOGGER_INFO("WiFi is started.");

  if (!wifi_socket_open(wifiIP))
  {
    wifi_close();
    return false;
  }
  logger::memory("after WiFi/socket start");
  return true;
}
/****************************/

/*****************************
          BLE协议
*****************************/
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"
// IDF的ble_store_config.h未声明init函数,官方示例同样是手动前置声明(注意保持C链接)
extern "C" void ble_store_config_init(void);

static bool bleIsOpen = false;
static bool bleScanning = false;
// 储存池
struct bleReceive
{
  Device *device;
  uint16_t size;
  uint8_t data[BLE_RECEIVE_DATA_MAX];
};
static QueueHandle_t bleReceiveQueue;

static void ble_start_scanning();
static void ble_stop_scanning();
static bool ble_connect_to_device(const std::string &mac);
// 协议栈就绪后需要自动恢复扫描(WiFi模式断线超时后重开BLE时置位)
static bool ble_scan_on_sync = false;
// NimBLE主机与控制器同步完成(ble_on_sync回调置位):
// 同步完成前调用ble_hs_id_infer_auto等主机接口会解引用未初始化状态直接崩溃
static volatile bool bleSynced = false;

// L2CAP 事件
static int ble_l2cap_handler(ble_l2cap_event *event, void *arg)
{
  int rc;
  Device *device = (Device *)arg;

  switch (event->type)
  {
  // 建立连接事件
  case BLE_L2CAP_EVENT_COC_CONNECTED:
  {
    if (event->connect.status == 0)
    {
      // 更新界面和连接信息
      device->setBleChannel(event->connect.chan);
      device->setBleConnected(true);
      ui_bt_update(device->getBleMACString(), true);

      ble_l2cap_chan_info chan_info;
      rc = ble_l2cap_get_chan_info(event->connect.chan, &chan_info);
      if (rc != 0)
      {
        LOGGER_WARN("BLE ble_l2cap_get_chan_info error: %d", rc);
        break;
      }
      LOGGER_INFO("BLE LE COC connected, conn: %d, our_mps: %d, our_mtu: %d, peer_mps: %d, peer_mtu: %d",
                  event->connect.conn_handle,
                  chan_info.our_l2cap_mtu, chan_info.our_coc_mtu,
                  chan_info.peer_l2cap_mtu, chan_info.peer_coc_mtu);
    }
    else
    {
      LOGGER_WARN("BLE LE COC error: %d", event->connect.status);
      break;
    }
    break;
  }

  // 断开连接事件
  case BLE_L2CAP_EVENT_COC_DISCONNECTED:
  {
    device->setBleConnected(false);
    device->setBleChannel(nullptr);
    LOGGER_INFO("BLE LE CoC disconnected, conn: %d", event->disconnect.conn_handle);
    break;
  }

  // 接收连接事件
  case BLE_L2CAP_EVENT_COC_ACCEPT:
  {
    // 接受连接
    os_mbuf *sdu_rx;
    sdu_rx = os_msys_get_pkthdr(BLE_RECEIVE_DATA_MAX, 0);
    if (sdu_rx == NULL)
    {
      LOGGER_WARN("BLE L2CAP accept no memory!");
      break;
    }
    rc = ble_l2cap_recv_ready(event->accept.chan, sdu_rx);
    if (rc != 0)
    {
      LOGGER_WARN("BLE L2CAP accept failed!");
      break;
    }
    LOGGER_INFO("BLE L2CAP accept request.");
    break;
  }

  // 接收到数据事件
  case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
  {
    if (event->receive.sdu_rx != NULL)
    {
      // 放进接收队列
      uint16_t size = event->receive.sdu_rx->om_len;
      bleReceive receive = {.device = device, .size = size};
      if (size > sizeof(receive.data))
      {
        LOGGER_WARN("BLE packet is too large: %u", size);
        os_mbuf_free(event->receive.sdu_rx);
        break;
      }
      memcpy(receive.data, event->receive.sdu_rx->om_data, size);
      if (xQueueSend(bleReceiveQueue, &receive, 0) != pdPASS)
      {
        LOGGER_WARN("BLE receive queue is full.");
      }
      os_mbuf_free(event->receive.sdu_rx);
    }

    // 响应数据 准备接收下一个数据包
    os_mbuf *sdu_rx;
    sdu_rx = os_msys_get_pkthdr(BLE_RECEIVE_DATA_MAX, 0);
    if (sdu_rx == NULL)
    {
      LOGGER_WARN("BLE L2CAP accept no memory!");
      break;
    }
    rc = ble_l2cap_recv_ready(event->receive.chan, sdu_rx);
    if (rc != 0)
    {
      LOGGER_WARN("BLE L2CAP accept failed!");
      break;
    }
    break;
  }

  default:
  {
    break;
  }
  }
  return 0;
}

// GAP 事件
static void ble_mac_reverse(uint8_t dest[6], uint8_t src[6])
{
  // 翻转MAC地址字节序
  for (int i = 0; i < 6; i++)
  {
    dest[i] = src[5 - i];
  }
}
static int ble_gap_handler(ble_gap_event *event, void *arg)
{
  int rc;

  switch (event->type)
  {
  // 发现设备事件
  case BLE_GAP_EVENT_DISC:
  {
    // 解析公告数据
    ble_hs_adv_fields fields;
    rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
    if (rc != 0)
    {
      LOGGER_WARN("BLE ble_hs_adv_parse_fields failed!");
      break;
    }

    // 判断是否需要连接
    if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
        event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND)
    {
      // 判断设备名是否一致
      char name[fields.name_len + 1];
      memcpy(name, fields.name, fields.name_len);
      name[fields.name_len] = '\0';
      if (strcmp(name, BLE_NAME) == 0)
      {
        uint8_t mac[6];
        ble_mac_reverse(mac, event->disc.addr.val);
        Device *device = deviceManager.getDeviceByBleMAC(mac);
        if (device == nullptr)
        {
          device = deviceManager.addDevice(mac);
          if (device != nullptr)
          {
            device->setBleConnected(false);
            device->setWifiConnected(false);
            device->setBleHandle(0);
            device->setBleChannel(nullptr);
            device->setRssi(event->disc.rssi);
            // 添加新设备到界面
            ui_bt_update(device->getBleMACString());
            LOGGER_INFO("BLE find new device: %s.", device->getBleMACString().c_str());
          }
        }
        else
        {
          // 已知设备:刷新信号强度,并确保设备连接页列表中有它的行
          // (断线超时返回设备页后,设备仍在管理器中但列表行需要重建)
          device->setRssi(event->disc.rssi);
          ui_bt_update(device->getBleMACString(), device->isBleConnected());
        }
      }
    }
    break;
  }

  // 设备连接事件
  case BLE_GAP_EVENT_CONNECT:
  {
    if (event->connect.status == 0)
    {
      // 设置蓝牙控制器包长提升性能
      rc = ble_hs_hci_util_set_data_len(event->connect.conn_handle, BLE_PACKET_LENGTH, BLE_PACKET_TIME);
      if (rc != 0)
      {
        LOGGER_WARN("BLE set packet length failed; rc = %d", rc);
      }
      // 协商2M PHY提升空口速率(音频带宽约为2M空口的一半,1M下余量不足)
      rc = ble_gap_set_prefered_le_phy(event->connect.conn_handle,
                                       BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK,
                                       BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK, 0);
      if (rc != 0)
      {
        LOGGER_WARN("BLE failed to set prefered phy; rc = %d", rc);
      }

      ble_gap_conn_desc desc;
      rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
      if (rc != 0)
      {
        LOGGER_WARN("BLE failed to ble_gap_conn_find; rc=%d", rc);
        break;
      }

      uint8_t mac[6];
      ble_mac_reverse(mac, desc.peer_id_addr.val);
      Device *device = deviceManager.getDeviceByBleMAC(mac);
      if (device != nullptr)
      {
        device->setBleHandle(event->connect.conn_handle);

        // 连接到 L2CAP
        os_mbuf *sdu_rx;
        sdu_rx = os_msys_get_pkthdr(BLE_RECEIVE_DATA_MAX, 0);
        if (sdu_rx == NULL)
        {
          LOGGER_WARN("BLE failed to os_msys_get_pkthdr");
          break;
        }
        rc = ble_l2cap_connect(device->getBleHandle(), BLE_L2CAP_PSM, BLE_L2CAP_MTU, sdu_rx, ble_l2cap_handler, device);
        if (rc != 0)
        {
          LOGGER_WARN("BLE failed to ble_l2cap_connect; rc=%d", rc);
          break;
        }

        // 成功建立连接
        LOGGER_INFO("BLE connection established.");
      }
      else
      {
        LOGGER_WARN("BLE device not found for MAC: %2x:%2x:%2x:%2x:%2x:%2x",
                    mac[0], mac[1], mac[2],
                    mac[3], mac[4], mac[5]);
      }
    }
    else
    {
      LOGGER_WARN("BLE Connection failed; status=%d", event->connect.status);
    }
    break;
  }

  // 连接参数更新事件
  case BLE_GAP_EVENT_CONN_UPDATE:
  {
    ble_gap_conn_desc desc;
    rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
    if (rc != 0)
    {
      LOGGER_WARN("BLE connection updated, but ble_gap_conn_find desc failed!");
    }
    LOGGER_INFO("BLE connection updated; status=%d handle=%d conn_itvl=%d conn_latency=%d supervision_timeout=%d encrypted=%d authenticated=%d bonded=%d",
                event->conn_update.status, desc.conn_handle,
                desc.conn_itvl, desc.conn_latency, desc.supervision_timeout,
                desc.sec_state.encrypted, desc.sec_state.authenticated, desc.sec_state.bonded);
    break;
  }

  // PHY更新完成事件
  case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
  {
    LOGGER_INFO("BLE phy updated; status=%d handle=%d tx_phy=%d rx_phy=%d (1=1M 2=2M)",
                event->phy_updated.status, event->phy_updated.conn_handle,
                event->phy_updated.tx_phy, event->phy_updated.rx_phy);
    break;
  }

  default:
  {
    break;
  }
  }
  return 0;
}

// 重置
static void ble_on_reset(int reason)
{
  LOGGER_WARN("BLE reset, reason: %d", reason);
  ble_stop_scanning();
  bleIsOpen = false;
  bleSynced = false;
}

// NimBLE 协议栈加载完成
static void ble_on_sync(void)
{
  // 主机已与控制器同步完成,此后才允许调用地址推断/扫描/连接等主机接口
  bleSynced = true;

  // 默认偏好2M PHY:对端发起PHY更新时优先协商到2M
  int rc = ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK,
                                               BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK);
  if (rc != 0)
  {
    LOGGER_WARN("BLE set default phy failed! rc=%d", rc);
  }

  // 协议栈就绪后恢复扫描(WiFi模式断线超时后会重开BLE并请求扫描)
  if (ble_scan_on_sync)
  {
    ble_scan_on_sync = false;
    ble_start_scanning();
  }
}

void ble_host_task(void *param)
{
  LOGGER_INFO("BLE Host Task Started.");
  /* This function will return only when nimble_port_stop() is executed */
  nimble_port_run();
  nimble_port_freertos_deinit();
}

static bool ble_close()
{
  if (bleIsOpen)
  {
    int rc;
    // 停止扫描
    ble_stop_scanning();

    // 停止NimBLE主机任务
    rc = nimble_port_stop();
    if (rc != 0)
    {
      LOGGER_WARN("BLE NimBLE port stop failed: %d", rc);
      return false;
    }
    // 等待NimBLE停止完成
    rc = nimble_port_deinit();
    if (rc != 0)
    {
      LOGGER_WARN("BLE NimBLE port deinit failed: %d", rc);
      return false;
    }

    bleIsOpen = false;
    bleSynced = false;
  }
  LOGGER_INFO("BLE is close.");
  return true;
}

static bool ble_open()
{
  if (bleIsOpen)
  {
    ble_close();
  }

  // 初始化 NimBLE
  esp_err_t ret = nimble_port_init();
  if (ret != ESP_OK)
  {
    LOGGER_WARN("BLE port init failed: %d", ret);
    return false;
  }

  // 初始化键值存储回调:否则协议栈启动时"Failed to persist local IRK"告警
  // (本机不使用加密/绑定,仅注册RAM存储让IRK写入成功)
  ble_store_config_init();

  // 初始化配置
  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;
  ble_hs_cfg.sm_sc = 0; // 关闭安全连接
  ble_hs_cfg.sm_bonding = 0;

  // 启动 NimBLE 主机任务
  nimble_port_freertos_init(ble_host_task);

  bleIsOpen = true;
  LOGGER_INFO("BLE is started.");
  return true;
}

static void ble_stop_scanning()
{
  if (bleScanning)
  {
    int rc;
    rc = ble_gap_disc_cancel();
    if (rc != 0)
    {
      LOGGER_WARN("BLE cancel discovery failed; rc=%d", rc);
      return;
    }

    bleScanning = false;
  }
}

// 搜索BLE设备
static void ble_start_scanning()
{
  if (!bleIsOpen)
  {
    // NimBLE未初始化(WiFi模式)时不可调用协议栈接口
    return;
  }
  if (!bleSynced)
  {
    // 主机尚未与控制器完成同步(ble_open后约需数十毫秒):
    // 此时调用ble_hs_id_infer_auto会解引用未初始化的主机状态直接崩溃,
    // 挂起请求,由ble_on_sync在同步完成后补启扫描
    ble_scan_on_sync = true;
    return;
  }
  if (!bleScanning)
  {
    int rc;

    uint8_t own_addr_type;
    ble_gap_disc_params disc_params = {0};

    // 获取当前设备地址
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
      LOGGER_WARN("BLE error determining address type; rc=%d", rc);
      return;
    }

    // 扫描参数
    disc_params.filter_duplicates = 1;
    disc_params.passive = 1;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, ble_gap_handler, NULL);
    if (rc != 0)
    {
      LOGGER_WARN("BLE error initiating GAP discovery procedure; rc=%d", rc);
    }

    bleScanning = true;
  }
}

// 连接设备
static bool ble_connect_to_device(const std::string &mac)
{
  if (bleIsOpen)
  {
    if (!bleSynced)
    {
      // 主机尚未同步完成,直接调用会解引用未初始化状态;
      // 返回失败由重连监控稍后自动重试
      return false;
    }
    Device *device = deviceManager.getDeviceByBleMAC(mac);
    if (device != nullptr)
    {
      int rc;

      uint8_t own_addr_type;
      rc = ble_hs_id_infer_auto(0, &own_addr_type);
      if (rc != 0)
      {
        LOGGER_WARN("BLE error determining address type; rc=%d", rc);
        return false;
      }
      if (bleScanning)
      {
        // ble_gap_disc_cancel();
        ble_stop_scanning();
      }
      ble_addr_t peer_addr;
      peer_addr.type = BLE_ADDR_PUBLIC;
      // 翻转MAC地址字节序
      for (int i = 0; i < 6; i++)
      {
        peer_addr.val[i] = device->getBleMAC()[5 - i];
      }
      // 直接以7.5ms连接间隔发起连接:CONNECT_IND由主机指定、从机不可拒绝,
      // 无需依赖连接后再更新参数(默认30~50ms间隔的吞吐只有约50KB/s)
      const struct ble_gap_conn_params conn_params = {
          .scan_itvl = 16,
          .scan_window = 16,
          .itvl_min = BLE_ITVL_MIN,
          .itvl_max = BLE_ITVL_MAX,
          .latency = BLE_LATENCY,
          .supervision_timeout = BLE_SUPERVISION_TIMEOUT,
          .min_ce_len = BLE_CE_LEN_MIN,
          .max_ce_len = BLE_CE_LEN_MAX,
      };
      rc = ble_gap_connect(own_addr_type, &peer_addr, BLE_HS_FOREVER, &conn_params, ble_gap_handler, NULL);
      if (rc != 0)
      {
        LOGGER_WARN("Error: Failed to connect to device, address is %s.", mac.c_str());
        return false;
      }
      // if (bleScanning)
      // {
      //   bleScanning = false;
      //   ble_start_scanning();
      // }

      LOGGER_INFO("BLE connecting to server %s.", mac.c_str());
      return true;
    }
    LOGGER_WARN("BLE connect failed, because not found address %s.", mac.c_str());
    return false;
  }
  LOGGER_WARN("BLE connect failed, because BLE not open.");
  return false;
}

// 断开连接设备
static bool ble_disconnect_to_device(const std::string &mac)
{
  if (bleIsOpen)
  {
    Device *device = deviceManager.getDeviceByBleMAC(mac);
    if (device != nullptr)
    {
      int rc;

      if (device->isBleConnected())
      {
        ble_l2cap_disconnect(device->getBleChannel());
        ble_gap_terminate(device->getBleHandle(), BLE_ERR_REM_USER_CONN_TERM);
      }

      LOGGER_INFO("BLE disconnecting to server %s.", mac.c_str());
      return true;
    }
    LOGGER_WARN("BLE disconnect failed, because not found address %s.", mac.c_str());
    return false;
  }
  LOGGER_WARN("BLE disconnect failed, because BLE not open.");
  return false;
}

// 发送数据
bool ble_send(const Device &device, const uint8_t *data, uint16_t len)
{
  if (!bleIsOpen || !device.getBleChannel())
  {
    LOGGER_WARN("BLE channel not ready.");
    return false;
  }

  if (len > BLE_L2CAP_MTU)
  {
    LOGGER_WARN("BLE data too large for L2CAP MTU.");
    return false;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
  if (!om)
  {
    LOGGER_WARN("BLE failed to allocate mbuf.");
    return false;
  }

  int rc = ble_l2cap_send(device.getBleChannel(), om);
  if (rc == 0 || rc == BLE_HS_ESTALLED)
  {
    // 0=已发出;ESTALLED=栈已接管SDU等待信用自动续发,mbuf归栈所有,均视为成功
    return true;
  }
  if (rc == BLE_HS_EBUSY || rc == BLE_HS_EBADDATA)
  {
    // SDU未被栈接收,缓冲仍归调用方,可释放
    os_mbuf_free_chain(om);
    return false;
  }
  // 其余错误路径中栈已自行释放SDU,不得重复释放
  LOGGER_WARN("BLE failed to send data: %d", rc);
  return false;
}
/****************************/

struct VoiceMeterState
{
  Device *device = nullptr;
  uint64_t peakL = 0;
  uint64_t peakR = 0;
};
static VoiceMeterState voiceMeterStates[RF_MAX_CONNECTION];
static uint32_t voiceMeterLastUpdate = 0;

static VoiceMeterState *rf_get_voice_meter(Device *device)
{
  for (VoiceMeterState &state : voiceMeterStates)
  {
    if (state.device == device)
      return &state;
  }
  for (VoiceMeterState &state : voiceMeterStates)
  {
    if (state.device == nullptr)
    {
      state.device = device;
      return &state;
    }
  }
  return nullptr;
}

struct AudioFragmentValidation
{
  bool valid;
  uint8_t expectedParts;
};

// 按音频缓冲区规则校验分片，并返回当前帧的分片数。
static AudioFragmentValidation rf_validate_audio_fragment(uint16_t packetSize, uint8_t packetPart,
                                                           uint16_t payloadCapacity)
{
  const uint32_t frameSize = audio::buffer::getFrameSize();
  const uint8_t expectedParts = frameSize > 0
                                    ? static_cast<uint8_t>((frameSize + payloadCapacity - 1) / payloadCapacity)
                                    : 0;
  if (frameSize == 0 || expectedParts == 0 || expectedParts > 32 || packetPart >= expectedParts)
  {
    return {false, 0};
  }

  const uint32_t offset = static_cast<uint32_t>(payloadCapacity) * packetPart;
  const uint16_t expectedPartSize = static_cast<uint16_t>(
      (frameSize - offset) > payloadCapacity ? payloadCapacity : (frameSize - offset));
  return {packetSize == expectedPartSize && offset + packetSize <= AUDIO_BUFFER_MAX_DATA_SIZE, expectedParts};
}

// 每设备音频帧丢包统计:按帧序号和分片掩码统计,每秒汇总为百分比供主界面显示
struct AudioLossState
{
  Device *device = nullptr;
  uint32_t currentNumber = 0; // 当前音频帧序号
  uint8_t expectedParts = 0;  // 当前帧应有分片数
  uint32_t receivedParts = 0; // 当前帧已收到分片掩码
  bool hasNumber = false;     // 是否已收到首帧
  uint32_t windowLost = 0;    // 统计窗口内丢失分片数
  uint32_t windowTotal = 0;   // 统计窗口内应收分片数
  uint8_t lossPercent = 0;    // 上一窗口丢包率(0-100)
};
static AudioLossState audioLossStates[RF_MAX_CONNECTION];
// 帧序号空隙超过该值视为码流重启(如断线重连),不计入丢包
static const uint32_t RF_LOSS_RESTART_DELTA = 1000;

static AudioLossState *rf_get_loss_state(Device *device)
{
  for (AudioLossState &state : audioLossStates)
  {
    if (state.device == device)
      return &state;
  }
  for (AudioLossState &state : audioLossStates)
  {
    if (state.device == nullptr)
    {
      state.device = device;
      return &state;
    }
  }
  return nullptr;
}

static void rf_reset_audio_loss(Device &device)
{
  for (AudioLossState &state : audioLossStates)
  {
    if (state.device == &device)
    {
      state.currentNumber = 0;
      state.expectedParts = 0;
      state.receivedParts = 0;
      state.hasNumber = false;
      state.windowLost = 0;
      state.windowTotal = 0;
      state.lossPercent = 0;
      return;
    }
  }
}

static void rf_update_audio_loss(Device *device, uint32_t number, uint8_t part, uint8_t expectedParts)
{
  if (device == nullptr)
  {
    return;
  }
  AudioLossState *state = rf_get_loss_state(device);
  if (state == nullptr)
  {
    return;
  }
  const uint32_t partMask = 1UL << part;
  if (!state->hasNumber)
  {
    state->hasNumber = true;
    state->currentNumber = number;
    state->expectedParts = expectedParts;
    state->receivedParts = partMask;
    state->windowTotal++;
    return;
  }
  const uint32_t delta = number - state->currentNumber; // 无符号减法对序号回绕安全
  if (delta == 0)
  {
    if (state->expectedParts != expectedParts)
    {
      const uint32_t expectedMask = state->expectedParts == 32
                                        ? UINT32_MAX
                                        : ((1UL << state->expectedParts) - 1UL);
      const uint32_t receivedCount =
          static_cast<uint32_t>(__builtin_popcount(state->receivedParts & expectedMask));
      const uint32_t missingParts = state->expectedParts - receivedCount;
      state->windowLost += missingParts;
      state->windowTotal += missingParts;
      state->expectedParts = expectedParts;
      state->receivedParts = 0;
    }
    if ((state->receivedParts & partMask) == 0)
    {
      state->receivedParts |= partMask;
      state->windowTotal++;
    }
    return; // 同帧的其他分片或重复分片
  }
  if (delta > RF_LOSS_RESTART_DELTA)
  {
    if (delta > UINT32_MAX / 2U)
    {
      if (state->currentNumber > RF_LOSS_RESTART_DELTA &&
          number <= RF_LOSS_RESTART_DELTA &&
          state->currentNumber - number > RF_LOSS_RESTART_DELTA)
      {
        rf_reset_audio_loss(*device);
        state->hasNumber = true;
        state->currentNumber = number;
        state->expectedParts = expectedParts;
        state->receivedParts = partMask;
        state->windowTotal++;
      }
      return; // 其他晚到帧不改变统计基线
    }
    state->currentNumber = number;
    state->expectedParts = expectedParts;
    state->receivedParts = partMask;
    state->windowTotal++;
    return;
  }

  const uint32_t expectedMask = state->expectedParts == 32
                                    ? UINT32_MAX
                                    : ((1UL << state->expectedParts) - 1UL);
  const uint32_t receivedCount =
      static_cast<uint32_t>(__builtin_popcount(state->receivedParts & expectedMask));
  const uint32_t missingParts = state->expectedParts - receivedCount;
  const uint32_t missingFrames = delta - 1;
  const uint32_t missingFrameParts = missingFrames * state->expectedParts;
  state->windowTotal += missingParts + missingFrameParts;
  state->windowLost += missingParts + missingFrameParts;
  state->currentNumber = number;
  state->expectedParts = expectedParts;
  state->receivedParts = partMask;
  state->windowTotal++;
}

// 将峰值映射到约-60dBFS至0dBFS的0-100显示范围，仅每40ms计算一次。
static uint8_t rf_peak_to_meter(uint64_t peak, AudioBit bit)
{
  if (peak == 0)
    return 0;
  const uint32_t full_scale_exponent = static_cast<uint32_t>(bit) - 1U;
  const uint32_t peak_exponent = 63U - static_cast<uint32_t>(__builtin_clzll(peak));
  if (peak_exponent >= full_scale_exponent)
    return 100;

  const uint64_t exponent_base = 1ULL << peak_exponent;
  const uint32_t fraction_tenths = static_cast<uint32_t>(
      ((peak - exponent_base) * 10U) / exponent_base);
  const uint32_t attenuation_tenths =
      (full_scale_exponent - peak_exponent) * 10U - fraction_tenths;
  return attenuation_tenths >= 100U ? 0U : static_cast<uint8_t>(100U - attenuation_tenths);
}

static uint8_t rf_apply_meter_envelope(uint8_t current, uint8_t target)
{
  if (target >= current)
    return target;
  const uint8_t released = current > RF_VOICE_METER_RELEASE_STEP
                               ? static_cast<uint8_t>(current - RF_VOICE_METER_RELEASE_STEP)
                               : 0;
  return target > released ? target : released;
}

static void rf_publish_voice_levels(uint32_t now)
{
  if (now - voiceMeterLastUpdate < RF_VOICE_METER_UPDATE_MS)
    return;
  voiceMeterLastUpdate = now;
  for (VoiceMeterState &state : voiceMeterStates)
  {
    if (state.device == nullptr)
      continue;
    const uint8_t targetL = rf_peak_to_meter(state.peakL, config::config.audio.bit);
    const uint8_t targetR = rf_peak_to_meter(state.peakR, config::config.audio.bit);
    state.device->setVoiceLevelL(rf_apply_meter_envelope(state.device->getVoiceLevelL(), targetL));
    state.device->setVoiceLevelR(rf_apply_meter_envelope(state.device->getVoiceLevelR(), targetR));
    state.peakL = 0;
    state.peakR = 0;
  }
}

// 解析每帧首分片中的稀疏PCM样本；热路径只保留整数读取和峰值比较。
static void rf_update_voice_level(Device *device, const uint8_t *data, uint16_t size)
{
  if (device == nullptr || data == nullptr || size == 0)
  {
    return;
  }

  const uint8_t bytesPerSample = (uint8_t)(config::config.audio.bit / 8); // 2/3/4
  const uint8_t channels = (uint8_t)config::config.audio.channel;         // 1/2
  if (bytesPerSample < 2 || bytesPerSample > 4 || channels == 0 || channels > 2)
    return;
  const size_t sampleCount = size / bytesPerSample;

  int64_t peakMagnitudeL = 0;
  int64_t peakMagnitudeR = 0;
  for (size_t i = 0; i + channels <= sampleCount;
       i += channels * RF_VOICE_METER_SAMPLE_STRIDE)
  {
    for (uint8_t ch = 0; ch < channels; ch++)
    {
      const uint8_t *p = data + (i + ch) * bytesPerSample;
      int32_t sample;
      switch (bytesPerSample)
      {
      case 2:
        sample = (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        break;
      case 3:
        sample = (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16);
        if (sample & 0x00800000)
          sample -= 0x1000000; // 24位符号扩展
        break;
      default:
        sample = (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
        break;
      }
      // 用int64取绝对值，避免 INT32_MIN 取负溢出
      int64_t magnitude = sample < 0 ? -(int64_t)sample : (int64_t)sample;

      if (ch == 0)
      {
        if (magnitude > peakMagnitudeL)
          peakMagnitudeL = magnitude;
      }
      else if (magnitude > peakMagnitudeR)
      {
        peakMagnitudeR = magnitude;
      }
    }
  }

  VoiceMeterState *state = rf_get_voice_meter(device);
  if (state == nullptr)
    return;
  if (static_cast<uint64_t>(peakMagnitudeL) > state->peakL)
    state->peakL = static_cast<uint64_t>(peakMagnitudeL);
  const uint64_t peakR = channels == 1 ? static_cast<uint64_t>(peakMagnitudeL)
                                       : static_cast<uint64_t>(peakMagnitudeR);
  if (peakR > state->peakR)
    state->peakR = peakR;
}

// 解析数据包
static bool rf_receive_packet(Device *device, const uint8_t *data, size_t len)
{
  if (data == nullptr || len < sizeof(PacketType))
  {
    return false;
  }
  Packet *packet = (Packet *)data;
  switch (packet->type)
  {
  // WIFI音频数据包
  case PACKET_TYPE_WIFI_AUDIO:
  {
    if (len < PACKET_WIFI_AUDIO_HEAD_SIZE ||
        packet->packet.audioDataWiFi.size > PACKET_WIFI_AUDIO_DATA_MAX_SIZE ||
        len < PACKET_WIFI_AUDIO_HEAD_SIZE + packet->packet.audioDataWiFi.size)
    {
      return false;
    }
    const AudioFragmentValidation validation = rf_validate_audio_fragment(
        packet->packet.audioDataWiFi.size, packet->packet.audioDataWiFi.part, PACKET_WIFI_AUDIO_DATA_MAX_SIZE);
    if (!validation.valid)
    {
      return false;
    }
    audio::buffer::writeWiFiPacket(&packet->packet.audioDataWiFi);
    // 统计音频帧丢包率
    rf_update_audio_loss(device, packet->packet.audioDataWiFi.number, packet->packet.audioDataWiFi.part,
                         validation.expectedParts);
    // 更新设备实时音频电平
    if (device && packet->packet.audioDataWiFi.part == 0)
    {
      rf_update_voice_level(device, packet->packet.audioDataWiFi.data, packet->packet.audioDataWiFi.size);
    }
    break;
  }

  // BLE音频数据包
  case PACKET_TYPE_BLE_AUDIO:
  {
    if (len < PACKET_BLE_AUDIO_HEAD_SIZE ||
        packet->packet.audioDataBLE.size > PACKET_BLE_AUDIO_DATA_MAX_SIZE ||
        len < PACKET_BLE_AUDIO_HEAD_SIZE + packet->packet.audioDataBLE.size)
    {
      return false;
    }
    const AudioFragmentValidation validation = rf_validate_audio_fragment(
        packet->packet.audioDataBLE.size, packet->packet.audioDataBLE.part, PACKET_BLE_AUDIO_DATA_MAX_SIZE);
    if (!validation.valid)
    {
      return false;
    }
    audio::buffer::writeBLEPacket(&packet->packet.audioDataBLE);
    // 统计音频帧丢包率
    rf_update_audio_loss(device, packet->packet.audioDataBLE.number, packet->packet.audioDataBLE.part,
                         validation.expectedParts);
    // 更新设备实时音频电平
    if (device && packet->packet.audioDataBLE.part == 0)
    {
      rf_update_voice_level(device, packet->packet.audioDataBLE.data, packet->packet.audioDataBLE.size);
    }
    break;
  }

  // 客户端响应ACK
  case PACKET_TYPE_CLIENT_ACK:
  {
    break;
  }

  // 客户端状态
  case PACKET_TYPE_CLIENT_STATUS:
  {
    // packet->packet.clientStatus.status
    if (len != PACKET_CLIENT_STATUS_SIZE || device == nullptr)
    {
      return false;
    }
    device->setBattery(packet->packet.clientStatus.battery);
    // device->print();
    return true;
  }

  default:
  {
    LOGGER_WARN("RF unknow packet type=%d", packet->type);
    break;
  }
  }
  return true;
}

static unsigned long secondLastTime = millis(); // 每秒时间点

/*****************************
        断线重连监控
*****************************/
// 每个设备的重连监控状态(按BLE MAC区分,设备总数受RF_MAX_CONNECTION限制)
struct ReconnectState
{
  std::string mac;
  bool awaiting = false;    // 已断开,宽限期内等待重连
  uint32_t sinceMs = 0;     // 断开时刻
  uint32_t lastRetryMs = 0; // 上次主动重连尝试时刻
};
static ReconnectState reconnectStates[RF_MAX_CONNECTION];

static ReconnectState *reconnect_state_find(const std::string &mac)
{
  for (uint8_t i = 0; i < RF_MAX_CONNECTION; i++)
  {
    if (reconnectStates[i].mac == mac)
      return &reconnectStates[i];
  }
  for (uint8_t i = 0; i < RF_MAX_CONNECTION; i++)
  {
    if (reconnectStates[i].mac.empty())
    {
      reconnectStates[i].mac = mac;
      return &reconnectStates[i];
    }
  }
  return nullptr;
}

// 重连成功后重新下发音频启动命令(幂等),确保发射端恢复推流
static void rf_resume_audio(Device &device)
{
  rf_reset_audio_loss(device);
  if (device.isBleConnected())
  {
    Packet *packet = (Packet *)malloc(PACKET_SERVER_CONTROL_AUDIO_SIZE);
    if (packet != nullptr)
    {
      packet->type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
      packet->packet.serverControlAudio.start = true;
      packet->packet.serverControlAudio.channel = config::config.audio.channel;
      packet->packet.serverControlAudio.rate = config::config.audio.rate;
      packet->packet.serverControlAudio.bit = config::config.audio.bit;
      packet->packet.serverControlAudio.mode = config::config.audio.mode;
      packet->packet.serverControlAudio.gain = config::config.audio.gain;
      ble_send(device, (uint8_t *)packet, PACKET_SERVER_CONTROL_AUDIO_SIZE);
      free(packet);
    }
  }
  else if (device.isWifiConnected())
  {
    // 裸IP传输不可靠,连发多次确保送达(与初始启动流程一致)
    for (uint8_t retry = 0; retry < 3; retry++)
    {
      netbuf *buf = netbuf_new();
      if (buf == NULL)
        break;
      Packet *packet = (Packet *)netbuf_alloc(buf, PACKET_SERVER_CONTROL_AUDIO_SIZE);
      if (packet == NULL)
      {
        netbuf_delete(buf);
        continue;
      }
      packet->type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
      packet->packet.serverControlAudio.start = true;
      packet->packet.serverControlAudio.channel = config::config.audio.channel;
      packet->packet.serverControlAudio.rate = config::config.audio.rate;
      packet->packet.serverControlAudio.bit = config::config.audio.bit;
      packet->packet.serverControlAudio.mode = config::config.audio.mode;
      packet->packet.serverControlAudio.gain = config::config.audio.gain;
      if (!wifi_send(device, buf))
        break;
    }
  }
}

// 每秒检查各设备连接状态:断开时界面提示重连中,BLE模式主动重试,超时判定失败
static void rf_reconnect_monitor()
{
  unsigned long now = millis();
  Device *devices = deviceManager.getAllDevices();

  // 先统计在线情况(用于超时后是否恢复BLE配对)
  bool anyConnected = false;
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    if (devices[i].isBleConnected() || devices[i].isWifiConnected())
    {
      anyConnected = true;
      break;
    }
  }

  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    Device &device = devices[i];
    bool connected = device.isBleConnected() || device.isWifiConnected();
    // 只监控主界面卡片上的设备,未配对/已在设备页手动断开的设备不参与
    const std::string mac = device.getBleMACString();
    if (!ui_main_has_device(mac))
      continue;
    ReconnectState *state = reconnect_state_find(mac);
    if (state == nullptr)
      continue;

    if (connected)
    {
      if (state->awaiting)
      {
        // 重连成功:撤下提示并让发射端恢复推流
        state->awaiting = false;
        ui_main_set_reconnect(mac, false);
        rf_resume_audio(device);
        LOGGER_INFO("Device %s reconnected.", mac.c_str());
      }
    }
    else if (!state->awaiting)
    {
      // 刚断开:进入宽限期,界面提示重连中
      state->awaiting = true;
      state->sinceMs = now;
      state->lastRetryMs = now;
      ui_main_set_reconnect(mac, true);
      LOGGER_WARN("Device %s disconnected, waiting for reconnect.", mac.c_str());
    }
    else if (now - state->sinceMs >= RF_RECONNECT_TIMEOUT_MS)
    {
      // 宽限期用尽:判定连接失败,移除卡片(可能自动返回设备连接页)
      state->awaiting = false;
      LOGGER_WARN("Device %s reconnect failed after %d ms.", mac.c_str(), (int)RF_RECONNECT_TIMEOUT_MS);
      if (bleIsOpen)
      {
        // 清掉挂起的GAP过程(连接请求FOREVER不会自行超时),否则设备页无法恢复扫描
        if (bleScanning)
        {
          ble_stop_scanning();
        }
        else
        {
          ble_gap_conn_cancel(); // 取消仍在进行中的重连连接请求
        }
      }
      if (ui_main_notify_link_lost(mac))
      {
        // 已无任何在线设备且WiFi模式在运行:关闭WiFi、恢复BLE扫描,设备连接页才能重新发现发射器
        if (!anyConnected && wifiIsOpen && !bleIsOpen)
        {
          wifi_close();
          ble_scan_on_sync = true;
          ble_open();
          ble_start_scanning(); // 协议栈未就绪时由ble_on_sync补启
        }
      }
    }
    else if (bleIsOpen && !wifiIsOpen && now - state->lastRetryMs >= RF_RECONNECT_RETRY_INTERVAL_MS)
    {
      // BLE模式:接收器为主动机,周期性主动重连
      state->lastRetryMs = now;
      ble_connect_to_device(mac);
    }
  }
}

// 将BLE连接的发射器迁移到WiFi并恢复音频(设备页完成流程与设置页协议切换共用)
// BLE与WiFi绝不同时运行:先经BLE下发命令,彻底关闭BLE后再启动WiFi AP,避免IRAM被同时占满
static bool rf_migrate_devices_to_wifi()
{
  Device *devices = deviceManager.getAllDevices();

  // 第一阶段(纯BLE):向所有已连接发射器下发WiFi接入命令
  bool commanded[RF_MAX_CONNECTION] = {};
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    Device &device = devices[i];
    if (!device.isBleConnected())
    {
      continue;
    }

    // 让设备以 WiFi 模式接入
    Packet *packet = (Packet *)malloc(PACKET_SERVER_CONTROL_RF_SIZE);
    if (packet != nullptr)
    {
      packet->type = PACKET_TYPE_SERVER_CONTROL_RF;
      packet->packet.serverControlRF.mode = RF_MODE_WIFI;
      // TODO: WiFi 名字和密码随机加密
      snprintf(packet->packet.serverControlRF.ssid,
               sizeof(packet->packet.serverControlRF.ssid), "%s", WIFI_NAME);
      snprintf(packet->packet.serverControlRF.password,
               sizeof(packet->packet.serverControlRF.password), "%s", WIFI_PASSWORD);
      if (ble_send(device, (uint8_t *)packet, PACKET_SERVER_CONTROL_RF_SIZE))
      {
        commanded[i] = true;
      }
      free(packet);
    }
  }

  // 等待命令经BLE链路层发送完成,再关闭BLE协议栈
  delay(RF_BLE_SHUTDOWN_FLUSH_MS);

  // 第二阶段(关闭BLE):此时WiFi尚未启动,两套协议栈绝不共存
  if (!ble_close())
  {
    LOGGER_ERROR("BLE close failed before starting WiFi, migration aborted.");
    return false;
  }
  // BLE链路已断,不再有断开事件到达,显式清除连接状态
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    devices[i].setBleConnected(false);
    devices[i].setBleChannel(nullptr);
  }

  // 第三阶段(纯WiFi):启动AP,发射器收到命令后同样会先关闭自身BLE再来连接
  if (!wifi_open())
  {
    // AP启动失败:回退到BLE扫描,发射器连不上WiFi会自动回退到BLE广播,重新配对即可恢复
    LOGGER_ERROR("WiFi open failed after BLE closed, rolling back to BLE.");
    ble_scan_on_sync = true;
    ble_open();
    ble_start_scanning(); // 协议栈未就绪时由ble_on_sync补启
    return false;
  }

  // 第四阶段(纯WiFi):等待已通知的设备接入并发送音频启动命令
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    if (!commanded[i])
    {
      continue;
    }
    Device &device = devices[i];

    // 等待设备通过 WiFi 连接成功(限时,避免调用线程无限阻塞)
    uint32_t waitStart = millis();
    while (!device.isWifiConnected() && millis() - waitStart < RF_MODE_SWITCH_TIMEOUT_MS)
    {
      delay(100);
    }
    if (!device.isWifiConnected())
    {
      LOGGER_WARN("Device %s did not join WiFi in %d ms.",
                  device.getBleMACString().c_str(), (int)RF_MODE_SWITCH_TIMEOUT_MS);
      continue;
    }

    // 开启设备音频传输
    // 裸IP传输不可靠且对端socket可能尚未就绪，重发多次确保送达(该包幂等)
    for (uint8_t retry = 0; retry < 3; retry++)
    {
      netbuf *buf = netbuf_new();
      if (buf != NULL)
      {
        Packet *audio = (Packet *)netbuf_alloc(buf, PACKET_SERVER_CONTROL_AUDIO_SIZE);
        if (audio != NULL)
        {
          audio->type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
          audio->packet.serverControlAudio.start = true;
          audio->packet.serverControlAudio.channel = config::config.audio.channel;
          audio->packet.serverControlAudio.rate = config::config.audio.rate;
          audio->packet.serverControlAudio.bit = config::config.audio.bit;
          audio->packet.serverControlAudio.mode = config::config.audio.mode;
          audio->packet.serverControlAudio.gain = config::config.audio.gain;
          wifi_send(device, buf);
        }
        else
        {
          netbuf_delete(buf);
        }
      }
      delay(50);
    }
  }
  return true;
}

// 将WiFi连接的发射器迁回BLE:重开扫描,由重连监控自动回连并恢复音频
static bool rf_migrate_devices_to_ble()
{
  Device *devices = deviceManager.getAllDevices();

  // 通过WiFi通知发射器切回BLE(发射器会停止WiFi重新广播)
  // 裸IP传输不可靠,重发多次确保送达
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    Device &device = devices[i];
    if (!device.isWifiConnected())
    {
      continue;
    }
    for (uint8_t retry = 0; retry < 3; retry++)
    {
      // 发射器收到命令后会立即断开WiFi切回BLE,此时离线属预期,静默结束重发
      if (!device.isWifiConnected())
      {
        break;
      }
      netbuf *buf = netbuf_new();
      if (buf == NULL)
      {
        break;
      }
      Packet *packet = (Packet *)netbuf_alloc(buf, PACKET_SERVER_CONTROL_RF_SIZE);
      if (packet == NULL)
      {
        netbuf_delete(buf);
        continue;
      }
      packet->type = PACKET_TYPE_SERVER_CONTROL_RF;
      packet->packet.serverControlRF.mode = RF_MODE_BLE;
      snprintf(packet->packet.serverControlRF.ssid,
               sizeof(packet->packet.serverControlRF.ssid), "%s", WIFI_NAME);
      snprintf(packet->packet.serverControlRF.password,
               sizeof(packet->packet.serverControlRF.password), "%s", WIFI_PASSWORD);
      if (!wifi_send(device, buf))
      {
        break;
      }
      delay(50);
    }
  }

  // 关闭WiFi并重开BLE扫描;停止AP不一定逐站触发断开事件,这里显式清位
  wifi_close();
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    devices[i].setWifiConnected(false);
  }
  if (!bleIsOpen)
  {
    // 协议栈未就绪时由ble_on_sync补启扫描
    ble_scan_on_sync = true;
    if (!ble_open())
    {
      LOGGER_ERROR("BLE reopen failed after WiFi closed.");
      return false;
    }
  }
  ble_start_scanning();
  return true;
}

/*****************************
        协议切换任务
*****************************/
// BLE/WiFi迁移全程可达数十秒,绝不能在LVGL任务的事件回调里同步执行:
// 1.界面会冻结; 2.LVGL任务持有LVGL锁期间等待NimBLE停止,而NimBLE主机任务的
// 回调(ui_bt_update)又在等LVGL锁,形成死锁导致接收器卡死。
// 因此迁移放在独立任务中执行,完成后持锁操作界面。
static bool rfSwitchBusy = false;
static bool rfSwitchEnterMain = false;

static void rf_switch_task(void *arg)
{
  const RFMode target = static_cast<RFMode>(reinterpret_cast<uintptr_t>(arg));
  const bool enterMain = rfSwitchEnterMain;
  LOGGER_INFO("RF protocol switch task start, target=%u.", static_cast<unsigned int>(target));

  bool ok = false;
  if (target == RF_MODE_BLE)
  {
    // BLE带宽仅支持48000Hz/16bit/单声道,切到BLE前先收敛已保存的音频格式
    if (config::config.audio.channel != AUDIO_CHANNEL_SINGLE ||
        config::config.audio.rate != AUDIO_RATE_48000 ||
        config::config.audio.bit != AUDIO_BIT_16)
    {
      LOGGER_INFO("BLE mode fixes audio format to 48000Hz/16bit/mono.");
      config::config.audio.channel = AUDIO_CHANNEL_SINGLE;
      config::config.audio.rate = AUDIO_RATE_48000;
      config::config.audio.bit = AUDIO_BIT_16;
    }
    ok = rf_migrate_devices_to_ble();
  }
  else if (target == RF_MODE_WIFI)
  {
    ok = rf_migrate_devices_to_wifi();
  }

  if (ok)
  {
    config::config.rf.mode = target;
    config::save();
  }

  LV_LOCK();
  // 关闭"正在连接/切换"提示弹窗
  ui_close_popup();
  if (ok)
  {
    if (enterMain)
    {
      // 设备页完成流程:连接完成后立即进入主界面
      ui_bt_finish_to_main();
    }
  }
  else
  {
    // 切换失败:射频已回退到BLE并重新扫描,设备页流程留在设备页等待重新选择
    ui_popwin_msgbox(enterMain ? "连接失败,请重试" : "切换失败,已恢复BLE", nullptr, nullptr);
  }
  LV_UNLOCK();

  rfSwitchBusy = false;
  LOGGER_INFO("RF protocol switch task done, success=%d.", ok ? 1 : 0);
  vTaskDelete(nullptr);
}

// 请求在独立任务中切换协议;返回false表示已有切换在进行或任务创建失败
static bool rf_request_protocol_switch(RFMode target, bool enter_main_on_success)
{
  if (rfSwitchBusy)
  {
    return false;
  }
  rfSwitchBusy = true;
  rfSwitchEnterMain = enter_main_on_success;
  if (xTaskCreatePinnedToCore(rf_switch_task, "rf_switch", TASK_RF_SWITCH_STACK,
                              reinterpret_cast<void *>(static_cast<uintptr_t>(target)),
                              TASK_RF_SWITCH_PRIORITY, nullptr, TASK_RF_SWITCH_CORE) != pdPASS)
  {
    rfSwitchBusy = false;
    LOGGER_ERROR("RF switch task creation failed.");
    return false;
  }
  return true;
}

static void rf_handle(void *arg)
{
  // 缓存
  Device *device = nullptr;
  netbuf *receiveBuffer = NULL;
  // rssi
  wifi_sta_list_t staList;
#ifdef BUILD_DEBUG
  uint32_t wifiRxPackets = 0;
  uint32_t wifiRxMaxBurst = 0;
  uint32_t wifiRxMailboxPeak = 0;
  uint32_t wifiRxBudgetHits = 0;
  uint32_t wifiRxMaxDrainUs = 0;
  uint32_t bleRxPackets = 0;
  uint32_t bleRxBytes = 0;
#endif

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_RF_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    /* 处理 BLE 模块 */
    if (bleIsOpen)
    {
      /* 发送 */
      //////////

      /* 接收 */
      bleReceive receive;
      while (xQueueReceive(bleReceiveQueue, &receive, 0) == pdPASS)
      {
        rf_receive_packet(receive.device, receive.data, receive.size);
        transmitSpeedData += receive.size;
#ifdef BUILD_DEBUG
        bleRxPackets++;
        bleRxBytes += receive.size;
#endif
      }
    }

    /* 处理 WIFI 模块 */
    if (wifiIsOpen && socketIsOpen)
    {
      /* 发送 */
      //////////

      /* 接收 */
      uint32_t wifiRxBurst = 0;
#ifdef BUILD_DEBUG
      if (socketReceiveInstance != nullptr && sys_mbox_valid(&socketReceiveInstance->recvmbox))
      {
        const uint32_t waiting = uxQueueMessagesWaiting(socketReceiveInstance->recvmbox->os_mbox);
        if (waiting > wifiRxMailboxPeak)
        {
          wifiRxMailboxPeak = waiting;
        }
      }
      const uint32_t wifiRxDrainStartUs = micros();
#endif
      while (wifiRxBurst < RF_WIFI_RX_BURST_MAX && wifi_receive(&device, &receiveBuffer))
      {
        uint8_t *data;
        uint16_t len;
        do
        {
            netbuf_data(receiveBuffer, (void **)&data, &len);
            if (data == nullptr || len < WIFI_IP_HEAD_LEN)
            {
              break;
            }
            data += WIFI_IP_HEAD_LEN;
            len -= WIFI_IP_HEAD_LEN;
            // 解析数据包
            rf_receive_packet(device, data, len);
          transmitSpeedData += len;
        } while (netbuf_next(receiveBuffer) >= 0);
        netbuf_delete(receiveBuffer);
        receiveBuffer = NULL;
        device = nullptr;
        wifiRxBurst++;
      }
#ifdef BUILD_DEBUG
      const uint32_t wifiRxDrainUs = micros() - wifiRxDrainStartUs;
      wifiRxPackets += wifiRxBurst;
      if (wifiRxBurst > wifiRxMaxBurst)
      {
        wifiRxMaxBurst = wifiRxBurst;
      }
      wifiRxBudgetHits += wifiRxBurst == RF_WIFI_RX_BURST_MAX ? 1U : 0U;
      if (wifiRxBurst > 0 && wifiRxDrainUs > wifiRxMaxDrainUs)
      {
        wifiRxMaxDrainUs = wifiRxDrainUs;
      }
#endif
    }

    rf_publish_voice_levels(millis());

    /* 每秒执行一次获取其他信息 */
    unsigned long secondNowTime = millis();
    if (secondNowTime - secondLastTime > 1000)
    {
      /* 计算速度 */
      transmitSpeed = transmitSpeedData;
      transmitSpeedData = 0;
      /* 汇总每设备丢包率 */
      for (AudioLossState &state : audioLossStates)
      {
        if (state.device == nullptr)
        {
          continue;
        }
        uint32_t lossPercent = state.windowTotal > 0
                                   ? (state.windowLost * 100U + state.windowTotal - 1U) / state.windowTotal
                                   : (state.hasNumber &&
                                              (state.device->isBleConnected() || state.device->isWifiConnected())
                                          ? 100U
                                          : 0U);
        if (lossPercent > 100)
        {
          lossPercent = 100;
        }
        state.lossPercent = static_cast<uint8_t>(lossPercent);
        state.windowLost = 0;
        state.windowTotal = 0;
      }
      secondLastTime = secondNowTime;
      // LOGGER_INFO("RF data speed %dKB/s", transmitSpeed / 1024);
#ifdef BUILD_DEBUG
      uint8_t levelL = 0;
      uint8_t levelR = 0;
      if (deviceManager.size() > 0)
      {
        levelL = deviceManager.getAllDevices()[0].getVoiceLevelL();
        levelR = deviceManager.getAllDevices()[0].getVoiceLevelR();
      }
      RfDebugSnapshot snapshot = {};
      snapshot.ready = true;
      snapshot.packets = wifiRxPackets;
      snapshot.bytes = transmitSpeed;
      snapshot.max_burst = wifiRxMaxBurst;
      snapshot.mailbox_peak = wifiRxMailboxPeak;
      snapshot.budget_hits = wifiRxBudgetHits;
      snapshot.max_drain_us = wifiRxMaxDrainUs;
      snapshot.ble_packets = bleRxPackets;
      snapshot.ble_bytes = bleRxBytes;
      snapshot.level_l = levelL;
      snapshot.level_r = levelR;
      portENTER_CRITICAL(&rf_debug_mux);
      rf_debug_snapshot = snapshot;
      portEXIT_CRITICAL(&rf_debug_mux);
      wifiRxPackets = 0;
      wifiRxMaxBurst = 0;
      wifiRxMailboxPeak = 0;
      wifiRxBudgetHits = 0;
      wifiRxMaxDrainUs = 0;
      bleRxPackets = 0;
      bleRxBytes = 0;
#endif

      /* 获取信号强度 */
      if (bleIsOpen)
      {
        // BLE模式下已连接设备读取实时RSSI(阻塞式HCI读,每秒一次开销可忽略)
        Device *allDevices = deviceManager.getAllDevices();
        for (uint8_t i = 0; i < deviceManager.size(); i++)
        {
          if (allDevices[i].isBleConnected())
          {
            int8_t rssi = 0;
            if (ble_gap_conn_rssi(allDevices[i].getBleHandle(), &rssi) == 0)
            {
              allDevices[i].setRssi(rssi);
            }
          }
        }
      }
      else if (wifiIsOpen && socketIsOpen)
      {
        esp_wifi_ap_get_sta_list(&staList);
        for (int i = 0; i < staList.num; i++)
        {
          wifi_sta_info_t station = staList.sta[i];
          Device *device = deviceManager.getDeviceByWifiMAC(station.mac);
          if (device != nullptr)
          {
            device->setRssi(station.rssi);
            // device->print();
          }
        }
      }

      /* 断线重连监控 */
      rf_reconnect_monitor();
    }
  }
}

void rf::setup()
{
  // BLE带宽仅支持48000Hz/16bit/单声道:历史版本可能保存了更高格式(如在WIFI模式设置192kHz后切到BLE),
  // 若不收敛,本机解码帧长与发射端实际格式不匹配会导致整流丢弃,且旧配置会被原样下发给发射端
  if (config::config.rf.mode == RF_MODE_BLE &&
      (config::config.audio.channel != AUDIO_CHANNEL_SINGLE ||
       config::config.audio.rate != AUDIO_RATE_48000 ||
       config::config.audio.bit != AUDIO_BIT_16))
  {
    LOGGER_INFO("BLE mode fixes stored audio format to 48000Hz/16bit/mono.");
    config::config.audio.channel = AUDIO_CHANNEL_SINGLE;
    config::config.audio.rate = AUDIO_RATE_48000;
    config::config.audio.bit = AUDIO_BIT_16;
    config::save();
  }
  wifiSocketMutex = xSemaphoreCreateMutex();
  if (wifiSocketMutex == nullptr)
  {
    LOGGER_ERROR("WiFi socket mutex creation failed.");
    return;
  }
  bleReceiveQueue = xQueueCreate(16, sizeof(bleReceive));
  if (bleReceiveQueue == nullptr)
  {
    LOGGER_ERROR("BLE receive queue creation failed.");
    return;
  }
  ble_open();
  // 启动发送接收线程
  xTaskCreatePinnedToCore(rf_handle, "rf_handle", TASK_RF_STACK, NULL, TASK_RF_PRIORITY, NULL, TASK_RF_CORE);
#ifdef BUILD_DEBUG
  if (xTaskCreatePinnedToCoreWithCaps(rfDebugHandle, "rf_debug", 2560, nullptr, 1, nullptr,
                                      1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
  {
    LOGGER_INFO("RF debug task creation failed.");
  }
#endif
}

/*****************************
            界面
*****************************/
// 连接蓝牙设备
bool ui_bt_link(const std::string &mac)
{
  if (ble_connect_to_device(mac))
  {
    return true; // 返回连接状态
  }
  return false;
}

// 断开蓝牙设备
bool ui_bt_unlink(const std::string &mac)
{
  if (ble_disconnect_to_device(mac))
  {
    return true; // 返回连接状态
  }
  return false;
}

// 需要update已连接和未连接的蓝牙
void ui_bt_search()
{
  ble_start_scanning();
}
void ui_bt_pause_search()
{
  Device *devices = deviceManager.getAllDevices();
  switch (config::config.rf.mode)
  {
  case RF_MODE_BLE:
  {
    LOGGER_INFO("RF start BLE audio transmission");
    for (uint8_t i = 0; i < deviceManager.size(); i++)
    {
      Device &device = devices[i];
      if (device.isBleConnected())
      {
        // 开启设备的音频传输
        Packet *packet = (Packet *)malloc(PACKET_SERVER_CONTROL_AUDIO_SIZE);
        packet->type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
        packet->packet.serverControlAudio.start = true;
        packet->packet.serverControlAudio.channel = config::config.audio.channel;
        packet->packet.serverControlAudio.rate = config::config.audio.rate;
        packet->packet.serverControlAudio.bit = config::config.audio.bit;
        packet->packet.serverControlAudio.mode = config::config.audio.mode;
        packet->packet.serverControlAudio.gain = config::config.audio.gain;
        ble_send(device, (uint8_t *)packet, PACKET_SERVER_CONTROL_AUDIO_SIZE);
        free(packet);
      }
    }
    break;
  }
  case RF_MODE_WIFI:
  {
    LOGGER_INFO("RF start WiFi audio transmission");
    // 迁移交由协议切换任务异步执行,完成后自动进入主界面
    // (失败时回退BLE重新扫描,提示用户重试)
    if (!rf_request_protocol_switch(RF_MODE_WIFI, true))
    {
      LOGGER_WARN("WiFi migration request rejected, another switch is running.");
    }
    break;
  }
  }
}

std::vector<std::string> ui_bt_get_linked()
{
  std::vector<std::string> strs;
  uint8_t size = deviceManager.size();
  if (size != 0)
  {
    Device *devices = deviceManager.getAllDevices();
    for (uint8_t i = 0; i < size; i++)
      {
        if (devices[i].isBleConnected() || devices[i].isWifiConnected())
        {
          strs.push_back(devices[i].getBleMACString());
        }
    }
  }
  return strs;
}

void ui_bt_seed_devices()
{
  Device *devices = deviceManager.getAllDevices();
  const uint8_t size = deviceManager.size();
  for (uint8_t i = 0; i < size; i++)
  {
    ui_bt_update(devices[i].getBleMACString(), devices[i].isBleConnected() || devices[i].isWifiConnected());
  }
}

int8_t ui_info_get_left_voice(const std::string &mac)
{
  Device *device = deviceManager.getDeviceByBleMAC(mac);
  return (device != nullptr) ? (int8_t)device->getVoiceLevelL() : 0;
}

int8_t ui_info_get_right_voice(const std::string &mac)
{
  Device *device = deviceManager.getDeviceByBleMAC(mac);
  return (device != nullptr) ? (int8_t)device->getVoiceLevelR() : 0;
}

uint8_t ui_info_get_loss(const std::string &mac)
{
  Device *device = deviceManager.getDeviceByBleMAC(mac);
  if (device != nullptr)
  {
    for (AudioLossState &state : audioLossStates)
    {
      if (state.device == device)
      {
        return state.lossPercent;
      }
    }
  }
  return 0;
}

int8_t ui_info_get_power(const std::string &mac)
{
  Device *device = deviceManager.getDeviceByBleMAC(mac);
  if (device != nullptr)
  {
    return device->getBattery();
  }
  LOGGER_WARN("Screen get wrong MAC by %s", mac.c_str());
  return 100;
}

int8_t ui_info_get_signal(const std::string &mac)
{
  Device *device = deviceManager.getDeviceByBleMAC(mac);
  if (device != nullptr)
  {
    return device->getRssi();
  }
  LOGGER_WARN("Screen get wrong MAC by %s", mac.c_str());
  return 100;
}

void ui_setting_audio_input_page_rcb(AudioBit &bit, AudioChannel &channel, AudioRate &rate, AudioGain &gain, AudioMode &mode)
{
  bit = config::config.audio.bit;
  channel = config::config.audio.channel;
  rate = config::config.audio.rate;
  gain = config::config.audio.gain;
  mode = config::config.audio.mode;
}

void ui_setting_audio_input_page_scb(AudioBit bit, AudioChannel channel, AudioRate rate, AudioGain gain, AudioMode mode)
{
  LOGGER_INFO("ui_setting_audio_input_page_scb");
  // BLE带宽仅支持48000Hz/16bit/单声道,收到更高格式时收敛(未来支持立体声)
  if (config::config.rf.mode == RF_MODE_BLE &&
      (channel != AUDIO_CHANNEL_SINGLE || rate != AUDIO_RATE_48000 || bit != AUDIO_BIT_16))
  {
    LOGGER_INFO("BLE mode fixes audio format to 48000Hz/16bit/mono.");
    channel = AUDIO_CHANNEL_SINGLE;
    rate = AUDIO_RATE_48000;
    bit = AUDIO_BIT_16;
  }
  config::config.audio.bit = (AudioBit)bit;
  config::config.audio.channel = (AudioChannel)channel;
  config::config.audio.rate = (AudioRate)rate;
  config::config.audio.gain = (AudioGain)gain;
  config::config.audio.mode = (AudioMode)mode;
  config::save();

  // 立即将新音频配置发送到所有已连接设备
  Device *devices = deviceManager.getAllDevices();
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    Device &device = devices[i];
    if (device.isBleConnected())
    {
      Packet *packet = (Packet *)malloc(PACKET_SERVER_CONTROL_AUDIO_SIZE);
      if (packet)
      {
        packet->type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
        packet->packet.serverControlAudio.start = true;
        packet->packet.serverControlAudio.channel = channel;
        packet->packet.serverControlAudio.rate = rate;
        packet->packet.serverControlAudio.bit = bit;
        packet->packet.serverControlAudio.mode = mode;
        packet->packet.serverControlAudio.gain = gain;
        ble_send(device, (uint8_t *)packet, PACKET_SERVER_CONTROL_AUDIO_SIZE);
        free(packet);
      }
    }
    if (device.isWifiConnected())
    {
      netbuf *buf = netbuf_new();
      if (buf != NULL)
      {
        Packet *packet = (Packet *)netbuf_alloc(buf, PACKET_SERVER_CONTROL_AUDIO_SIZE);
        if (packet)
        {
          packet->type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
          packet->packet.serverControlAudio.start = true;
          packet->packet.serverControlAudio.channel = channel;
          packet->packet.serverControlAudio.rate = rate;
          packet->packet.serverControlAudio.bit = bit;
          packet->packet.serverControlAudio.mode = mode;
          packet->packet.serverControlAudio.gain = gain;
          wifi_send(device, buf);
        }
      }
    }
  }
}

void ui_setting_rf_page_rcb(RFMode &mode)
{
  mode = config::config.rf.mode;
}

void ui_setting_rf_page_scb(RFMode mode)
{
  LOGGER_INFO("ui_setting_rf_page_scb");
  if (config::config.rf.mode == mode)
  {
    config::save();
    return;
  }

  // 已连接设备迁移到新协议;未连接设备只保存配置,
  // 保留当前协议栈供设备页继续配对,完成时再按新模式迁移
  Device *devices = deviceManager.getAllDevices();
  bool anyBle = false;
  bool anyWifi = false;
  for (uint8_t i = 0; i < deviceManager.size(); i++)
  {
    anyBle = anyBle || devices[i].isBleConnected();
    anyWifi = anyWifi || devices[i].isWifiConnected();
  }
  if (!((mode == RF_MODE_WIFI && anyBle) || (mode == RF_MODE_BLE && anyWifi)))
  {
    config::config.rf.mode = mode;
    config::save();
    return;
  }

  // 协议迁移耗时较长,交给独立任务执行,避免在LVGL事件回调里阻塞界面
  // (成功后由切换任务保存新模式;失败保持原模式,重新进页面时下拉框会同步)
  if (!rf_request_protocol_switch(mode, false))
  {
    LOGGER_WARN("Protocol switch busy, ignore rf page change.");
    return;
  }
  ui_popwin_msgbox("正在切换传输协议...", nullptr, nullptr);
}
/****************************/
