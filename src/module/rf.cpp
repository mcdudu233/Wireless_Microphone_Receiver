#include "logger.h"
#include "config.h"
#include "module/rf.h"
#include "module/audio/buffer.h"
#include "module/audio/decoder.h"
#include "tool/device.h"
#include "ui/ui_bt.h"
#include "ui/ui_setting.h"

#include "string"

// 缓存的设备
static DeviceManager deviceManager;

// 计算音频传输速度
static uint32_t transmitSpeed = 0;
static uint32_t transmitSpeedData = 0;

/*****************************
          WIFI协议
*****************************/
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/api.h"
static bool wifiIsOpen = false;
static bool netifInitialized = false;
static bool eventLoopInitialized = false;
static uint32_t wifiIP;
static esp_netif_t *wifiNetIF;
static esp_event_handler_instance_t wifiHandlerInstance1;
static esp_event_handler_instance_t wifiHandlerInstance2;
static bool socketIsOpen = false;
static netconn *socketSendInstance = NULL;
static netconn *socketReceiveInstance = NULL;

// 发送数据 需要 netbuf_new
static bool wifi_send(const Device &device, netbuf *buf)
{
  if (buf == NULL || socketSendInstance == NULL)
  {
    if (buf != NULL) netbuf_delete(buf);
    return false;
  }
  if (device.isWifiConnected())
  {
    ip_addr_t socketDestination;
    socketDestination.u_addr.ip4.addr = htonl(device.getWifiIP());
    err_t err = netconn_sendto(socketSendInstance, buf, &socketDestination, WIFI_NO_PORT);
    if (err != ERR_OK)
    {
      LOGGER_WARN("WiFi Socket send failed: %d", err);
      netbuf_delete(buf);
      return false;
    }
    // 释放
    netbuf_delete(buf);
    return true;
  }
  else
  {
    LOGGER_WARN("WiFi Socket send failed, device haven't connect to WiFi.");
    netbuf_delete(buf);
    return false;
  }
}

// 读取数据 需要 netbuf_delete
static bool wifi_receive(Device **device, netbuf **buf)
{
  err_t err = netconn_recv(socketReceiveInstance, buf);
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

static bool wifi_socket_close()
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
    socketReceiveInstance = NULL;
  }
  LOGGER_INFO("WiFi Socket is shutdown.");
  return true;
}

static bool wifi_socket_open(uint32_t localIP)
{
  if (socketIsOpen)
  {
    wifi_socket_close();
  }

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
    LOGGER_WARN("WiFi Socket unable to create!");
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
  LOGGER_INFO("WiFi Socket is started.");
  return true;
}

static void wifi_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
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
    wifi_socket_close();
    wifiIsOpen = false;

    esp_err_t err;
    err = esp_wifi_stop();
    if (err != ESP_OK)
    {
      LOGGER_ERROR("WiFi esp_wifi_stop failed! Reason=%s", esp_err_to_name(err));
      return false;
    }

    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiHandlerInstance1);
    if (err != ESP_OK)
    {
      LOGGER_ERROR("WiFi esp_event_handler_instance_unregister failed! Reason=%s", esp_err_to_name(err));
      return false;
    }
    err = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &wifiHandlerInstance2);
    if (err != ESP_OK)
    {
      LOGGER_ERROR("WiFi esp_event_handler_instance_unregister failed! Reason=%s", esp_err_to_name(err));
      return false;
    }

    err = esp_wifi_deinit();
    if (err != ESP_OK)
    {
      LOGGER_ERROR("WiFi esp_wifi_deinit failed! Reason=%s", esp_err_to_name(err));
      return false;
    }

    esp_netif_destroy(wifiNetIF);
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

  // 初始化 WiFi
  static wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&wifi_init_config);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_event_loop_create_default failed! Reason=%s", esp_err_to_name(err));
    return false;
  }

  // 注册事件
  err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handle, NULL, &wifiHandlerInstance1);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_event_handler_instance_register failed! Reason=%s", esp_err_to_name(err));
    return false;
  }
  err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handle, NULL, &wifiHandlerInstance2);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_event_handler_instance_register failed! Reason=%s", esp_err_to_name(err));
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
    return false;
  }
  err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_wifi_set_config failed! Reason=%s", esp_err_to_name(err));
    return false;
  }
  err = esp_wifi_start();
  if (err != ESP_OK)
  {
    LOGGER_ERROR("WiFi esp_wifi_start failed! Reason=%s", esp_err_to_name(err));
    return false;
  }

  // 获取当前IP地址
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(wifiNetIF, &ip) == ESP_OK)
  {
    wifiIP = ip.ip.addr;
  }

  wifiIsOpen = true;
  LOGGER_INFO("WiFi is started.");

  if (!wifi_socket_open(wifiIP))
  {
    wifi_close();
    return false;
  }
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

static bool bleIsOpen = false;
static bool bleScanning = false;
// 连接参数
static ble_gap_upd_params bleConnectionParams = {
    /** Minimum value for connection interval in 1.25ms units */
    .itvl_min = BLE_ITVL_MIN,
    /** Maximum value for connection interval in 1.25ms units */
    .itvl_max = BLE_ITVL_MAX,
    /** Connection latency */
    .latency = BLE_LATENCY,
    /** Supervision timeout in 10ms units */
    .supervision_timeout = BLE_SUPERVISION_TIMEOUT,
    /** Minimum length of connection event in 0.625ms units */
    .min_ce_len = BLE_CE_LEN_MIN,
    /** Maximum length of connection event in 0.625ms units */
    .max_ce_len = BLE_CE_LEN_MAX,
};
// 储存池
struct bleReceive
{
  Device *device;
  uint16_t size;
  uint8_t data[BLE_L2CAP_MTU];
};
static QueueHandle_t bleReceiveQueue;

static void ble_start_scanning();
static void ble_stop_scanning();

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
    sdu_rx = os_msys_get_pkthdr(BLE_L2CAP_MTU, 0);
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
      LOGGER_INFO("BLE received %d bytes on L2CAP channel.", size);
    }

    // 响应数据 准备接收下一个数据包
    os_mbuf *sdu_rx;
    sdu_rx = os_msys_get_pkthdr(BLE_L2CAP_MTU, 0);
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
      // 更新连接参数提升性能
      rc = ble_gap_update_params(event->connect.conn_handle, &bleConnectionParams);
      if (rc != 0)
      {
        LOGGER_WARN("BLE failed to update params; rc = %d", rc);
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
        sdu_rx = os_msys_get_pkthdr(BLE_L2CAP_MTU, 0);
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
}

// NimBLE 协议栈加载完成
static void ble_on_sync(void)
{
  // 开始搜索设备
  // ble_start_scanning();
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

  // 初始化配置
  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;
  ble_hs_cfg.store_status_cb = NULL;
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
      rc = ble_gap_connect(own_addr_type, &peer_addr, BLE_HS_FOREVER, NULL, ble_gap_handler, NULL);
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
  if (rc != 0)
  {
    LOGGER_WARN("BLE failed to send data: %d", rc);
    os_mbuf_free_chain(om);
    return false;
  }

  LOGGER_INFO("BLE sent %d bytes.", len);
  return true;
}
/****************************/

// 解析音频包内PCM样本，计算左右声道实时峰值电平(0-100)并写入设备
// 音频载荷为发送端按当前配置转换后的裸PCM: 采样位深=audio.bit, 声道数=audio.channel
// 峰值保持: 仅当新峰值更高时更新，每秒由 rf_handle 统一衰减，避免闪烁
static void rf_update_voice_level(Device *device, const uint8_t *data, uint16_t size)
{
  if (device == nullptr || data == nullptr || size == 0)
  {
    return;
  }

  const uint8_t bytesPerSample = (uint8_t)(config::config.audio.bit / 8); // 2/3/4
  const uint8_t channels = (uint8_t)config::config.audio.channel;         // 1/2
  const int64_t fullScale = (int64_t)1 << (config::config.audio.bit - 1); // 2^(bit-1)
  const size_t sampleCount = size / bytesPerSample;

  uint8_t peakL = 0;
  uint8_t peakR = 0;
  for (size_t i = 0; i + channels <= sampleCount; i += channels)
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

      uint8_t pct = (uint8_t)((magnitude * 100) / fullScale);
      if (ch == 0)
      {
        if (pct > peakL)
          peakL = pct;
        if (channels == 1 && pct > peakR)
          peakR = pct; // 单声道同时驱动L/R
      }
      else if (pct > peakR)
      {
        peakR = pct;
      }
    }
  }

  if (peakL > device->getVoiceLevelL())
    device->setVoiceLevelL(peakL);
  if (peakR > device->getVoiceLevelR())
    device->setVoiceLevelR(peakR);
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
    audio::buffer::writeWiFiPacket(&packet->packet.audioDataWiFi);
    // 更新设备实时音频电平
    if (device)
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
    audio::buffer::writeBLEPacket(&packet->packet.audioDataBLE);
    // 更新设备实时音频电平
    if (device)
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
static void rf_handle(void *arg)
{
  // 缓存
  Device *device = nullptr;
  netbuf *receiveBuffer = NULL;
  // rssi
  wifi_sta_list_t staList;

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
      }
    }

    /* 处理 WIFI 模块 */
    if (wifiIsOpen && socketIsOpen)
    {
      /* 发送 */
      //////////

      /* 接收 */
      if (wifi_receive(&device, &receiveBuffer))
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
      }
    }

    /* 每秒执行一次获取其他信息 */
    unsigned long secondNowTime = millis();
    if (secondNowTime - secondLastTime > 1000)
    {
      /* 计算速度 */
      transmitSpeed = transmitSpeedData;
      transmitSpeedData = 0;
      secondLastTime = secondNowTime;
      // LOGGER_INFO("RF data speed %dKB/s", transmitSpeed / 1024);

      /* 衰减音频电平 (无新音频时逐渐归零) */
      {
        Device *levelDevices = deviceManager.getAllDevices();
        for (uint8_t i = 0; i < deviceManager.size(); i++)
        {
          Device &levelDevice = levelDevices[i];
          levelDevice.setVoiceLevelL(levelDevice.getVoiceLevelL() > 10 ? levelDevice.getVoiceLevelL() - 10 : 0);
          levelDevice.setVoiceLevelR(levelDevice.getVoiceLevelR() > 10 ? levelDevice.getVoiceLevelR() - 10 : 0);
        }
      }

      /* 获取信号强度 */
      if (bleIsOpen)
      {
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
    }
  }
}

void rf::setup()
{
  bleReceiveQueue = xQueueCreate(16, sizeof(bleReceive));
  if (bleReceiveQueue == nullptr)
  {
    LOGGER_ERROR("BLE receive queue creation failed.");
    return;
  }
  ble_open();
  // 启动发送接收线程
  xTaskCreatePinnedToCore(rf_handle, "rf_handle", TASK_RF_STACK, NULL, TASK_RF_PRIORITY, NULL, TASK_RF_CORE);
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
    wifi_open();

    for (uint8_t i = 0; i < deviceManager.size(); i++)
    {
      Device &device = devices[i];
      if (device.isBleConnected())
      {
        // 让设备以 WiFi 模式接入
        Packet *packet = (Packet *)malloc(PACKET_SERVER_CONTROL_RF_SIZE);
        packet->type = PACKET_TYPE_SERVER_CONTROL_RF;
        packet->packet.serverControlRF.mode = RF_MODE_WIFI;
        // TODO: WiFi 名字和密码随机加密
        snprintf(packet->packet.serverControlRF.ssid,
                 sizeof(packet->packet.serverControlRF.ssid), "%s", WIFI_NAME);
        snprintf(packet->packet.serverControlRF.password,
                 sizeof(packet->packet.serverControlRF.password), "%s", WIFI_PASSWORD);
        ble_send(device, (uint8_t *)packet, PACKET_SERVER_CONTROL_RF_SIZE);
        free(packet);

        // 等待设备通过 WiFi 连接成功
        while (!device.isWifiConnected())
        {
          delay(100);
        }

        // 开启设备音频传输
        netbuf *buf = netbuf_new();
        if (buf != NULL)
        {
          Packet *packet = (Packet *)netbuf_alloc(buf, PACKET_SERVER_CONTROL_AUDIO_SIZE);
          packet->type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
          packet->packet.serverControlAudio.start = true;
          packet->packet.serverControlAudio.channel = config::config.audio.channel;
          packet->packet.serverControlAudio.rate = config::config.audio.rate;
          packet->packet.serverControlAudio.bit = config::config.audio.bit;
          packet->packet.serverControlAudio.mode = config::config.audio.mode;
          packet->packet.serverControlAudio.gain = config::config.audio.gain;
          wifi_send(device, buf);
        }
      }
    }

    ble_close();
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

const std::string ui_info_get_transmit_speed()
{
  return std::to_string(transmitSpeed / 1024) + " KB/s";
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

void ui_setting_audio_page_rcb(AudioBit &bit, AudioChannel &channel, AudioRate &rate, AudioGain &gain, AudioMode &mode)
{
  bit = config::config.audio.bit;
  channel = config::config.audio.channel;
  rate = config::config.audio.rate;
  gain = config::config.audio.gain;
  mode = config::config.audio.mode;
}

void ui_setting_audio_page_scb(AudioBit bit, AudioChannel channel, AudioRate rate, AudioGain gain, AudioMode mode, bool now)
{
  LOGGER_INFO("ui_setting_audio_page_scb");
  config::config.audio.bit = (AudioBit)bit;
  config::config.audio.channel = (AudioChannel)channel;
  config::config.audio.rate = (AudioRate)rate;
  config::config.audio.gain = (AudioGain)gain;
  config::config.audio.mode = (AudioMode)mode;
  config::save();

  if (now)
  {
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
  config::config.audio.bit = bit;
  config::config.audio.channel = channel;
  config::config.audio.rate = rate;
  config::config.audio.gain = gain;
  config::config.audio.mode = mode;
  config::save();
}

void ui_setting_rf_page_rcb(RFMode &mode)
{
  mode = config::config.rf.mode;
}

void ui_setting_rf_page_scb(RFMode mode, bool now)
{
  LOGGER_INFO("ui_setting_rf_page_scb");
  if (now)
  {
    // RF 模式切换需要重启通信协议
    Device *devices = deviceManager.getAllDevices();
    for (uint8_t i = 0; i < deviceManager.size(); i++)
    {
      Device &device = devices[i];
      if (device.isBleConnected())
      {
        // 通过 BLE 发送 RF 模式切换命令
        Packet *packet = (Packet *)malloc(PACKET_SERVER_CONTROL_RF_SIZE);
        if (packet)
        {
          packet->type = PACKET_TYPE_SERVER_CONTROL_RF;
          packet->packet.serverControlRF.mode = mode;
          snprintf(packet->packet.serverControlRF.ssid,
                   sizeof(packet->packet.serverControlRF.ssid), "%s", WIFI_NAME);
          snprintf(packet->packet.serverControlRF.password,
                   sizeof(packet->packet.serverControlRF.password), "%s", WIFI_PASSWORD);
          ble_send(device, (uint8_t *)packet, PACKET_SERVER_CONTROL_RF_SIZE);
          free(packet);
        }
      }
    }
  }
  config::config.rf.mode = mode;
  config::save();
}
/****************************/
