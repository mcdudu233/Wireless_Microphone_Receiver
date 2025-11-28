#pragma once

// 配置文件名
#define CONFIG_NAME "config"
#define CONFIG_VALUE_NAME "config"

// 多线程任务配置
#define TASK_BUTTON_CORE 1
#define TASK_BUTTON_PERIOD 5
#define TASK_BUTTON_STACK 4096
#define TASK_BUTTON_PRIORITY 2

#define TASK_AUDIO_DECODER_CORE 0
#define TASK_AUDIO_DECODER_PERIOD 1
#define TASK_AUDIO_DECODER_STACK 4096
#define TASK_AUDIO_DECODER_PRIORITY 2

#define TASK_SCREEN_CORE 1
#define TASK_SCREEN_PERIOD 10
#define TASK_SCREEN_STACK 8192
#define TASK_SCREEN_PRIORITY 1

#define TASK_RF_CORE 1
#define TASK_RF_PERIOD 1
#define TASK_RF_STACK 8192
#define TASK_RF_PRIORITY 1

namespace config
{
  enum TransmitMode
  {
    TRANSMIT_MODE_BLE = 0,
    TRANSMIT_MODE_WIFI_UDP = 1,
    TRANSMIT_MODE_WIFI_TCP = 2,
  };

  // 全局配置
  struct ConfigValue
  {
    TransmitMode transmitMode = TRANSMIT_MODE_WIFI_TCP;
  } value;

  void setup();
}
