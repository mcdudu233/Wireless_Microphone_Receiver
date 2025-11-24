#include "logger.h"
#include "config.h"
#include "module/rf.h"

#include "map"
#include "iostream"
#include "random"
#include "string"
#include "algorithm"
#include "BLEDevice.h"
#include "WiFi.h"
#include "NetworkUdp.h"

// 记录所有设备
static std::map<std::string, DeviceConnection> devices;

// WIFI
static std::string generateRandomWord(int length);
static bool wifiOn = false;
static const char *wifiName = WIFI_NAME_VALUE;
static const char *wifiPassword = WIFI_PASSWORD_VALUE;
NetworkUDP wifiServer;

// Client callback class to handle connect/disconnect events
class BLEClientCallback : public BLEClientCallbacks
{
  std::string serverAddress;

public:
  BLEClientCallback(std::string index) : serverAddress(index) {}

  void onConnect(BLEClient *pclient)
  {
    // servers[serverAddress].connected = true;
    logger::debugln("BLE connected to server %s.", serverAddress);
  }

  void onDisconnect(BLEClient *pclient)
  {
    devices[serverAddress].bleConnected = false;
    logger::debugln("BLE disconnected from server %s.", serverAddress);
  }
};

// 扫描到蓝牙设备
class BLEAdvertisedDeviceCallback : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice pDevice)
  {
    // logger::debugln("BLE Device found: %s", advertisedDevice.toString().c_str());
    if (pDevice.getName().equalsIgnoreCase("Microphone Transmitter"))
    {
      std::string address = pDevice.getAddress().toString().c_str();
      if (devices.contains(address))
      {
        // 如果已经连接
        logger::debugln("BLE already connected or connecting to this device {name=%s, address=%s}.", pDevice.getName(), address);
      }
      else
      {
        // 设备加入待连接列表
        devices.insert(std::pair<std::string, DeviceConnection>(address, (DeviceConnection){
                                                                             .bleDoConnect = true,
                                                                             .bleDevice = new BLEAdvertisedDevice(pDevice),
                                                                         }));
        strcpy(devices.at(address).configBasic.name, wifiName);
        strcpy(devices.at(address).configBasic.password, wifiPassword);

        // BLEDevice::getScan()->start(5, false);
      }
    }
  }
};

// Callback function to handle notifications from any server
static void batteryNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (devices.contains(address))
  {
    devices.at(address).configBattery = *pData;
  }
}
static void configControlIndicateCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (devices.contains(address))
  {
    ConfigControl *src = (ConfigControl *)pData;
    ConfigControl &dst = devices.at(address).configBasic;
    dst.start = src->start;
    dst.mode = src->mode;
    dst.ip = src->ip;
    strcpy(dst.name, src->name);
    strcpy(dst.password, src->password);
    logger::debugln("BLE get indicate config.");
  }
}
static void audioControlIndicateCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (devices.contains(address))
  {
    devices.at(address).configAudio = *(AudioControl *)pData;
  }
}
static unsigned long last_time = millis();
static uint32_t last_num;
static uint32_t packet_size = 0;
static uint32_t packet_num = 0;
static void dataNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (devices.contains(address))
  {
    AudioPacket *packet = (AudioPacket *)pData;
    packet_size += length;
    packet_num += packet->num - last_num - 1;
    last_num = packet->num;
    unsigned long now_time = millis();
    if (now_time - last_time > 1000)
    {
      last_time = now_time;
      logger::debugln("BLE get packet for %dbytes/s and loss for %D.", packet_size, packet_num / 1000.0 * 100.0);
      packet_size = 0;
      packet_num = 0;
    }
  }
}

static void rf_handle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_RF_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    if (wifiOn)
    {
      int size = wifiServer.parsePacket();
      if (size != 0)
      {
        logger::debugln("Received packet from %s:%d.", wifiServer.remoteIP().toString(), wifiServer.remotePort());
        char buffer[255];
        int len = wifiServer.read(buffer, 255);
        if (len > 0)
        {
          buffer[len] = 0;
          Serial.println(buffer);
        }
      }
    }

    // 连接到设备
    for (auto &device : devices)
    {
      const std::string &address = device.first;
      DeviceConnection &connection = device.second;
      if (connection.bleDoConnect)
      {
        logger::debugln("BLE connecting to server %s.", address);
        connection.bleClient = BLEDevice::createClient();
        // Set the callback for this specific server connection
        connection.bleClient->setClientCallbacks(new BLEClientCallback(address));
        // Connect to the remote BLE Server
        connection.bleClient->connect(connection.bleDevice);
        connection.bleClient->setMTU(517); // Request maximum MTU from server
        // Obtain a reference to the service we are after in the remote BLE server
        std::map<std::string, BLERemoteService *> *pRemoteService = connection.bleClient->getServices();
        if (pRemoteService->size() == 0)
        {
          logger::warnln("BLE failed to find service at %s.", address);
          connection.bleClient->disconnect();
          delete connection.bleDevice;
          connection.bleDevice = nullptr;
        }
        else
        {
          std::string batteryServiceUUID = BLEUUID(BATTERY_SERVICE_UUID).to128().toString().c_str();
          std::string audioServiceUUID = BLEUUID(AUDIO_SERVICE_UUID).to128().toString().c_str();
          if (!pRemoteService->contains(batteryServiceUUID) ||
              !pRemoteService->contains(audioServiceUUID))
          {
            logger::warnln("BLE failed to find service UUID at ", address);
            connection.bleClient->disconnect();
            delete connection.bleDevice;
            connection.bleDevice = nullptr;
          }
          else
          {
            std::map<std::string, BLERemoteCharacteristic *> *batteryCharacteristics = pRemoteService->at(batteryServiceUUID)->getCharacteristics();
            std::map<std::string, BLERemoteCharacteristic *> *audioCharacteristics = pRemoteService->at(audioServiceUUID)->getCharacteristics();
            std::string batteryCharacteristicUUID = BLEUUID(BATTERY_CHARACTERISTIC_UUID).to128().toString().c_str();
            std::string dataCharacteristicUUID = BLEUUID(DATA_CHARACTERISTIC_UUID).to128().toString().c_str();
            std::string configControlCharacteristicUUID = BLEUUID(CONFIG_CONTROL_CHARACTERISTIC_UUID).to128().toString().c_str();
            std::string audioControlCharacteristicUUID = BLEUUID(AUDIO_CONTROL_CHARACTERISTIC_UUID).to128().toString().c_str();
            if (!batteryCharacteristics->contains(batteryCharacteristicUUID) ||
                !audioCharacteristics->contains(dataCharacteristicUUID) ||
                !audioCharacteristics->contains(configControlCharacteristicUUID) ||
                !audioCharacteristics->contains(audioControlCharacteristicUUID))
            {
              logger::warnln("BLE failed to find characteristic UUID at ", address);
              connection.bleClient->disconnect();
              delete connection.bleDevice;
              connection.bleDevice = nullptr;
            }
            else
            {
              connection.batteryCharacteristic = batteryCharacteristics->at(batteryCharacteristicUUID);
              connection.batteryCharacteristic->registerForNotify(batteryNotifyCallback);
              connection.dataCharacteristic = audioCharacteristics->at(dataCharacteristicUUID);
              connection.dataCharacteristic->registerForNotify(dataNotifyCallback);
              connection.configControlCharacteristic = audioCharacteristics->at(configControlCharacteristicUUID);
              connection.configControlCharacteristic->registerForNotify(configControlIndicateCallback, false);
              connection.audioControlCharacteristic = audioCharacteristics->at(audioControlCharacteristicUUID);
              connection.audioControlCharacteristic->registerForNotify(audioControlIndicateCallback, false);
              // 读取数据
              connection.configBattery = connection.batteryCharacteristic->readUInt8();

              // 测试连接
              connection.configBasic.start = true;
              connection.configBasic.mode = AUDIO_CONTROL_MODE_WIFI;
              if (!wifiOn)
              {
                logger::debugln("WiFi is starting...");
                WiFi.mode(WIFI_AP);
                if (!WiFi.softAP(wifiName, wifiPassword))
                {
                  WiFi.mode(WIFI_OFF);
                  logger::warnln("WiFi started fail!");
                  break;
                }
                logger::debugln("WiFi is opened for IP %s.", WiFi.softAPIP().toString());
                logger::debugln("WiFi UDP starting...");
                if (!wifiServer.begin(WIFI_UDP_PORT))
                {
                  WiFi.mode(WIFI_OFF);
                  logger::warnln("WiFi UDP started fail!");
                  break;
                }
                logger::debugln("WiFi UDP is started.");
                logger::debugln("WiFi is started.");
                wifiOn;
              }
              connection.configAudio.start = true;
              connection.configControlCharacteristic->writeValue((uint8_t *)&connection.configBasic, sizeof(ConfigControl));
              connection.audioControlCharacteristic->writeValue((uint8_t *)&connection.configAudio, sizeof(AudioControl));

              connection.bleConnected = true;
              logger::debugln("BLE successfully connected to %s.", address);
            }
          }
        }
        connection.bleDoConnect = false;
      }
    }
  }
}

void rf::setup()
{
  logger::debugln("BLE is starting...");
  // 初始化蓝牙
  BLEDevice::init("Microphone Receiver");
  BLEDevice::setMTU(517);

  // 设置蓝牙扫描
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new BLEAdvertisedDeviceCallback());
  pBLEScan->setInterval(3000);
  pBLEScan->setWindow(1000);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  logger::debugln("BLE is started.");

  WiFi.mode(WIFI_OFF);
  logger::debugln("WiFi is off.");

  xTaskCreatePinnedToCore(rf_handle, "rf_handle", TASK_RF_STACK, NULL, TASK_RF_PRIORITY, NULL, TASK_RF_CORE);
}