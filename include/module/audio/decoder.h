#pragma once

#include "cstdint"

#define AUDIO_DECODER_ON GPIO_NUM_1

#define AUDIO_DECODER_WS GPIO_NUM_38
#define AUDIO_DECODER_CLK GPIO_NUM_40
#define AUDIO_DECODER_SD GPIO_NUM_39
#define AUDIO_DECODER_MUTE GPIO_NUM_48
#define AUDIO_DECODER_FLT GPIO_NUM_47

#define AUDIO_DECODER_POLLING_CYCLE 4 // ms 决定了扬声器的延迟
#define AUDIO_DECODER_LOSS_FADE_MS 2  // 丢包静音及恢复淡变时长，避免PCM断点爆音
#define AUDIO_DECODER_DMA_DESC_NUM 8  // 192kHz时保留8ms DMA缓存
#define AUDIO_DECODER_DMA_FRAME_NUM 192
#define AUDIO_DECODER_RATE 192000     // 最大频率
#define AUDIO_DECODER_BIT 32          // 固定的比特数
#define AUDIO_DECODER_CHANNEL 2       // 固定的通道数

namespace audio::decoder
{
  void setup();
  bool on(AudioRate rate, AudioBit bit, AudioChannel channel);
  void off();
  bool isOn();

  // 设置静音
  void setMute(bool on);
  // 设置为低延迟
  void setFLT(bool on);
}
