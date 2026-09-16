#pragma once

#include "module/rf.h"
#include "cinttypes"
#include "string"
#include "unordered_map"
#include "host/ble_hs.h"

class Device
{
private:
  // 设备唯一ID
  uint8_t id;
  // 设备信息
  uint8_t battery;
  int8_t rssi;
  // 发射端上报的当前实际增益(dB),-1表示尚未上报
  AudioGain gain;
  // 实时音频电平 (0-100)
  uint8_t voiceLevelL;
  uint8_t voiceLevelR;
  // 蓝牙记录
  bool bleConnected;
  uint8_t bleMAC[6]; // 6字节MAC地址
  uint16_t bleHandle;
  ble_l2cap_chan *bleChannel;
  // WIFI记录
  bool wifiConnected;
  uint8_t wifiMAC[6]; // 6字节MAC地址
  uint32_t wifiIP;    // IPv4地址
  // 用户在设备连接页手动断开后置位:抑制"发现即自动连接",
  // 重新进入设备页(ui_bt_init)时统一清除
  bool userUnlinked;

public:
  // 构造函数
  Device();
  Device(uint8_t battery, int8_t rssi,
         bool bleConnected, bool bleDoConnect, uint8_t *bleMAC, uint16_t bleHandle, ble_l2cap_chan *bleChannel,
         bool wifiConnected, uint8_t *wifiMAC, uint32_t wifiIP);
  // 拷贝构造函数
  Device(const Device &other);
  // 移动构造函数
  Device(Device &&other) noexcept;
  // 拷贝赋值运算符
  Device &operator=(const Device &other);
  // 移动赋值运算符
  Device &operator=(Device &&other) noexcept;

  uint8_t getId() const;
  uint8_t getBattery() const;
  void setBattery(uint8_t battery);
  int8_t getRssi() const;
  void setRssi(int8_t rssi);
  // 发射端当前实际增益(dB)
  AudioGain getGain() const;
  void setGain(AudioGain gain);
  uint8_t getVoiceLevelL() const;
  uint8_t getVoiceLevelR() const;
  void setVoiceLevelL(uint8_t level);
  void setVoiceLevelR(uint8_t level);
  bool isBleConnected() const;
  void setBleConnected(bool connected);
  uint8_t *getBleMAC();
  void setBleMAC(uint8_t *mac);
  uint16_t getBleHandle() const;
  void setBleHandle(uint16_t handle);
  ble_l2cap_chan *getBleChannel() const;
  void setBleChannel(ble_l2cap_chan *channel);
  bool isWifiConnected() const;
  void setWifiConnected(bool connected);
  uint8_t *getWifiMAC();
  void setWifiMAC(uint8_t *mac);
  uint32_t getWifiIP() const;
  void setWifiIP(uint32_t ip);
  std::string getBleMACString();
  std::string getWifiMACString();
  std::string getWifiIPString() const;
  // 用户是否在设备页手动断开(抑制发现即自动连接)
  bool isUserUnlinked() const;
  void setUserUnlinked(bool unlinked);

  // 打印设备信息
  void print();
};

class DeviceManager
{
private:
  uint8_t deviceCount = 0;
  Device devices[RF_MAX_CONNECTION];
  std::unordered_map<int64_t, Device *> bleMACToDevice;
  std::unordered_map<int64_t, Device *> wifiMACToDevice;
  std::unordered_map<uint32_t, Device *> wifiIPToDevice;

public:
  // 加载持久化设备编号表(NVS);在NVS初始化后(config::setup之后)调用一次
  void setupNumbering();

  // 查询/分配设备持久编号(1起):同一MAC跨重启返回相同编号,
  // 新分配的编号立即落盘;表满时淘汰最早条目(其设备下次获得新编号)
  uint8_t getDeviceNumber(const uint8_t bleMAC[6]);
  uint8_t getDeviceNumber(const std::string bleMAC);

  // 添加设备
  Device *addDevice(uint8_t bleMAC[6]);

  // 通过BLE MAC地址获取设备指针
  Device *getDeviceByBleMAC(uint8_t bleMAC[6]);
  Device *getDeviceByBleMAC(const std::string bleMAC);

  // 通过WiFi MAC地址获取设备指针
  Device *getDeviceByWifiMAC(uint8_t wifiMAC[6]);
  Device *getDeviceByWifiMAC(const std::string wifiMAC);

  // 通过WiFi IP地址获取设备指针
  Device *getDeviceByWifiIP(uint32_t wifiIP);

  // 绑定WiFi MAC地址到IP地址
  void bindWifiMACToIP(uint8_t wifiMAC[6], uint32_t wifiIP);

  // 获取所有设备
  Device *getAllDevices();

  // 清空所有设备
  void clear();

  // 获取设备数量
  uint8_t size() const;
};