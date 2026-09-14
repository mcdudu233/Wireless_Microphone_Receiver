#include "config.h"
#include "logger.h"
#include "module/audio/buffer.h"

// 循环缓冲区。data_pointer始终指向下一个待写槽位。
static AudioData *data;
static uint8_t data_pointer;
static uint8_t buffered_slots;
static bool buffer_has_data;

// 使用互斥锁保护缓冲区及重组状态。
static SemaphoreHandle_t mutex = NULL;

static uint32_t decoderLastNumber;
static bool decoderStarted;
static bool decoderDelayFlag;
static uint32_t usbLastNumber;
static bool usbStarted;
static bool usbDelayFlag;

#ifdef BUILD_DEBUG
static AudioBufferDebugStats debug_stats = {};
#endif

uint32_t audio::buffer::getFrameSize()
{
  const uint64_t bytes = static_cast<uint64_t>(config::config.audio.rate) *
                         static_cast<uint64_t>(config::config.audio.bit) *
                         static_cast<uint64_t>(config::config.audio.channel) *
                         AUDIO_DECODER_POLLING_CYCLE / 8000U;
  return bytes <= AUDIO_BUFFER_MAX_DATA_SIZE ? static_cast<uint32_t>(bytes) : 0;
}

// 获取过去的第N个数据(不带缓冲)。
static AudioData *getAudioData(uint32_t last)
{
  if (!buffer_has_data || last >= buffered_slots || last >= AUDIO_BUFFER_MAX_BUFFER_SIZE)
  {
    return nullptr;
  }
  return &data[(data_pointer + AUDIO_BUFFER_MAX_BUFFER_SIZE - 1 - last) % AUDIO_BUFFER_MAX_BUFFER_SIZE];
}

static AudioData *getAudioDataFront()
{
  return getAudioData(0);
}

static void initializeAudioData(AudioData *audio, uint32_t number, uint32_t frame_size,
                                uint8_t expected_parts, uint16_t payload_capacity)
{
  audio->num = number;
  audio->size = frame_size;
  audio->received_size = 0;
  audio->received_parts = 0;
  audio->payload_capacity = payload_capacity;
  audio->expected_parts = expected_parts;
  audio->complete = false;
}

static AudioData *appendAudioData(uint32_t number, uint32_t frame_size, uint8_t expected_parts,
                                  uint16_t payload_capacity)
{
  AudioData *audio = &data[data_pointer];
  data_pointer = (data_pointer + 1) % AUDIO_BUFFER_MAX_BUFFER_SIZE;
  if (buffered_slots < AUDIO_BUFFER_MAX_BUFFER_SIZE)
  {
    buffered_slots++;
  }
  buffer_has_data = true;
  initializeAudioData(audio, number, frame_size, expected_parts, payload_capacity);
  return audio;
}

static void writePacket(uint16_t packet_size, uint32_t packet_number,
                        uint8_t packet_part, const uint8_t *packet_data,
                        uint16_t payload_capacity)
{
  const uint32_t frame_size = audio::buffer::getFrameSize();
  const uint8_t expected_parts = frame_size > 0
                                     ? static_cast<uint8_t>((frame_size + payload_capacity - 1) / payload_capacity)
                                     : 0;

  xSemaphoreTake(mutex, portMAX_DELAY);
#ifdef BUILD_DEBUG
  debug_stats.rx_parts++;
  debug_stats.rx_bytes += packet_size;
#endif

  if (packet_data == nullptr || frame_size == 0 || expected_parts == 0 || expected_parts > 32 ||
      packet_part >= expected_parts)
  {
#ifdef BUILD_DEBUG
    debug_stats.invalid_parts++;
#endif
    xSemaphoreGive(mutex);
    return;
  }

  const uint32_t offset = static_cast<uint32_t>(payload_capacity) * packet_part;
  const uint16_t expected_part_size = static_cast<uint16_t>(
      (frame_size - offset) > payload_capacity ? payload_capacity : (frame_size - offset));
  if (packet_size != expected_part_size || offset + packet_size > AUDIO_BUFFER_MAX_DATA_SIZE)
  {
#ifdef BUILD_DEBUG
    debug_stats.invalid_parts++;
#endif
    xSemaphoreGive(mutex);
    return;
  }

  if (decoderStarted && static_cast<int32_t>(packet_number - decoderLastNumber) <= 0)
  {
#ifdef BUILD_DEBUG
    debug_stats.late_parts++;
#endif
  }

  AudioData *audio = nullptr;
  if (!buffer_has_data)
  {
    audio = appendAudioData(packet_number, frame_size, expected_parts, payload_capacity);
  }
  else
  {
    const uint32_t now_number = getAudioDataFront()->num;
    const int32_t delta = static_cast<int32_t>(packet_number - now_number);
    if (delta > 0)
    {
#ifdef BUILD_DEBUG
      debug_stats.gap_frames += static_cast<uint32_t>(delta - 1);
#endif
      const uint32_t slots_to_add = static_cast<uint32_t>(delta) > AUDIO_BUFFER_MAX_BUFFER_SIZE
                                        ? AUDIO_BUFFER_MAX_BUFFER_SIZE
                                        : static_cast<uint32_t>(delta);
      const uint32_t first_number = packet_number - slots_to_add + 1;
      for (uint32_t index = 0; index < slots_to_add; index++)
      {
        audio = appendAudioData(first_number + index, frame_size, expected_parts, payload_capacity);
      }
    }
    else
    {
      const uint32_t age = now_number - packet_number;
      audio = getAudioData(age);
    }
  }

  if (audio == nullptr)
  {
#ifdef BUILD_DEBUG
    debug_stats.late_parts++;
#endif
    xSemaphoreGive(mutex);
    return;
  }

  // 配置切换时同一槽位可能仍保留旧格式，收到新分片后按新格式重新初始化。
  if (audio->size != frame_size || audio->expected_parts != expected_parts ||
      audio->payload_capacity != payload_capacity)
  {
    initializeAudioData(audio, packet_number, frame_size, expected_parts, payload_capacity);
  }

  const uint32_t part_mask = 1UL << packet_part;
  if ((audio->received_parts & part_mask) != 0)
  {
#ifdef BUILD_DEBUG
    debug_stats.duplicate_parts++;
#endif
    xSemaphoreGive(mutex);
    return;
  }

  memcpy(audio->data + offset, packet_data, packet_size);
  audio->received_size += packet_size;
  audio->received_parts |= part_mask;

  const uint32_t complete_mask = expected_parts == 32 ? UINT32_MAX : ((1UL << expected_parts) - 1UL);
  if (!audio->complete && audio->received_size == audio->size && audio->received_parts == complete_mask)
  {
    audio->complete = true;
#ifdef BUILD_DEBUG
    debug_stats.completed_frames++;
#endif
  }
  xSemaphoreGive(mutex);
}

void audio::buffer::writeWiFiPacket(WiFiAudioPacket *packet)
{
  if (packet != nullptr)
  {
    writePacket(packet->size, packet->number, packet->part, packet->data,
                PACKET_WIFI_AUDIO_DATA_MAX_SIZE);
  }
}

void audio::buffer::writeBLEPacket(BLEAudioPacket *packet)
{
  if (packet != nullptr)
  {
    writePacket(packet->size, packet->number, packet->part, packet->data,
                PACKET_BLE_AUDIO_DATA_MAX_SIZE);
  }
}

static AudioData *getConsumerData(uint32_t &last_number, bool &started, bool &delay_flag,
                                  bool decoder_consumer)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  AudioData *audio = nullptr;

  if (!started)
  {
    if (buffered_slots > AUDIO_BUFFER_DELAY_PACKET)
    {
      audio = getAudioData(AUDIO_BUFFER_DELAY_PACKET);
      if (audio != nullptr)
      {
        last_number = audio->num;
        started = true;
      }
    }
  }
  else
  {
    AudioData *front = getAudioDataFront();
    if (front != nullptr)
    {
      const uint32_t next_number = last_number + 1;
      const int32_t available_ahead = static_cast<int32_t>(front->num - next_number);
      if (available_ahead < 0)
      {
        delay_flag = true;
      }
      else if (delay_flag && available_ahead < AUDIO_BUFFER_REBUFFER_PACKET)
      {
        // 欠载后只积累短余量；长期速率偏差由解码端I2S时钟同步处理。
      }
      else
      {
        delay_flag = false;
        audio = getAudioData(static_cast<uint32_t>(front->num - next_number));
        if (audio == nullptr)
        {
          audio = getAudioData(AUDIO_BUFFER_DELAY_PACKET);
          if (audio != nullptr)
          {
#ifdef BUILD_DEBUG
            if (decoder_consumer)
            {
              debug_stats.overrun_frames += audio->num - last_number - 1;
            }
#endif
          }
        }
        if (audio != nullptr)
        {
          last_number = audio->num;
        }
      }
    }
  }

#ifdef BUILD_DEBUG
  if (decoder_consumer && audio != nullptr && !audio->complete)
  {
    debug_stats.incomplete_playouts++;
    const uint32_t expected_mask = audio->expected_parts == 32
                                       ? UINT32_MAX
                                       : ((1UL << audio->expected_parts) - 1UL);
    debug_stats.missing_parts += static_cast<uint32_t>(
        __builtin_popcount(expected_mask & ~audio->received_parts));
  }
#else
  (void)decoder_consumer;
#endif
  xSemaphoreGive(mutex);
  return audio;
}

AudioData *audio::buffer::getDecoderData()
{
  return getConsumerData(decoderLastNumber, decoderStarted, decoderDelayFlag, true);
}

AudioData *audio::buffer::getUSBData()
{
  return getConsumerData(usbLastNumber, usbStarted, usbDelayFlag, false);
}

uint32_t audio::buffer::getDecoderDepth()
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  AudioData *front = getAudioDataFront();
  const uint32_t depth = decoderStarted && front != nullptr
                             ? static_cast<uint32_t>(front->num - decoderLastNumber)
                             : buffered_slots;
  xSemaphoreGive(mutex);
  return depth;
}

#ifdef BUILD_DEBUG
void audio::buffer::getDebugStats(AudioBufferDebugStats &stats)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  stats = debug_stats;
  AudioData *front = getAudioDataFront();
  stats.depth = decoderStarted && front != nullptr
                    ? static_cast<uint32_t>(front->num - decoderLastNumber)
                    : buffered_slots;
  debug_stats = {};
  xSemaphoreGive(mutex);
}
#endif

void audio::buffer::restart()
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < AUDIO_BUFFER_MAX_BUFFER_SIZE; i++)
  {
    data[i].num = 0;
    data[i].size = 0;
    data[i].received_size = 0;
    data[i].received_parts = 0;
    data[i].payload_capacity = 0;
    data[i].expected_parts = 0;
    data[i].complete = false;
  }
  data_pointer = 0;
  buffered_slots = 0;
  buffer_has_data = false;
  decoderLastNumber = 0;
  decoderStarted = false;
  decoderDelayFlag = false;
  usbLastNumber = 0;
  usbStarted = false;
  usbDelayFlag = false;
#ifdef BUILD_DEBUG
  debug_stats = {};
#endif
  xSemaphoreGive(mutex);
}

void audio::buffer::setup()
{
  mutex = xSemaphoreCreateMutex();
  data = static_cast<AudioData *>(heap_caps_malloc(sizeof(AudioData) * AUDIO_BUFFER_MAX_BUFFER_SIZE,
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT));
  if (mutex == nullptr || data == nullptr)
  {
    LOGGER_ERROR("Audio Buffer allocation failed.");
    return;
  }
  restart();
  LOGGER_INFO("Audio Buffer is started.");
}
