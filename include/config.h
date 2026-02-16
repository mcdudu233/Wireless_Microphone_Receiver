#pragma once

#include "cstdint"

// 配置文件名
#define CONFIG_NAME "config"
#define CONFIG_DATA_NAME "config"
#define CONFIG_VERSION_NAME "version"
#define CONFIG_VERSION_VALUE 0x000B // 前两位大版本号 后两位小版本号

// 多线程任务配置
#define TASK_SYSTEM_CORE 1
#define TASK_SYSTEM_PERIOD 1000
#define TASK_SYSTEM_STACK 2570
#define TASK_SYSTEM_PRIORITY 1

#define TASK_BUTTON_CORE 1
#define TASK_BUTTON_PERIOD 5
#define TASK_BUTTON_STACK 1024
#define TASK_BUTTON_PRIORITY 5

#define TASK_SCREEN_CORE 1
#define TASK_SCREEN_PERIOD 5
#define TASK_SCREEN_STACK 8192
#define TASK_SCREEN_PRIORITY 3

#define TASK_USB_CORE 0
#define TASK_USB_PERIOD 4
#define TASK_USB_STACK 4096
#define TASK_USB_PRIORITY 6

#define TASK_TUSB_CORE 0
#define TASK_TUSB_STACK 4096
#define TASK_TUSB_PRIORITY 8

#define TASK_AUDIO_DECODER_CORE 0
#define TASK_AUDIO_DECODER_PERIOD 4
#define TASK_AUDIO_DECODER_STACK 4096
#define TASK_AUDIO_DECODER_PRIORITY 5

#define TASK_RF_CORE 0
#define TASK_RF_PERIOD 1
#define TASK_RF_STACK 4096
#define TASK_RF_PRIORITY 9

// 射频模式
enum RFMode : uint8_t
{
  RF_MODE_BLE = 1,
  RF_MODE_WIFI = 2
};

// 射频文本
typedef char RFText[16];

// USB 模式
enum USBMode : uint8_t
{
  USB_MODE_NONE = 0,
  USB_MODE_JTAG = 1,
  USB_MODE_AUDIO = 2,
  USB_MODE_SD = 3,
};

// 音频声道
enum AudioChannel : uint8_t
{
  AUDIO_CHANNEL_SINGLE = 1, // 单声道
  AUDIO_CHANNEL_STEREO = 2  // 立体声
};

// 音频采样率
enum AudioRate : uint32_t
{
  AUDIO_RATE_48000 = 48000,
  AUDIO_RATE_96000 = 96000,
  AUDIO_RATE_192000 = 192000
};

// 音频比特
enum AudioBit : uint8_t
{
  AUDIO_BIT_16 = 16,
  AUDIO_BIT_24 = 24,
  AUDIO_BIT_32 = 32
};

// 音频模式
enum AudioMode : uint8_t
{
  AUDIO_MODE_AUTO = 0,
  AUDIO_MODE_PEEK = 1,
  AUDIO_MODE_MANUAL = 2
};

// 音频增益 [-64, 63] 单位: dB
typedef int8_t AudioGain;

namespace config
{

  // 全局配置
  struct ConfigValue
  {
    // 音频配置
    struct
    {
      AudioChannel channel = AUDIO_CHANNEL_SINGLE;
      AudioRate rate = AUDIO_RATE_48000;
      AudioBit bit = AUDIO_BIT_16;
      // 增益模式 自动增益;峰值减少;手动
      AudioMode mode = AUDIO_MODE_AUTO;
      // 增益
      AudioGain gain = 0;
    } audio;
    // 协议配置
    struct
    {
      RFMode mode = RF_MODE_WIFI;
    } rf;
    // USB配置
    struct
    {
      USBMode mode = USB_MODE_AUDIO;
    } usb;
  };
  extern ConfigValue config;

  void setup();
  void save();
}
