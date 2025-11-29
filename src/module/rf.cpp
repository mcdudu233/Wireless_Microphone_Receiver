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
static bool ble_scanning = false;
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

// GAP事件处理
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
      std::string device_name = "";

      // 解析设备名称
      uint8_t name_len = 0;
      uint8_t *name_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &name_len);
      if (name_data == NULL)
      {
        // 如果没有完整的设备名称，尝试获取缩短的设备名称
        name_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &name_len);
      }
      if (name_data != NULL && name_len > 0)
      {
        device_name = std::string((char *)name_data, name_len);
      }

      if (device_name.find(BLE_NAME) != std::string::npos)
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
          logger::debugln("BLE found device: %s, address: %s", device_name.c_str(), address.c_str());

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
      ble_scanning = true;
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
  if (gattc_if == ESP_GATT_IF_NONE || gattc_if == bleGattcInterface)
  {
    switch (event)
    {
    // 注册事件
    case ESP_GATTC_REG_EVT:
    {
      if (param->reg.app_id == bleGattcId && param->reg.status == ESP_GATT_OK)
      {
        bleGattcInterface = gattc_if;
      }
      else
      {
        logger::warnln("BLE register app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
      }
      break;
    }

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
            logger::debugln("BLE found battery service.");
            device->bleBatteryStart = param->search_res.start_handle;
            device->bleBatteryEnd = param->search_res.end_handle;
            break;
          }
          case AUDIO_SERVICE_UUID:
          {
            logger::debugln("BLE found audio service.");
            device->bleAudioStart = param->search_res.start_handle;
            device->bleAudioEnd = param->search_res.end_handle;
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
          esp_bt_uuid_t battery_char_uuid = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = BATTERY_CHARACTERISTIC_UUID}};
          status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                  param->search_cmpl.conn_id,
                                                  device->bleAudioStart,
                                                  device->bleBatteryEnd,
                                                  battery_char_uuid,
                                                  &char_elem, &count);
          if (status == ESP_GATT_OK && count > 0)
          {
            device->bleBattery = char_elem.char_handle;
            logger::debugln("BLE found battery characteristic, handle: 0x%04x", char_elem.char_handle);
          }
          else
          {
            logger::warnln("BLE battery characteristic not found, status: %d", status);
          }
        }

        // 2. 查询音频服务的三个特征
        if (device->bleAudioStart != 0 && device->bleAudioEnd != 0)
        {
          // 2.1 查询数据特征 (0x2B81)
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
            logger::debugln("BLE found data characteristic, handle: 0x%04x", char_elem.char_handle);
          }
          else
          {
            logger::warnln("BLE data characteristic not found, status: %d", status);
          }

          // 2.2 查询配置控制特征 (0x2B7A)
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
            logger::debugln("BLE found config control characteristic, handle: 0x%04x", char_elem.char_handle);
          }
          else
          {
            logger::warnln("BLE config control characteristic not found, status: %d", status);
          }

          // 2.3 查询音频控制特征 (0x2B7B)
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
            logger::debugln("BLE found audio control characteristic, handle: 0x%04x", char_elem.char_handle);
          }
          else
          {
            logger::warnln("BLE audio control characteristic not found, status: %d", status);
          }
        }

        // 为所有4个特征注册通知
        if (device->bleBattery != 0)
        {
          esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleBattery);
          logger::debugln("BLE registered for battery notifications");
        }
        if (device->bleData = 0)
        {
          esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleData);
          logger::debugln("BLE registered for data notifications");
        }
        if (device->bleConfigControl != 0)
        {
          esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleConfigControl);
          logger::debugln("BLE registered for config control notifications");
        }
        if (device->bleAudioControl != 0)
        {
          esp_ble_gattc_register_for_notify(gattc_if, device->bleAddress, device->bleAudioControl);
          logger::debugln("BLE registered for audio control notifications");
        }
        logger::debugln("BLE all notifications registered successfully");
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
        uint16_t handle = param->reg_for_notify.handle;
        // 寻找对应的设备
        for (auto &pair : devices)
        {
          DeviceConnection &device = pair.second;
          if (device.bleBattery == handle)
          {
            connection = device.bleConnectionID;
            break;
          }
          else if (device.bleData == handle)
          {
            connection = device.bleConnectionID;
            break;
          }
          else if (device.bleConfigControl == handle)
          {
            connection = device.bleConnectionID;
            break;
          }
          else if (device.bleAudioControl == handle)
          {
            connection = device.bleConnectionID;
            break;
          }
        }
        // 写入描述符
        // ret_status = esp_ble_gattc_write_char_descr(gattc_if,
        //                                             connection,
        //                                             handle,
        //                                             sizeof(notify_en),
        //                                             (uint8_t *)&notify_en,
        //                                             ESP_GATT_WRITE_TYPE_RSP,
        //                                             ESP_GATT_AUTH_REQ_NONE);
        if (connection != -1)
        {
          // 查找 CCCD 描述符
          esp_gattc_descr_elem_t descr_elem_result[1];
          uint16_t count = 1;
          esp_bt_uuid_t cccd_uuid = {
              .len = ESP_UUID_LEN_16,
              .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};

          esp_gatt_status_t status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                            connection,
                                                                            handle,
                                                                            cccd_uuid, descr_elem_result, &count);

          // if (status == ESP_GATT_OK && count > 0)
          // {
          //   uint16_t cccd_handle = descr_elem_result[0].handle;
          //   uint16_t notify_en;

          //   if (is_indicate)
          //   {
          //     notify_en = 0x0002; // 启用 Indicate
          //     logger::debugln("Enabling INDICATE for char 0x%04x, CCCD: 0x%04x", char_handle, cccd_handle);
          //   }
          //   else
          //   {
          //     notify_en = 0x0001; // 启用 Notify
          //     logger::debugln("Enabling NOTIFY for char 0x%04x, CCCD: 0x%04x", char_handle, cccd_handle);
          //   }

          //   // 使用您提供的写入方式
          //   esp_err_t ret_status = esp_ble_gattc_write_char_descr(gl_profile.gattc_if,
          //                                                         device->conn_id,
          //                                                         descr_elem_result[0].handle,
          //                                                         sizeof(notify_en),
          //                                                         (uint8_t *)&notify_en,
          //                                                         ESP_GATT_WRITE_TYPE_RSP,
          //                                                         ESP_GATT_AUTH_REQ_NONE);

          //   if (ret_status == ESP_OK)
          //   {
          //     device->pending_configurations++;
          //     logger::debugln("CCCD write request sent for char 0x%04x", char_handle);
          //   }
          //   else
          //   {
          //     logger::warnln("Failed to write CCCD for char 0x%04x: %s", char_handle, esp_err_to_name(ret_status));
          //   }
          // }
          // else
          // {
          //   logger::warnln("CCCD not found for char 0x%04x, status: %d", char_handle, status);
          // }
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
          }
          else if (handle == device.bleBattery)
          {
            device.battery = param->notify.value[0];
            logger::debugln("BLE battery level: %d", param->notify.value[0]);
          }
        }
        else
        {
          if (handle == device.bleConfigControl)
          {
            if (param->notify.value_len == sizeof(ConfigControl))
            {

              ConfigControl *src = (ConfigControl *)param->notify.value;
              configBasic.start = src->start;
              configBasic.mode = src->mode;
              configBasic.ip = src->ip;
              strcpy(configBasic.name, src->name);
              strcpy(configBasic.password, src->password);
              logger::debugln("BLE get indicate basic config.");
              // 发送 Indicate 确认
              // esp_gatt_rsp_t rsp;
              // memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
              // rsp.attr_value.handle = p_data->notify.handle;
              // esp_err_t ret = esp_ble_gattc_send_cmd(gattc_if, param->notify.conn_id, ESP_GATT_RSP_MTU, &rsp);
            }
          }
          else if (handle == device.bleAudioControl)
          {
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

static bool ble_close()
{
  if (bleIsOpen)
  {
    // 停止扫描
    if (ble_scanning)
    {
      esp_ble_gap_stop_scanning();
      ble_scanning = false;
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
  ble_scanning = true;

  logger::debugln("BLE is started.");
  return true;
}
/****************************/

void rf::setup()
{
  ble_open();
}