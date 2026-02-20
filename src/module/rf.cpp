#include "logger.h"
#include "config.h"
#include "module/rf.h"
#include "module/audio/buffer.h"
#include "module/audio/decoder.h"
#include "tool/device.h"
#include "ui/ui_bt.h"
#include "ui/ui_setting.h"

#include "queue"
#include "string"

// 缓存的设备
static DeviceManager deviceManager;

// 计算音频传输速度
static uint32_t transmitSpeed = 0;
static uint32_t transmitSpeedData = 0;

/*****************************
          传输层协议
*****************************/
#include "lwip/err.h"
#include "lwip/api.h"
static bool socketIsOpen = false;
static netconn *socketSendInstance = NULL;
static netconn *socketReceiveInstance = NULL;

// 发送数据 需要 netbuf_new
static bool socket_send(uint32_t destIP, netbuf *buf)
{
  ip_addr_t socketDestination;
  socketDestination.addr = destIP;
  err_t err = netconn_sendto(socketSendInstance, buf, &socketDestination, WIFI_NO_PORT);
  if (err != ERR_OK)
  {
    LOGGER_WARN("Socket send failed: %d", err);
    return false;
  }
  // 释放
  netbuf_delete(buf);
  return true;
}

// 读取数据 需要 netbuf_delete
static bool socket_receive(netbuf **buf)
{
  err_t err = netconn_recv(socketReceiveInstance, buf);
  if (err == ERR_WOULDBLOCK)
  {
    return false;
  }
  else if (err != ERR_OK)
  {
    LOGGER_WARN("Socket receive failed: %d", err);
    return false;
  }
  return true;
}

static bool socket_close()
{
  if (socketIsOpen)
  {
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
  LOGGER_INFO("Socket is shutdown.");
  return true;
}

static bool socket_open(uint32_t localIP)
{
  if (socketIsOpen)
  {
    socket_close();
  }

  // 创建
  socketSendInstance = netconn_new_with_proto_and_callback(NETCONN_RAW, WIFI_IP_PROTOCOL, NULL);
  if (socketSendInstance == NULL)
  {
    LOGGER_WARN("Socket unable to create:!");
    return false;
  }
  socketReceiveInstance = netconn_new_with_proto_and_callback(NETCONN_RAW, WIFI_IP_PROTOCOL, NULL);
  if (socketReceiveInstance == NULL)
  {
    netconn_delete(socketSendInstance);
    LOGGER_WARN("Socket unable to create:!");
    return false;
  }

  // 绑定到指定地址
  ip_addr_t local_ip = {.addr = localIP};
  err_t ret = netconn_bind(socketSendInstance, &local_ip, WIFI_NO_PORT);
  if (ret != ERR_OK)
  {
    netconn_delete(socketSendInstance);
    socketSendInstance = NULL;
    netconn_delete(socketReceiveInstance);
    socketReceiveInstance = NULL;
    LOGGER_WARN("Socket netconn bind failed: %d", ret);
    return false;
  }
  ret = netconn_bind(socketReceiveInstance, IP_ADDR_ANY, WIFI_NO_PORT);
  if (ret != ERR_OK)
  {
    netconn_delete(socketSendInstance);
    socketSendInstance = NULL;
    netconn_delete(socketReceiveInstance);
    socketReceiveInstance = NULL;
    LOGGER_WARN("Socket netconn bind failed: %d", ret);
    return false;
  }

  // 设置非阻塞模式
  netconn_set_nonblocking(socketReceiveInstance, true);

  socketIsOpen = true;
  LOGGER_INFO("Socket is started.");
  return true;
}
/****************************/

/*****************************
          WIFI协议
*****************************/
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
static bool wifiIsOpen = false;
static uint32_t wifiIP;
static esp_netif_t *wifiNetIF;
static esp_event_handler_instance_t wifiHandlerInstance;
static const wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
static wifi_config_t wifiConfig = {
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

// 在DHCP分配IP时回调
static void dhcp_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == IP_EVENT_AP_STAIPASSIGNED)
  {
    ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
    // 记录映射
    deviceManager.bindWifiMACToIP(event->mac, htonl(event->ip.addr)); // 转换IP字节序
  }
}

static void wifi_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == WIFI_EVENT_AP_STACONNECTED)
  {
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    Device *device = deviceManager.getDeviceByWifiMAC(event->mac);
    if (device != nullptr)
    {
      device->setWifiConnected(true);
      LOGGER_INFO("WiFi station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else
    {
      LOGGER_WARN("WiFi can't find device by MAC " MACSTR, MAC2STR(event->mac));
    }
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

static bool wifi_close()
{
  if (wifiIsOpen)
  {
    socket_close();
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiHandlerInstance));
    ESP_ERROR_CHECK(esp_wifi_deinit());
    esp_netif_destroy(wifiNetIF);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, NULL);
    wifiNetIF = NULL;
    wifiIsOpen = false;
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

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifiNetIF = esp_netif_create_default_wifi_ap();

  ESP_ERROR_CHECK(esp_wifi_init(&wifiInitConfig));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handle, NULL, &wifiHandlerInstance));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());

  // 获取当前IP地址
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(wifiNetIF, &ip) == ESP_OK)
  {
    wifiIP = ip.ip.addr;
  }

  // 注册DHCP事件处理
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &dhcp_event_handle, NULL, NULL);

  wifiIsOpen = true;
  LOGGER_INFO("WiFi is started.");
  socket_open(wifiIP);
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
  uint8_t *data;
};
static std::queue<bleReceive> bleReceiveQueue;

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
      uint8_t *data = event->receive.sdu_rx->om_data;
      uint16_t size = event->receive.sdu_rx->om_len;
      uint8_t *packet = (uint8_t *)malloc(size);
      memcpy(packet, data, size);
      bleReceiveQueue.push({
          .device = device,
          .data = packet,
      });
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

// 解析数据包
static void rf_receive_packet(Device *device, const uint8_t *data)
{
  Packet *packet = (Packet *)data;
  switch (packet->type)
  {
  // WIFI音频数据包
  case PACKET_TYPE_WIFI_AUDIO:
  {
    audio::buffer::writeWiFiPacket(&packet->packet.audioDataWiFi);
    break;
  }

  // BLE音频数据包
  case PACKET_TYPE_BLE_AUDIO:
  {
    // audio::buffer::writeBLEPacket(&packet->packet.audioDataWiFi);
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
    device->setBattery(packet->packet.clientStatus.battery);
    // device->print();
    break;
  }

  default:
  {
    LOGGER_WARN("RF unknow packet type=%d", packet->type);
    break;
  }
  }
}

static unsigned long secondLastTime = millis(); // 每秒时间点
static void rf_handle(void *arg)
{
  // 缓存
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
      while (!bleReceiveQueue.empty())
      {
        bleReceive &receive = bleReceiveQueue.front();
        // 解析数据包
        rf_receive_packet(receive.device, receive.data);
        free(receive.data);
        bleReceiveQueue.pop();
      }
    }

    /* 处理 WIFI 模块 */
    if (wifiIsOpen && socketIsOpen)
    {
      /* 发送 */
      //////////

      /* 接收 */
      if (socket_receive(&receiveBuffer))
      {
        uint8_t *data;
        uint16_t len;
        do
        {
          netbuf_data(receiveBuffer, (void **)&data, &len);
          data += WIFI_IP_HEAD_LEN;
          len -= WIFI_IP_HEAD_LEN;
          // 解析数据包 receiveBuffer->addr.addr
          Device *device = deviceManager.getDeviceByWifiIP(htonl(receiveBuffer->addr.addr));
          if (device != nullptr)
          {
            rf_receive_packet(device, data);
          }
          else
          {
            LOGGER_WARN("WiFi can't get device by unknowed IP address");
          }
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
        strcpy(packet->packet.serverControlRF.ssid, WIFI_NAME);
        strcpy(packet->packet.serverControlRF.password, WIFI_PASSWORD);
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
          socket_send(device.getWifiIP(), buf);
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
      std::string str = devices[i].getBleMACString();
      strs.push_back(str);
    }
  }
  return strs;
}

int8_t ui_info_get_left_voice(const std::string &mac)
{
  return 0;
}

int8_t ui_info_get_right_voice(const std::string &mac)
{
  return 0;
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
  // bit = config::config.audio.bit;
  // channel = config::config.audio.channel;
  // rate = config::config.audio.rate;
  // volumn = config::config.audio.volumn;
  // if (config::config.audio.autoVolumn)
  // {
  //   volumn_mode = 0;
  // }
  // else if (config::config.audio.peekVolumn)
  // {
  //   volumn_mode = 1;
  // }
  // else
  // {
  //   volumn_mode = 2;
  // }
}

void ui_setting_audio_page_scb(AudioBit bit, AudioChannel channel, AudioRate rate, AudioGain gain, AudioMode mode, bool now)
{
  // config::config.audio.bit = bit;
  // config::config.audio.channel = channel;
  // config::config.audio.rate = rate;
  // config::config.audio.volumn = volumn;
  // if (volumn_mode == 0)
  // {
  //   config::config.audio.autoVolumn = true;
  //   config::config.audio.peekVolumn = false;
  // }
  // else if (volumn_mode == 1)
  // {
  //   config::config.audio.autoVolumn = false;
  //   config::config.audio.peekVolumn = true;
  // }
  // else
  // {
  //   config::config.audio.autoVolumn = false;
  //   config::config.audio.peekVolumn = false;
  // }
  // config::save();

  // if (now)
  // {
  // }
}

void ui_setting_rf_page_rcb(RFMode &mode)
{
  mode = config::config.rf.mode;
}

void ui_setting_rf_page_scb(RFMode mode, bool now)
{
  // config::config.rf.mode = mode;
  // config::save();

  // if (now)
  // {
  // }
}
/****************************/