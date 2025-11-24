#pragma once

#include "cstdint"

#define AUDIO_DECODER_WS 38
#define AUDIO_DECODER_CLK 40
#define AUDIO_DECODER_SD 39

#define AUDIO_DECODER_MUTE 48
#define AUDIO_DECODER_FLT 47

#define AUDIO_ENCODER_MAX_DATA_SIZE 1536
#define AUDIO_ENCODER_MAX_BUFFER_SIZE 10

struct AudioData
{
  uint32_t num;
  uint32_t size;
  uint8_t data[AUDIO_ENCODER_MAX_DATA_SIZE];
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
  bool writeData(uint8_t *waitData);
  size_t writeSize();
}