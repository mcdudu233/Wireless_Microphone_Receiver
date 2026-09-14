#pragma once

#include "config.h"

#define RF_MAX_CONNECTION 4
// 蓝牙
#define BLE_NAME "MicTx"
#define BLE_PACKET_LENGTH 251
#define BLE_PACKET_TIME 2120
#define BLE_ITVL_MIN 6
#define BLE_ITVL_MAX 6
#define BLE_LATENCY 0
#define BLE_SUPERVISION_TIMEOUT 300
#define BLE_CE_LEN_MIN 12
#define BLE_CE_LEN_MAX 30
#define BLE_L2CAP_PSM 0x1001
#define BLE_L2CAP_MTU 512
// WIFI
#define WIFI_NAME "MicRx"
#define WIFI_PASSWORD "Cx^9Xbg5wih3"
#define WIFI_CHANNEL 8
#define WIFI_IP_PROTOCOL 0xE9
#define WIFI_NO_PORT 0
#define WIFI_IP_HEAD_LEN 20
// 每个1ms调度周期最多批量处理的WiFi包数；192k/32bit/立体声约需1.25包/ms。
// 一个AMPDU批次可能同时到达数十个包；应能一次排空整个接收窗口。
#define RF_WIFI_RX_BURST_MAX 32
// ESP-IDF默认RAW netconn接收邮箱只有6项，无法吸收192kHz音频的AMPDU突发。
#define RF_WIFI_RAW_RX_MBOX_SIZE 64

// 主界面音量表：稀疏采样PCM，在固定周期发布dBFS包络。
#define RF_VOICE_METER_SAMPLE_STRIDE 4
#define RF_VOICE_METER_UPDATE_MS 40
#define RF_VOICE_METER_RELEASE_STEP 8

// 断线重连监控
#define RF_RECONNECT_TIMEOUT_MS 10000       // 断开后判定连接失败的宽限期
#define RF_RECONNECT_RETRY_INTERVAL_MS 2000 // BLE模式下主动重连尝试间隔

// 协议切换时等待发射器接入WiFi的最长时间
#define RF_MODE_SWITCH_TIMEOUT_MS 10000

// 客户端状态
#define PACKET_CLIENT_STATUS_SIZE (sizeof(uint8_t) + sizeof(ClientStatusPacket))
enum PacketClientStatus : uint8_t
{
  PACKET_CLIENT_STATUS_OK = 0,
  PACKET_CLIENT_STATUS_ERROR_UNKNOW = 255,
};
struct __attribute__((packed)) ClientStatusPacket
{
  PacketClientStatus status;
  uint8_t battery;
};

// 服务端控制设备
#define PACKET_SERVER_CONTROL_RF_SIZE (sizeof(uint8_t) + sizeof(ServerControlRFPacket))
struct __attribute__((packed)) ServerControlRFPacket
{
  RFMode mode;
  RFText ssid;
  RFText password;
};

// 服务端控制音频
#define PACKET_SERVER_CONTROL_AUDIO_SIZE (sizeof(uint8_t) + sizeof(ServerControlAudioPacket))
struct __attribute__((packed)) ServerControlAudioPacket
{
  bool start;
  AudioChannel channel;
  AudioRate rate;
  AudioBit bit;
  AudioMode mode;
  AudioGain gain;
};

// WIFI传输包
#define PACKET_WIFI_AUDIO_HEAD_SIZE (sizeof(uint8_t) + sizeof(WiFiAudioPacket) - PACKET_WIFI_AUDIO_DATA_MAX_SIZE)
#define PACKET_WIFI_AUDIO_DATA_MAX_SIZE 1420
struct __attribute__((packed)) WiFiAudioPacket
{
  uint16_t size;
  uint32_t number;
  uint8_t part;
  uint8_t data[PACKET_WIFI_AUDIO_DATA_MAX_SIZE];
};

// BLE传输包
#define PACKET_BLE_AUDIO_HEAD_SIZE (sizeof(uint8_t) + sizeof(BLEAudioPacket) - PACKET_BLE_AUDIO_DATA_MAX_SIZE)
#define PACKET_BLE_AUDIO_DATA_MAX_SIZE 384
struct __attribute__((packed)) BLEAudioPacket
{
  uint16_t size;
  uint32_t number;
  uint8_t part;
  uint8_t data[PACKET_BLE_AUDIO_DATA_MAX_SIZE];
};

// 统一协议
enum PacketType : uint8_t
{
  PACKET_TYPE_WIFI_AUDIO = 0,
  PACKET_TYPE_BLE_AUDIO = 1,
  PACKET_TYPE_CLIENT_ACK = 2,
  PACKET_TYPE_SERVER_ACK = 3,
  PACKET_TYPE_CLIENT_STATUS = 4,
  PACKET_TYPE_SERVER_CONTROL_RF = 5,
  PACKET_TYPE_SERVER_CONTROL_AUDIO = 6,
};
struct __attribute__((packed)) Packet
{
  PacketType type; // 标识是哪个包
  union
  {
    // 音频包
    WiFiAudioPacket audioDataWiFi;
    BLEAudioPacket audioDataBLE;
    // 状态配置包
    ClientStatusPacket clientStatus;
    ServerControlRFPacket serverControlRF;
    ServerControlAudioPacket serverControlAudio;
  } packet;
};

namespace rf
{
  void setup();
}
