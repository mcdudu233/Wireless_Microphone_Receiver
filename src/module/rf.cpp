#include "logger.h"
#include "config.h"
#include "module/rf.h"
#include "module/audio/decoder.h"

#include "map"
#include "iostream"
#include "random"
#include "string"
#include "algorithm"

// 接收到的音频数据包
static AudioPacket packet;
// 整体配置
static ConfigControl configBasic;
static AudioControl configAudio;
// 缓存的设备
static std::map<std::string, DeviceConnection> devices;
static std::map<uint16_t, DeviceConnection *> connectionToDevice;

/*****************************
          传输层协议
         TCP 和 UDP
*****************************/
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
// TCP和UDP不能同时启用
static bool socketIsOpen = false;
static bool socketIsTCP;
static int socketNumber;

static void socket_handle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_SOCKET_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);
    if (!socketIsOpen)
    {
      vTaskDelete(NULL);
    }
    if (socketIsTCP)
    {
      struct sockaddr_storage source_addr;
      socklen_t source_addr_len = sizeof(source_addr);
      int sock = accept(socketNumber, (struct sockaddr *)&source_addr, &source_addr_len);
      if (sock >= 0)
      {
        logger::debugln("Socket get new client.");
      }
      else if (errno != EWOULDBLOCK)
      {
        logger::warnln("Socket could not get new client: %d", errno);
      }
      // int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, MSG_DONTWAIT | MSG_PEEK);
      // if (len >= 0)
      // {
      //   if (len == 0)
      //   {
      //     logger::debugln( "Socket connection closed.");
      //   }
      //   ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
      // }
    }
    else
    {
      struct sockaddr_storage source_addr;
      socklen_t source_addr_len = sizeof(source_addr);
      int len = recvfrom(socketNumber, &packet, sizeof(AudioPacket), MSG_DONTWAIT, (struct sockaddr *)&source_addr, &source_addr_len);
      if (len >= 0)
      {
        audio::decoder::writeData(packet.data);
        // logger::debugln("Socket get new packet, length is %d.", len);
      }
      else if (errno != EWOULDBLOCK)
      {
        logger::warnln("Socket could not receive data: %d", errno);
      }
    }
  }
}

static bool socket_close()
{
  if (socketIsOpen)
  {
    socketIsOpen = false;
    close(socketNumber);
  }
  logger::debugln("Socket is shutdown.");
  return true;
}

static bool socket_open(bool isTCP)
{
  if (socketIsOpen)
  {
    socket_close();
  }

  socketIsTCP = isTCP;
  if (isTCP)
  {
    socketNumber = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  }
  else
  {
    socketNumber = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  }
  struct sockaddr_in server;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_family = AF_INET;
  server.sin_port = htons(SOCKET_PORT);
  int opt = 1;
  setsockopt(socketNumber, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  int err = bind(socketNumber, (struct sockaddr *)&server, sizeof(server));
  if (err != 0)
  {
    close(socketNumber);
    logger::debugln("Socket unable to bind: errno %d", errno);
    return false;
  }
  fcntl(socketNumber, F_SETFL, O_NONBLOCK); // 不阻塞

  // 监听 TCP
  if (isTCP)
  {
    // TCP
    err = listen(socketNumber, RF_MAX_CONNECTION);
    if (err != 0)
    {
      close(socketNumber);
      logger::debugln("Socket unable to listen: errno %d", errno);
      return false;
    }
  }

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
    ESP_ERROR_CHECK(esp_wifi_stop());
    // TODO 报错 ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, (void (*)(void *, const char *, long int, void *))wifi_event_handle));
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
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handle, NULL, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());

  wifiIsOpen = true;
  logger::debugln("WiFi is started.");
  return true;
}
/****************************/

/*****************************
          BLE协议
*****************************/
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static bool bleIsOpen = false;
static bool bleScanning = false;
static const uint16_t bleGattcId = 0;
static uint16_t bleGattcInterface = ESP_GATT_IF_NONE;

// 地址转换为字符串
static std::string bleBdaToStr(esp_bd_addr_t bda)
{
  char bda_str[18];
  snprintf(bda_str, sizeof(bda_str), "%02x:%02x:%02x:%02x:%02x:%02x",
           bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return std::string(bda_str);
}

// 接收到电量信息
static void ble_battery_handler(DeviceConnection &device, uint8_t *data, uint16_t data_len)
{
  device.battery = data[0];
  logger::debugln("BLE battery level: %d", data[0]);
}

// 接收到音频数据包
static void ble_data_handler(DeviceConnection &device, uint8_t *data, uint16_t data_len)
{
  // 音频数据通知
  if (data_len < sizeof(AudioPacket))
  {
    memcpy(&packet, data, data_len);
  }
  else
  {
    packet = *(AudioPacket *)data;
  }

  // static uint32_t last_time = 0;
  // static uint32_t last_num = 0;
  // static uint32_t packet_size = 0;
  // static uint32_t packet_num = 0;

  // packet_size += value_len;
  // if (last_num != 0)
  // {
  //   packet_num += packet->num - last_num - 1;
  // }
  // last_num = packet->num;

  // uint32_t now_time = esp_timer_get_time() / 1000;
  // if (now_time - last_time > 1000)
  // {
  //   last_time = now_time;
  //   float loss_rate = (packet_num > 0) ? (packet_num / 1000.0f * 100.0f) : 0.0f;
  //   logger::debugln("BLE get packet for %dbytes/s and loss for %.2f%%.", packet_size, loss_rate);
  //   packet_size = 0;
  //   packet_num = 0;
  // }

  // audio::decoder::writeData(packet->data);
}

// 接收到配置
static void ble_config_control_handler(DeviceConnection &device, uint8_t *data, uint16_t data_len)
{
  if (data_len == sizeof(ConfigControl))
  {
    ConfigControl *src = (ConfigControl *)data;
    configBasic.start = src->start;
    configBasic.mode = src->mode;
    configBasic.ip = src->ip;
    strcpy(configBasic.name, src->name);
    strcpy(configBasic.password, src->password);
    logger::debugln("BLE get indicate config.");
  }
}

// 接收到音频配置
static void ble_audio_control_handler(DeviceConnection &device, uint8_t *data, uint16_t data_len)
{
  // 音频控制指示
  if (data_len == sizeof(AudioControl))
  {
    configAudio = *(AudioControl *)data;
    logger::debugln("BLE get audio control config.");
  }
}

// GAP事件处理
static bool ble_connect_to_device(const std::string &address);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  switch (event)
  {
  case ESP_GAP_BLE_SCAN_RESULT_EVT:
  {
    esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
    switch (scan_result->scan_rst.search_evt)
    {
    case ESP_GAP_SEARCH_INQ_RES_EVT:
    {
      // 解析设备名称
      uint8_t name_len = 0;
      uint8_t *name_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &name_len);
      if (name_data == NULL || name_len == 0)
      {
        // 如果没有完整的设备名称，尝试获取缩短的设备名称
        name_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &name_len);
      }
      if (name_data == NULL || name_len == 0)
      {
        break;
      }
      // 如果解析到一样的设备名
      if (strcmp((char *)name_data, BLE_NAME) == 0)
      {
        // 解析蓝牙地址
        std::string address = bleBdaToStr(scan_result->scan_rst.bda);

        if (!devices.contains(address))
        {
          // 新设备，添加到待连接列表
          devices.insert(std::make_pair(address, (DeviceConnection){.battery = 100,
                                                                    .bleConnected = false,
                                                                    .bleDoConnect = true,
                                                                    .wifiConnected = false}));
          memcpy(devices[address].bleAddress, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
          logger::debugln("BLE found device: %s, address: %s", name_data, address.c_str());

          ble_connect_to_device(address);
          // // 停止扫描并连接
          // esp_ble_gap_stop_scanning();
          // ble_scanning = false;
        }
      }
      break;
    }
    case ESP_GAP_SEARCH_INQ_CMPL_EVT:
      logger::debugln("BLE scan complete.");
      break;
    default:
      logger::debugln("BLE unknow GAP status.");
      break;
    }
    break;
  }

  case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
  {
    bleScanning = false;
    logger::debugln("BLE scan param set complete");
    break;
  }

  case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
  {
    if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
    {
      logger::warnln("BLE scan start failed.");
    }
    else
    {
      logger::debugln("BLE scan started.");
      bleScanning = true;
    }
    break;
  }

  default:
    break;
  }
}

// 全局GATTC事件处理
static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
  // 注册事件
  if (event == ESP_GATTC_REG_EVT)
  {
    if (param->reg.status == ESP_GATT_OK)
    {
      if (param->reg.app_id == bleGattcId)
      {
        bleGattcInterface = gattc_if;
        logger::debugln("BLE register app sucess, app_id %d, gattc_if %d", param->reg.app_id, gattc_if);
      }
      else
      {
        logger::warnln("BLE register app failed, app_id %d, status %d", param->reg.app_id, param->reg.status);
      }
    }
    else
    {
      logger::warnln("BLE register app failed, app_id %d, status %d", param->reg.app_id, param->reg.status);
    }
    return;
  }

  if (gattc_if == ESP_GATT_IF_NONE || gattc_if == bleGattcInterface)
  {
    switch (event)
    {
    // 连接成功事件
    case ESP_GATTC_OPEN_EVT:
    { // 查找对应的profile
      std::string address = bleBdaToStr(param->open.remote_bda);
      if (param->open.status != ESP_GATT_OK)
      {
        logger::warnln("BLE %s connect failed.", address);
        if (devices.contains(address))
        {
          devices[address].bleConnected = false;
          devices[address].bleDoConnect = false; // TODO: 连接失败
        }
      }
      else
      {
        logger::debugln("BLE %s connected to server. MTU is %d.", address, param->open.mtu);
        if (devices.contains(address))
        {
          devices[address].bleConnected = true;
          devices[address].bleConnectionID = param->open.conn_id;
          connectionToDevice[param->open.conn_id] = &devices[address];
          // 开始搜索服务
          esp_ble_gattc_search_service(gattc_if, param->open.conn_id, NULL);
        }
      }
      break;
    }

    // 断开连接事件
    case ESP_GATTC_DISCONNECT_EVT:
    {
      std::string address = bleBdaToStr(param->disconnect.remote_bda);
      logger::debugln("BLE %d disconnected.", address);
      if (devices.contains(address))
      {
        devices[address].bleConnected = false;
        devices[address].bleDoConnect = true;
        connectionToDevice.erase(devices[address].bleConnectionID);
      }
      break;
    }

    // 搜索到服务事件
    case ESP_GATTC_SEARCH_RES_EVT:
    {
      esp_gatt_id_t *srvc_id = &param->search_res.srvc_id;
      if (srvc_id->uuid.len == ESP_UUID_LEN_16)
      {
        if (connectionToDevice.contains(param->search_res.conn_id))
        {
          DeviceConnection *device = connectionToDevice[param->search_res.conn_id];
          uint16_t uuid = srvc_id->uuid.uuid.uuid16;
          // 检查服务UUID
          switch (uuid)
          {
          case BATTERY_SERVICE_UUID:
          {
            device->bleBatteryStart = param->search_res.start_handle;
            device->bleBatteryEnd = param->search_res.end_handle;
            logger::debugln("BLE found battery service, start at %d, end at %d.", param->search_res.start_handle, param->search_res.end_handle);
            break;
          }
          case AUDIO_SERVICE_UUID:
          {
            device->bleAudioStart = param->search_res.start_handle;
            device->bleAudioEnd = param->search_res.end_handle;
            logger::debugln("BLE found audio service, start at %d, end at %d.", param->search_res.start_handle, param->search_res.end_handle);
            break;
          }
          }
        }
      }
      break;
    }

    // 服务搜索完成事件
    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
      logger::debugln("BLE service search complete.");
      if (connectionToDevice.contains(param->search_cmpl.conn_id))
      {
        DeviceConnection *device = connectionToDevice[param->search_cmpl.conn_id];

        // 开始发现特征
        esp_gattc_char_elem_t char_elem;
        uint16_t count;
        esp_gatt_status_t status;
        // 1. 查询电池服务的电池特征 (0x2A19)
        if (device->bleAudioStart != 0 && device->bleBatteryEnd != 0)
        {
          if (device->bleBattery == 0)
          {
            esp_bt_uuid_t battery_char_uuid = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = BATTERY_CHARACTERISTIC_UUID}};
            status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                    param->search_cmpl.conn_id,
                                                    device->bleBatteryStart,
                                                    device->bleBatteryEnd,
                                                    battery_char_uuid,
                                                    &char_elem, &count);
            if (status == ESP_GATT_OK && count > 0)
            {
              device->bleBattery = char_elem.char_handle;
              logger::debugln("BLE found battery characteristic, handle: 0x%d", char_elem.char_handle);
              // 注册特征NOTIFY
              esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleBattery);
              logger::debugln("BLE registered for battery notifications");
            }
            else
            {
              logger::warnln("BLE battery characteristic not found, status: %d", status);
            }
          }
        }

        // 2. 查询音频服务的三个特征
        if (device->bleAudioStart != 0 && device->bleAudioEnd != 0)
        {
          // 2.1 查询数据特征 (0x2B81)
          if (device->bleData == 0)
          {
            esp_bt_uuid_t data_char_uuid = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = DATA_CHARACTERISTIC_UUID}};
            status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                    param->search_cmpl.conn_id,
                                                    device->bleAudioStart,
                                                    device->bleAudioEnd,
                                                    data_char_uuid,
                                                    &char_elem, &count);
            if (status == ESP_GATT_OK && count > 0)
            {
              device->bleData = char_elem.char_handle;
              logger::debugln("BLE found data characteristic, handle: 0x%d", char_elem.char_handle);
              esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleData);
              logger::debugln("BLE registered for data notifications");
            }
            else
            {
              logger::warnln("BLE data characteristic not found, status: %d", status);
            }
          }

          // 2.2 查询配置控制特征 (0x2B7A)
          if (device->bleConfigControl == 0)
          {
            esp_bt_uuid_t config_control_char_uuid = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = CONFIG_CONTROL_CHARACTERISTIC_UUID}};
            status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                    param->search_cmpl.conn_id,
                                                    device->bleAudioStart,
                                                    device->bleAudioEnd,
                                                    config_control_char_uuid,
                                                    &char_elem, &count);
            if (status == ESP_GATT_OK && count > 0)
            {
              device->bleConfigControl = char_elem.char_handle;
              logger::debugln("BLE found config control characteristic, handle: 0x%d", char_elem.char_handle);
              esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleConfigControl);
              logger::debugln("BLE registered for config control notifications");
            }
            else
            {
              logger::warnln("BLE config control characteristic not found, status: %d", status);
            }
          }

          // 2.3 查询音频控制特征 (0x2B7B)
          if (device->bleAudioControl == 0)
          {
            esp_bt_uuid_t audio_control_char_uuid = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = AUDIO_CONTROL_CHARACTERISTIC_UUID}};
            status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                    param->search_cmpl.conn_id,
                                                    device->bleAudioStart,
                                                    device->bleAudioEnd,
                                                    audio_control_char_uuid,
                                                    &char_elem, &count);
            if (status == ESP_GATT_OK && count > 0)
            {
              device->bleAudioControl = char_elem.char_handle;
              logger::debugln("BLE found audio control characteristic, handle: 0x%d", char_elem.char_handle);
              esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleAudioControl);
              logger::debugln("BLE registered for audio control notifications");
            }
            else
            {
              logger::warnln("BLE audio control characteristic not found, status: %d", status);
            }
          }
        }
      }
      break;
    }

    // 特征NOTIFY注册成功后事件
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    {
      if (param->reg_for_notify.status != ESP_GATT_OK)
      {
        logger::warnln("BLE register for notify fail!");
      }
      else
      {
        logger::debugln("BLE register for notify success");
        uint16_t connection = -1;
        uint16_t service_start = -1;
        uint16_t service_end = -1;
        uint16_t handle = param->reg_for_notify.handle;
        uint16_t notify_en = 0x0000;
        // 寻找对应的设备
        for (auto &pair : devices)
        {
          DeviceConnection &device = pair.second;
          if (device.bleBattery == handle)
          {
            service_start = device.bleBatteryStart;
            service_end = device.bleBatteryEnd;
            connection = device.bleConnectionID;
            notify_en = 0x0001;
            break;
          }
          else if (device.bleData == handle)
          {
            service_start = device.bleAudioStart;
            service_end = device.bleAudioEnd;
            connection = device.bleConnectionID;
            notify_en = 0x0001;
            break;
          }
          else if (device.bleConfigControl == handle)
          {
            service_start = device.bleAudioStart;
            service_end = device.bleAudioEnd;
            connection = device.bleConnectionID;
            notify_en = 0x0002;
            break;
          }
          else if (device.bleAudioControl == handle)
          {
            service_start = device.bleAudioStart;
            service_end = device.bleAudioEnd;
            connection = device.bleConnectionID;
            notify_en = 0x0002;
            break;
          }
        }

        if (connection != -1)
        {
          uint16_t count = 0;
          esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if, connection, ESP_GATT_DB_DESCRIPTOR,
                                                                      service_start, service_end, handle, &count);
          if (ret_status != ESP_GATT_OK)
          {
            logger::warnln("esp_ble_gattc_get_attr_count error");
            break;
          }
          if (count > 0)
          {
            esp_gattc_descr_elem_t *descr_elem_result = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
            if (!descr_elem_result)
            {
              logger::warnln("malloc error, gattc no mem");
              break;
            }

            const esp_bt_uuid_t cccd_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};
            ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if, connection, handle, cccd_uuid, descr_elem_result, &count);
            if (ret_status != ESP_GATT_OK)
            {
              logger::warnln("esp_ble_gattc_get_descr_by_char_handle error");
              free(descr_elem_result);
              descr_elem_result = NULL;
              break;
            }

            esp_err_t esp_status = esp_ble_gattc_write_char_descr(gattc_if, connection, descr_elem_result[0].handle,
                                                                  sizeof(notify_en), (uint8_t *)&notify_en,
                                                                  ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            if (esp_status != ESP_OK)
            {
              logger::warnln("esp_ble_gattc_write_char_descr error");
            }

            free(descr_elem_result);
            logger::debugln("BLE write CCCD success.");
          }
        }
      }
      break;
    }

    // 收到NOTIFY事件
    case ESP_GATTC_NOTIFY_EVT:
    {
      std::string address = bleBdaToStr(param->notify.remote_bda);
      if (devices.contains(address))
      {
        DeviceConnection &device = devices[address];
        uint16_t handle = param->notify.handle;
        if (param->notify.is_notify)
        {
          if (handle == device.bleData)
          {
            ble_data_handler(device, param->notify.value, param->notify.value_len);
          }
          else if (handle == device.bleBattery)
          {
            ble_battery_handler(device, param->notify.value, param->notify.value_len);
          }
        }
        else
        {
          if (handle == device.bleConfigControl)
          {
            ble_config_control_handler(device, param->notify.value, param->notify.value_len);
          }
          else if (handle == device.bleAudioControl)
          {
            ble_audio_control_handler(device, param->notify.value, param->notify.value_len);
          }
        }
      }
      break;
    }

    // 其他事件
    default:
    {
      break;
    }
    }
  }
}

// 连接设备
static bool ble_connect_to_device(const std::string &address)
{
  if (bleGattcInterface != ESP_GATT_IF_NONE)
  {
    if (devices.contains(address))
    {
      DeviceConnection &device = devices[address];
      esp_ble_gattc_open(bleGattcInterface, device.bleAddress, BLE_ADDR_TYPE_PUBLIC, true);
      logger::debugln("BLE connecting to server %s.", address.c_str());
      return true;
    }
    logger::warnln("BLE connect failed, because not found address %s.", address);
    return false;
  }
  logger::warnln("BLE connect failed, because not get gattc_if.");
  return false;
}

static bool ble_close()
{
  if (bleIsOpen)
  {
    // 停止扫描
    if (bleScanning)
    {
      esp_ble_gap_stop_scanning();
      bleScanning = false;
    }

    // 清空设备列表
    devices.clear();

    // 反注册GATTC应用
    esp_ble_gattc_app_unregister(bleGattcInterface);

    // 禁用Bluedroid
    esp_err_t ret;
    ret = esp_bluedroid_disable();
    if (ret != ESP_OK)
    {
      logger::warnln("BLE bluedroid disable failed: %s", esp_err_to_name(ret));
    }
    ret = esp_bluedroid_deinit();
    if (ret != ESP_OK)
    {
      logger::warnln("BLE bluedroid deinit failed: %s", esp_err_to_name(ret));
    }

    // 禁用蓝牙控制器
    ret = esp_bt_controller_disable();
    if (ret != ESP_OK)
    {
      logger::warnln("BLE controller disable failed: %s", esp_err_to_name(ret));
    }
    ret = esp_bt_controller_deinit();
    if (ret != ESP_OK)
    {
      logger::warnln("BLE controller deinit failed: %s", esp_err_to_name(ret));
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

  esp_err_t ret;
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  // 初始化蓝牙控制器
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret)
  {
    logger::warnln("BLE controller initialize failed: %s", esp_err_to_name(ret));
    return false;
  }
  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret)
  {
    logger::warnln("BLE controller enable failed: %s", esp_err_to_name(ret));
    return false;
  }

  ret = esp_bluedroid_init();
  if (ret)
  {
    logger::warnln("BLE bluedroid init failed: %s", esp_err_to_name(ret));
    return false;
  }
  ret = esp_bluedroid_enable();
  if (ret)
  {
    logger::warnln("BLE bluedroid enable failed: %s", esp_err_to_name(ret));
    return false;
  }

  // 注册GAP和GATTC回调
  ret = esp_ble_gap_register_callback(gap_event_handler);
  if (ret)
  {
    logger::warnln("BLE gap register failed: %s", esp_err_to_name(ret));
    return false;
  }

  ret = esp_ble_gattc_register_callback(gattc_event_handler);
  if (ret)
  {
    logger::warnln("BLE gattc register failed: %s", esp_err_to_name(ret));
    return false;
  }

  // 注册应用配置文件
  ret = esp_ble_gattc_app_register(bleGattcId);
  if (ret)
  {
    logger::warnln("BLE gattc app register failed: %s", esp_err_to_name(ret));
    return false;
  }

  esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(517);
  if (local_mtu_ret)
  {
    logger::warnln("BLE set local  MTU failed, error code = %x", local_mtu_ret);
  }

  bleIsOpen = true;

  esp_ble_gap_start_scanning(30);
  bleScanning = true;

  logger::debugln("BLE is started.");
  return true;
}
/****************************/

void rf::setup()
{
  ble_open();
}