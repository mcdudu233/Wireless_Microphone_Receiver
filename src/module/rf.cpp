#include "logger.h"
#include "config.h"
#include "module/rf.h"
#include "module/audio/buffer.h"
#include "module/audio/decoder.h"
#include "ui/ui_bt.h"

#include "map"
#include "string"

// 整体配置
static struct
{
  uint8_t type = PACKET_TYPE_SERVER_CONTROL_DEVICE;
  ServerControlDevicePacket packet;
} configDevice;
static struct
{
  uint8_t type = PACKET_TYPE_SERVER_CONTROL_AUDIO;
  ServerControlAudioPacket packet;
} configAudio;
// 缓存的设备
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
struct DeviceConnection
{
#include "host/ble_hs.h"
  // 设备信息
  uint8_t battery;
  // 蓝牙记录
  bool bleConnected;
  bool bleDoConnect;
  ble_addr_t bleAddress;
  uint16_t bleConnectionHandle;
  ble_l2cap_chan *bleChannel;
  // WIFI记录
  bool wifiConnected;
  uint32_t wifiConnectionIP;
};
static std::map<std::string, DeviceConnection> devices;
// static std::map<uint16_t, DeviceConnection *> connectionToDevice;

// 计算音频传输速度
static uint32_t speed = 0;
static uint32_t speedData = 0;
static unsigned long speedLastTime = millis();

/*****************************
          传输层协议
*****************************/
#include "lwip/err.h"
#include "lwip/api.h"
static bool socketIsOpen = false;
static netconn *socketInstance;

static void socket_handle(void *arg)
{
  struct netbuf *buf = NULL;
  ip_addr_t src_addr;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_SOCKET_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);
    if (!socketIsOpen)
    {
      vTaskDelete(NULL);
    }
    if (socketInstance != NULL)
    {
      // 接收数据
      err_t err = netconn_recv(socketInstance, &buf);
      if (err == ERR_OK)
      {
        uint32_t address = buf->addr.addr;

        // 处理数据包
        Packet *packet = (Packet *)((uint8_t *)buf->ptr->payload + WIFI_IP_HEAD_LEN);
        switch (packet->type)
        {
        // 音频数据包
        case PACKET_TYPE_WIFI_AUDIO:
        {
          audio::buffer::writeWiFiPacket(&packet->packet.audioDataWiFi);
          speedData += packet->packet.audioDataWiFi.size;
          break;
        }

        default:
        {
          break;
        }
        }
        // logger::debugln("Socket get data, size=%d", packet->size);

        netbuf_delete(buf);
      }

      // 计算速度
      unsigned long speedNowTime = millis();
      if (speedNowTime - speedLastTime > 1000)
      {
        speed = speedData;
        speedData = 0;
        speedLastTime = speedNowTime;
        logger::debugln("Socket data speed %dKB/s", speed / 1024);
      }
    }
  }
}

static bool socket_close()
{
  if (socketIsOpen)
  {
    socketIsOpen = false;
    netconn_delete(socketInstance);
    socketInstance = NULL;
  }
  logger::debugln("Socket is shutdown.");
  return true;
}

static bool socket_open()
{
  if (socketIsOpen)
  {
    socket_close();
  }

  // 创建
  socketInstance = netconn_new_with_proto_and_callback(NETCONN_RAW, WIFI_IP_PROTOCOL, NULL);
  if (socketInstance == NULL)
  {
    logger::warnln("Socket unable to create:!");
    return false;
  }

  // 绑定到所有地址
  err_t ret = netconn_bind(socketInstance, IP_ADDR_ANY, WIFI_NO_PORT);
  if (ret != ERR_OK)
  {
    logger::warnln("Socket netconn bind failed: %d", ret);
    netconn_delete(socketInstance);
    socketInstance = NULL;
    return false;
  }

  // 阻塞模式
  // netconn_set_recvtimeout(socketInstance, 0);

  socketIsOpen = true;
  xTaskCreatePinnedToCore(socket_handle, "socket_handle", TASK_SOCKET_STACK, NULL, TASK_SOCKET_PRIORITY, NULL, TASK_SOCKET_CORE);
  logger::debugln("Socket is started.");
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
        .max_connection = RF_MAX_CONNECTION,
        .pmf_cfg = {
            .required = true,
        },
        .gtk_rekey_interval = true,
    },
};

static void wifi_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == WIFI_EVENT_AP_STACONNECTED)
  {
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    logger::debugln("station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
  }
  else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
  {
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
    logger::debugln("station " MACSTR " leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
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
    wifiNetIF = NULL;
    wifiIsOpen = false;
  }
  logger::debugln("WiFi is shutdown.");
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

  wifiIsOpen = true;
  logger::debugln("WiFi is started.");
  socket_open();
  return true;
}
/****************************/

/*****************************
          BLE协议
*****************************/
static bool bleIsOpen = false;
static bool bleScanning = false;

// 储存池
#define BLE_L2CAP_COC_BUF_COUNT (20 * MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM))
static os_membuf_t bleMemory[OS_MEMPOOL_SIZE(BLE_L2CAP_COC_BUF_COUNT, BLE_L2CAP_MTU)];
static os_mempool bleMemoryPool;
static os_mbuf_pool bleBufferpool;

static void ble_start_scanning();
static void ble_stop_scanning();

// 地址转换为字符串
static std::string bleBdaToStr(ble_addr_t bda)
{
  char bda_str[18];
  snprintf(bda_str, sizeof(bda_str), "%02x:%02x:%02x:%02x:%02x:%02x",
           bda.val[0], bda.val[1], bda.val[2], bda.val[3], bda.val[4], bda.val[5]);
  return std::string(bda_str);
}

// L2CAP 事件
static int ble_l2cap_handler(struct ble_l2cap_event *event, void *arg)
{
  int rc;
  DeviceConnection *device = (DeviceConnection *)arg;
  struct ble_l2cap_chan_info chan_info;

  switch (event->type)
  {
  // 建立连接事件
  case BLE_L2CAP_EVENT_COC_CONNECTED:
  {
    if (event->connect.status == 0)
    {
      rc = ble_l2cap_get_chan_info(event->connect.chan, &chan_info);
      if (rc != 0)
      {
        logger::warnln("BLE ble_l2cap_get_chan_info error: %d\n", rc);
        break;
      }
      device->bleChannel = event->connect.chan;
      device->bleConnected = true;
      // 更新界面
      ui_bt_update(bleBdaToStr(device->bleAddress), true);
      logger::debugln("BLE LE COC connected, conn: %d, our_mps: %d, our_mtu: %d, peer_mps: %d, peer_mtu: %d\n",
                      event->connect.conn_handle,
                      chan_info.our_l2cap_mtu, chan_info.our_coc_mtu,
                      chan_info.peer_l2cap_mtu, chan_info.peer_coc_mtu);
    }
    else
    {
      logger::warnln("BLE LE COC error: %d\n", event->connect.status);
      break;
    }
    break;
  }

  // 断开连接事件
  case BLE_L2CAP_EVENT_COC_DISCONNECTED:
  {
    device->bleConnected = false;
    device->bleChannel = NULL;
    logger::debugln("BLE LE CoC disconnected, conn: %d", event->disconnect.conn_handle);
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
static int ble_gap_handler(struct ble_gap_event *event, void *arg)
{
  int rc;
  struct ble_gap_conn_desc desc;
  struct ble_hs_adv_fields fields;

  switch (event->type)
  {
  // 发现设备事件
  case BLE_GAP_EVENT_DISC:
  {
    rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
    if (rc != 0)
    {
      logger::warnln("BLE ble_hs_adv_parse_fields failed!");
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
        std::string address = bleBdaToStr(event->disc.addr);
        if (!devices.contains(address))
        {
          devices[address] = {
              // 设备信息
              .battery = 100,
              // 蓝牙记录
              .bleConnected = false,
              .bleDoConnect = true,
              .bleAddress = event->disc.addr,
              .bleConnectionHandle = 0,
              .bleChannel = NULL,
              // WIFI记录
              .wifiConnected = false,
              .wifiConnectionIP = 0x0,
          };

          // 添加新设备到界面
          ui_bt_update(address);
        }
        logger::debugln("BLE find new device: %s.", address);
      }
    }
    break;
  }

  // 设备连接事件
  case BLE_GAP_EVENT_CONNECT:
  {
    if (event->connect.status == 0)
    {
      rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
      if (rc != 0)
      {
        logger::warnln("BLE failed to ble_gap_conn_find; rc=%d\n", rc);
        break;
      }

      std::string address = bleBdaToStr(desc.peer_id_addr);
      if (devices.contains(address))
      {
        DeviceConnection &device = devices[address];
        device.bleConnectionHandle = event->connect.conn_handle;

        // 连接到 L2CAP
        struct os_mbuf *sdu_rx;
        sdu_rx = os_mbuf_get_pkthdr(&bleBufferpool, 0);
        rc = ble_l2cap_connect(device.bleConnectionHandle, BLE_L2CAP_PSM, BLE_L2CAP_MTU, sdu_rx, ble_l2cap_handler, &device);
        if (rc != 0)
        {
          logger::warnln("BLE failed to ble_l2cap_connect; rc=%d\n", rc);
          break;
        }

        // 成功建立连接
        logger::debugln("BLE connection established.");
      }
    }
    else
    {
      logger::warnln("BLE Connection failed; status=%d\n", event->connect.status);
    }
    break;
  }

  // 设备断开连接事件
  case BLE_GAP_EVENT_DISCONNECT:
  {
    logger::debugln("BLE disconnect; reason=%d ", event->disconnect.reason);
    break;
  }

  // 扫描完成事件
  case BLE_GAP_EVENT_DISC_COMPLETE:
  {
    logger::debugln("BLE discovery complete; reason=%d\n", event->disc_complete.reason);
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
  logger::warnln("BLE reset, reason: %d", reason);
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
  logger::debugln("BLE Host Task Started.");
  /* This function will return only when nimble_port_stop() is executed */
  nimble_port_run();
  nimble_port_freertos_deinit();
}

static bool ble_close()
{
  if (bleIsOpen)
  {
    // 停止扫描
    ble_stop_scanning();

    // 停止NimBLE主机任务
    nimble_port_stop();

    // 等待NimBLE停止完成
    int rc = nimble_port_deinit();
    if (rc != 0)
    {
      logger::warnln("BLE NimBLE port deinit failed: %d", rc);
    }

    // 清理内存池
    if (bleBufferpool.omp_pool != NULL)
    {
      os_mempool_clear(&bleMemoryPool);
    }

    bleIsOpen = false;
  }
  logger::debugln("BLE is close.");
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
    logger::warnln("BLE port init failed: %d", ret);
    return false;
  }

  // 初始化配置
  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;
  ble_hs_cfg.store_status_cb = NULL;

  // 创建接受池
  ret = os_mempool_init(&bleMemoryPool, BLE_L2CAP_COC_BUF_COUNT, BLE_L2CAP_MTU, bleMemory, "coc_sdu_pool");
  if (ret != 0)
  {
    logger::warnln("BLE os_mempool_init failed: %d", ret);
    return false;
  }
  ret = os_mbuf_pool_init(&bleBufferpool, &bleMemoryPool, BLE_L2CAP_MTU, BLE_L2CAP_COC_BUF_COUNT);
  if (ret != 0)
  {
    logger::warnln("BLE os_mbuf_pool_init failed: %d", ret);
    return false;
  }

  // 启动 NimBLE 主机任务
  nimble_port_freertos_init(ble_host_task);

  bleIsOpen = true;
  logger::debugln("BLE is started.");
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
      logger::warnln("BLE cancel discovery failed; rc=%d\n", rc);
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
      logger::warnln("BLE error determining address type; rc=%d\n", rc);
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
      logger::warnln("BLE error initiating GAP discovery procedure; rc=%d\n", rc);
    }

    bleScanning = true;
  }
}

// 连接设备
static bool ble_connect_to_device(const std::string &address)
{
  if (bleIsOpen)
  {
    if (devices.contains(address))
    {
      int rc;
      DeviceConnection &device = devices[address];

      uint8_t own_addr_type;
      rc = ble_hs_id_infer_auto(0, &own_addr_type);
      if (rc != 0)
      {
        logger::warnln("BLE error determining address type; rc=%d", rc);
        return false;
      }
      if (bleScanning)
      {
        // ble_gap_disc_cancel();
        ble_stop_scanning();
      }
      rc = ble_gap_connect(own_addr_type, &device.bleAddress, BLE_HS_FOREVER, NULL, ble_gap_handler, NULL);
      if (rc != 0)
      {
        logger::warnln("Error: Failed to connect to device; addr_type=%d; rc=%d\n", device.bleAddress.type, rc);
        return false;
      }
      // if (bleScanning)
      // {
      //   bleScanning = false;
      //   ble_start_scanning();
      // }

      logger::debugln("BLE connecting to server %s.", address.c_str());
      return true;
    }
    logger::warnln("BLE connect failed, because not found address %s.", address);
    return false;
  }
  logger::warnln("BLE connect failed, because BLE not open.");
  return false;
}

// 断开连接设备
static bool ble_disconnect_to_device(const std::string &address)
{
  if (bleIsOpen)
  {
    if (devices.contains(address))
    {
      int rc;
      DeviceConnection &device = devices[address];

      if (device.bleConnected)
      {
        ble_l2cap_disconnect(device.bleChannel);
        ble_gap_terminate(device.bleConnectionHandle, BLE_ERR_REM_USER_CONN_TERM);
      }

      devices.erase(address);
      logger::debugln("BLE disconnecting to server %s.", address.c_str());
      return true;
    }
    logger::warnln("BLE disconnect failed, because not found address %s.", address);
    return false;
  }
  logger::warnln("BLE disconnect failed, because BLE not open.");
  return false;
}

// 发送数据
bool ble_send(DeviceConnection &device, const uint8_t *data, uint16_t len)
{
  if (!bleIsOpen || !device.bleChannel)
  {
    logger::warnln("BLE channel not ready.");
    return false;
  }

  if (len > BLE_L2CAP_MTU)
  {
    logger::warnln("BLE data too large for L2CAP MTU.");
    return false;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
  if (!om)
  {
    logger::warnln("BLE failed to allocate mbuf.");
    return false;
  }

  int rc = ble_l2cap_send(device.bleChannel, om);
  if (rc != 0)
  {
    logger::warnln("BLE failed to send data: %d", rc);
    os_mbuf_free_chain(om);
    return false;
  }

  logger::debugln("BLE sent %d bytes.", len);
  return true;
}
/****************************/

/*****************************
          界面操作
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
  audio::decoder::on(48000, 32);
  for (auto &device : devices)
  {
    wifi_open();
    configAudio.packet.channel = 2;
    configAudio.packet.rate = 48000;
    configAudio.packet.bit = 32;
    configAudio.packet.autoVolumn = false;
    configAudio.packet.peekVolumn = false;
    configAudio.packet.volumn = 40;
    configAudio.packet.start = true;
    ble_send(device.second, (uint8_t *)&configAudio, PACKET_SERVER_CONTROL_AUDIO_SIZE);

    // configDevice.packet.startWiFi = true;
    // configDevice.packet.start = true;
    // ble_send(device.second, (uint8_t *)&configDevice, PACKET_SERVER_CONTROL_DEVICE_SIZE);
  }
}
/****************************/

void rf::reconfigure()
{
  // 刷新配置
  // configBasic.start = false;
  configDevice.packet.mode = config::value.transmitProtocol;
  strcpy(configDevice.packet.name, WIFI_NAME);
  strcpy(configDevice.packet.password, WIFI_PASSWORD);

  // configAudio.start = false;
  configAudio.packet.channel = config::value.audioChannel;
  configAudio.packet.rate = config::value.audioRate;
  configAudio.packet.bit = config::value.audioBit;
  // configAudio.autoVolumn = ;
  // configAudio.peekVolumn = ;
  // configAudio.volumn = ;
}

void rf::setup()
{
  reconfigure();
  ble_open();
}