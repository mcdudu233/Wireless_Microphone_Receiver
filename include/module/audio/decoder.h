#pragma once

#include "cstdint"
#define AUDIO_DECODER_WS GPIO_NUM_38
#define AUDIO_DECODER_CLK GPIO_NUM_40
#define AUDIO_DECODER_SD GPIO_NUM_39

#define AUDIO_DECODER_MUTE GPIO_NUM_48
#define AUDIO_DECODER_FLT GPIO_NUM_47

#define AUDIO_DECODER_POLLING_CYCLE 3 // ms 决定了扬声器的延迟
#define AUDIO_DECODER_RATE 192000     // 最大频率
#define AUDIO_DECODER_BIT 32          // 固定的比特数
#define AUDIO_DECODER_CHANNEL 2       // 固定的通道数

#define AUDIO_DECODER_MAX_DATA_SIZE (AUDIO_DECODER_RATE * AUDIO_DECODER_BIT * AUDIO_DECODER_CHANNEL / 8 * AUDIO_DECODER_POLLING_CYCLE / 1000)
#define AUDIO_DECODER_MAX_BUFFER_SIZE 10

struct AudioData
{
  uint32_t num;
  uint32_t size;
  uint8_t data[AUDIO_DECODER_MAX_DATA_SIZE];
};

namespace audio::decoder
{
  void setup();
  void on(uint32_t rate = 192 * 1000, uint32_t bit = 32);
  void off();
  bool isOn();

  // 设置静音
  void setMute(bool on);
  // 设置为低延迟
  void setFLT(bool on);

  // 写入音频数据
  uint32_t getDataNumber();
  bool writeData(uint8_t *waitData);
  size_t writeSize();
}