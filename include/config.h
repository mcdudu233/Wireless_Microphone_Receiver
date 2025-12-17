#pragma once

#include "cstdint"

// 配置文件名
#define CONFIG_NAME "config"
#define CONFIG_DATA_NAME "config"
#define CONFIG_VERSION_NAME "version"
#define CONFIG_VERSION_VALUE 0x0002 // 前两位大版本号 后两位小版本号

// 多线程任务配置
#define TASK_SYSTEM_CORE 1
#define TASK_SYSTEM_PERIOD 1000
#define TASK_SYSTEM_STACK 3072
#define TASK_SYSTEM_PRIORITY 1

#define TASK_BUTTON_CORE 1
#define TASK_BUTTON_PERIOD 5
#define TASK_BUTTON_STACK 2048
#define TASK_BUTTON_PRIORITY 5

#define TASK_SCREEN_CORE 1
#define TASK_SCREEN_PERIOD 10
#define TASK_SCREEN_STACK 8192
#define TASK_SCREEN_PRIORITY 3

#define TASK_AUDIO_DECODER_CORE 0
#define TASK_AUDIO_DECODER_PERIOD 4
#define TASK_AUDIO_DECODER_STACK 4096
#define TASK_AUDIO_DECODER_PRIORITY 5

#define TASK_RF_CORE 0
#define TASK_RF_PERIOD 1
#define TASK_RF_STACK 4096
#define TASK_RF_PRIORITY 8

namespace config
{
  enum TransmitMode
  {
    TRANSMIT_MODE_BLE = 0,
    TRANSMIT_MODE_WIFI = 1,
  };
  enum USBMode
  {
    USB_MODE_NONE = 0,
    USB_MODE_AUDIO = 1,
    USB_MODE_SD = 2,
  };

  // 全局配置
  struct ConfigValue
  {
    // 音频配置
    struct
    {
      uint8_t channel = 2;
      uint16_t rate = 48000;
      uint8_t bit = 16;
      bool autoVolumn = true;
      bool peekVolumn = false;
      uint8_t volumn = 30;
    } audio;
    // 协议配置
    struct
    {
      TransmitMode mode = TRANSMIT_MODE_WIFI;
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
