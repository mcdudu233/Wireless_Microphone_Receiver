#pragma once

#include "BLEDevice.h"

// 蓝牙UUID
#define INFO_SERVICE_UUID "180a"
#define DEVICE_CHARACTERISTIC_UUID "2a00"
#define MODEL_CHARACTERISTIC_UUID "2a24"
#define MANUFACTURER_CHARACTERISTIC_UUID "2a29"
#define BATTERY_SERVICE_UUID "180f"
#define BATTERY_CHARACTERISTIC_UUID "2a19"
#define AUDIO_SERVICE_UUID "1843"
#define DATA_CHARACTERISTIC_UUID "2b81"
#define CONFIG_CONTROL_CHARACTERISTIC_UUID "2b7a"
#define AUDIO_CONTROL_CHARACTERISTIC_UUID "2b7b"
// WIFI
#define WIFI_NAME_VALUE "Microphone Receiver"
#define WIFI_PASSWORD_VALUE "Cx^9Xbg5wih3"
#define WIFI_UDP_PORT 3333

enum ConfigControlMode
{
  AUDIO_CONTROL_MODE_BLE = 0,
  AUDIO_CONTROL_MODE_WIFI = 1,
};

struct ConfigControl
{
  bool start = false;
  bool mode = AUDIO_CONTROL_MODE_BLE;
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
  uint8_t configBattery;
  ConfigControl configBasic;
  AudioControl configAudio;
  // 蓝牙记录
  bool bleConnected;
  bool bleDoConnect;
  BLEClient *bleClient;
  BLEAdvertisedDevice *bleDevice;
  BLERemoteCharacteristic *batteryCharacteristic;
  BLERemoteCharacteristic *dataCharacteristic;
  BLERemoteCharacteristic *configControlCharacteristic;
  BLERemoteCharacteristic *audioControlCharacteristic;
  // WIFI记录
  bool wifiConnected;
};

namespace rf
{
  void setup();
}