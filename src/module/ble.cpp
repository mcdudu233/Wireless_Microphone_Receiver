#include "logger.h"
#include "config.h"
#include "module/ble.h"

#include "map"
#include "BLEDevice.h"

// Structure to hold information about each connected server
struct ServerConnection
{
  BLEClient *pClient;
  BLEAdvertisedDevice *pDevice;
  BLERemoteCharacteristic *batteryCharacteristic;
  BLERemoteCharacteristic *dataCharacteristic;
  BLERemoteCharacteristic *configControlCharacteristic;
  BLERemoteCharacteristic *audioControlCharacteristic;
  uint8_t battery;
  ConfigControl configControl;
  AudioControl audioControl;
  bool connected;
  bool doConnect;
};

static std::map<std::string, ServerConnection> servers;

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
    servers[serverAddress].connected = false;
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
      if (servers.contains(address))
      {
        // 如果已经连接
        logger::debugln("BLE already connected or connecting to this device {name=%s, address=%s}.", pDevice.getName(), address);
      }
      else
      {
        // 设备加入待连接列表
        servers.insert(std::pair<std::string, ServerConnection>(address, (ServerConnection){
                                                                             .pDevice = new BLEAdvertisedDevice(pDevice),
                                                                             .doConnect = true,
                                                                         }));

        // BLEDevice::getScan()->start(5, false);
      }
    }
  }
};

// Callback function to handle notifications from any server
static void batteryNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (servers.contains(address))
  {
    servers.at(address).battery = *pData;
  }
}
static void configControlNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (servers.contains(address))
  {
    servers.at(address).configControl = *(ConfigControl *)pData;
  }
}
static void audioControlNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (servers.contains(address))
  {
    servers.at(address).audioControl = *(AudioControl *)pData;
  }
}
static void dataNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  if (servers.contains(address))
  {
  }
}

static void ble_handle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_BLE_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 连接到设备
    for (auto &server : servers)
    {
      std::string address = server.first;
      ServerConnection &connection = server.second;
      if (connection.doConnect)
      {
        logger::debugln("BLE connecting to server %s.", address);
        connection.pClient = BLEDevice::createClient();
        // Set the callback for this specific server connection
        connection.pClient->setClientCallbacks(new BLEClientCallback(address));
        // Connect to the remote BLE Server
        connection.pClient->connect(connection.pDevice);
        connection.pClient->setMTU(517); // Request maximum MTU from server
        // Obtain a reference to the service we are after in the remote BLE server
        std::map<std::string, BLERemoteService *> *pRemoteService = connection.pClient->getServices();
        if (pRemoteService->size() == 0)
        {
          logger::warnln("BLE failed to find service at %s.", address);
          connection.pClient->disconnect();
          delete connection.pDevice;
          connection.pDevice = nullptr;
        }
        else
        {
          std::string batteryServiceUUID = BLEUUID(BATTERY_SERVICE_UUID).to128().toString().c_str();
          std::string audioServiceUUID = BLEUUID(AUDIO_SERVICE_UUID).to128().toString().c_str();
          if (!pRemoteService->contains(batteryServiceUUID) ||
              !pRemoteService->contains(audioServiceUUID))
          {
            logger::warnln("BLE failed to find service UUID at ", address);
            connection.pClient->disconnect();
            delete connection.pDevice;
            connection.pDevice = nullptr;
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
              connection.pClient->disconnect();
              delete connection.pDevice;
              connection.pDevice = nullptr;
            }
            else
            {
              connection.batteryCharacteristic = batteryCharacteristics->at(batteryCharacteristicUUID);
              connection.batteryCharacteristic->registerForNotify(batteryNotifyCallback);
              connection.dataCharacteristic = audioCharacteristics->at(dataCharacteristicUUID);
              connection.dataCharacteristic->registerForNotify(dataNotifyCallback);
              connection.configControlCharacteristic = audioCharacteristics->at(configControlCharacteristicUUID);
              connection.configControlCharacteristic->registerForNotify(configControlNotifyCallback);
              connection.audioControlCharacteristic = audioCharacteristics->at(audioControlCharacteristicUUID);
              connection.audioControlCharacteristic->registerForNotify(audioControlNotifyCallback);
              // 读取数据
              connection.battery = connection.batteryCharacteristic->readUInt8();
              connection.configControl = *(ConfigControl *)(connection.configControlCharacteristic->readValue().c_str());
              connection.audioControl = *(AudioControl *)(connection.configControlCharacteristic->readValue().c_str());

              connection.connected = true;
              logger::debugln("BLE successfully connected to %s.", address);
            }
          }
        }
        connection.doConnect = false;
      }
    }
  }
}

void ble::setup()
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

  xTaskCreatePinnedToCore(ble_handle, "ble_handle", TASK_BLE_STACK, NULL, TASK_BLE_PRIORITY, NULL, TASK_BLE_CORE);
  logger::debugln("BLE is started.");
}