#pragma once

#define RF_MAX_CONNECTION 4
// 蓝牙
#define BLE_NAME "Microphone Transmitter"
#define INFO_SERVICE_UUID "180a"
#define DEVICE_CHARACTERISTIC_UUID "2a00"
#define MODEL_CHARACTERISTIC_UUID "2a24"
#define MANUFACTURER_CHARACTERISTIC_UUID "2a29"
#define BATTERY_SERVICE_UUID 0x180F
#define BATTERY_CHARACTERISTIC_UUID 0x2A19
#define AUDIO_SERVICE_UUID 0x1843
#define DATA_CHARACTERISTIC_UUID 0x2B81
#define CONFIG_CONTROL_CHARACTERISTIC_UUID 0x2B7A
#define AUDIO_CONTROL_CHARACTERISTIC_UUID 0x2B7B
// WIFI
#define WIFI_NAME "Microphone Receiver"
#define WIFI_PASSWORD "Cx^9Xbg5wih3"
#define WIFI_CHANNEL 8
// SOCKET
#define SOCKET_PORT 3333

enum ConfigControlMode
{
  AUDIO_CONTROL_MODE_BLE = 0,
  AUDIO_CONTROL_MODE_WIFI_UDP = 1,
  AUDIO_CONTROL_MODE_WIFI_TCP = 2,
};

struct ConfigControl
{
  bool start = false;
  ConfigControlMode mode = AUDIO_CONTROL_MODE_BLE;
  char name[32] = "";
  char password[32] = "";
  uint32_t ip;
};

struct AudioControl
{
  bool start = false;
  uint8_t channel = 2;
  uint16_t rate = 48000;
  uint8_t bit = 16;
};

struct AudioPacket
{
  uint32_t num;
  uint8_t data[1536];
};

// 设备信息结构体
struct DeviceConnection
{
  // 设备信息
  uint8_t battery;
  // 蓝牙记录
  bool bleConnected;
  bool bleDoConnect;
  uint8_t bleAddress[6];
  uint16_t bleConnectionID;
  // GATT服务
  uint16_t bleBatteryStart;
  uint16_t bleBatteryEnd;
  uint16_t bleAudioStart;
  uint16_t bleAudioEnd;
  // GATT特征
  uint16_t bleBattery;
  uint16_t bleData;
  uint16_t bleAudioControl;
  uint16_t bleConfigControl;
  // WIFI记录
  bool wifiConnected;
  uint32_t wifiConnectionIP;
};

namespace rf
{
  void setup();
}