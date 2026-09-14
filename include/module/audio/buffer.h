#pragma once

#include "cinttypes"
#include "module/rf.h"
#include "module/audio/decoder.h"

#define AUDIO_BUFFER_MAX_DATA_SIZE (AUDIO_DECODER_RATE * AUDIO_DECODER_BIT * AUDIO_DECODER_CHANNEL / 8 * AUDIO_DECODER_POLLING_CYCLE / 1000)
#define AUDIO_BUFFER_MAX_BUFFER_SIZE 250

// 传输延迟(等待多久才把数据输出,数值过低会导致爆音)
// WiFi存在毫秒到几十毫秒级的传输抖动, 缓冲需要足够深避免周期性断续
#define AUDIO_BUFFER_DELAY_PACKET 50                                                      // 延迟多少个数据包
#define AUDIO_BUFFER_DELAY_TIME (AUDIO_BUFFER_DELAY_PACKET * AUDIO_DECODER_POLLING_CYCLE) // 计算得到传输延迟

struct AudioData
{
  uint32_t num;
  // 完整音频帧的期望长度。只有 complete=true 时 data 才可直接播放。
  uint32_t size;
  uint32_t received_size;
  uint32_t received_parts;
  uint16_t payload_capacity;
  uint8_t expected_parts;
  bool complete;
  uint8_t data[AUDIO_BUFFER_MAX_DATA_SIZE];
};

#ifdef BUILD_DEBUG
struct AudioBufferDebugStats
{
  uint32_t rx_parts;
  uint32_t rx_bytes;
  uint32_t completed_frames;
  uint32_t gap_frames;
  uint32_t duplicate_parts;
  uint32_t late_parts;
  uint32_t invalid_parts;
  uint32_t incomplete_playouts;
  uint32_t missing_parts;
  uint32_t overrun_frames;
  uint32_t depth;
};
#endif

namespace audio::buffer
{
  void setup();
  // 重置指针
  void restart();

  /* 写入数据 */
  // 写入音频数据
  void writeWiFiPacket(WiFiAudioPacket *packet);
  void writeBLEPacket(BLEAudioPacket *packet);

  /* 读取数据 */
  // 获取音频解码器的数据
  AudioData *getDecoderData();
  // 获取USB的数据
  AudioData *getUSBData();

  // 当前配置每个4ms音频帧应有的字节数。
  uint32_t getFrameSize();

#ifdef BUILD_DEBUG
  // 读取并清零自上次调用后的重组统计。
  void getDebugStats(AudioBufferDebugStats &stats);
#endif
}

extern AudioData *AudioWhiteData;
