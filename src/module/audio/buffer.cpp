#include "logger.h"
#include "module/audio/buffer.h"

// 无声音
AudioData *AudioWhiteData;

// 循环缓冲区
static AudioData *data;
static uint8_t data_pointer;

uint8_t audio::buffer::getPointer()
{
  return data_pointer;
}

uint32_t audio::buffer::getNumber()
{
  return getAudioDataFront()->num;
}

// 获取过去的第N个数据(不带缓冲)
static AudioData *getAudioData(uint8_t last)
{
  return &data[(data_pointer + AUDIO_BUFFER_MAX_BUFFER_SIZE - 1 - last) % AUDIO_BUFFER_MAX_BUFFER_SIZE];
}

AudioData *audio::buffer::getAudioDataFront()
{
  return getAudioData(0);
}

AudioData *audio::buffer::getAudioDataFromNumber(uint32_t number)
{
  int32_t now = getAudioDataFront()->num;
  if (number > now)
  {
    return nullptr;
  }
  else if (number == now)
  {
    return getAudioDataFront();
  }
  else
  {
    if ((now - number) >= AUDIO_BUFFER_MAX_BUFFER_SIZE)
    {
      return nullptr;
    }
    return getAudioData(now - number);
  }
}

AudioData *audio::buffer::getAudioDataBufferFront()
{
  return getAudioDataFromNumber(getAudioDataFront()->num - AUDIO_BUFFER_DELAY_PACKET);
}

AudioData *audio::buffer::getAudioDataBufferFromNumber(uint32_t number)
{
  return getAudioDataFromNumber(number - AUDIO_BUFFER_DELAY_PACKET);
}

void audio::buffer::writeWiFiPacket(WiFiAudioPacket *packet)
{
  if (packet->number <= getAudioDataFront()->num)
  {
    // 将分包合在一个音频数据包里
    AudioData *audio = getAudioDataFromNumber(packet->number);
    if (audio != nullptr)
    {
      audio->size += packet->size;
      memcpy(audio->data + PACKET_WIFI_AUDIO_DATA_MAX_SIZE * packet->part, packet->data, packet->size);
    }
  }
  else
  {
    // 接收到新的包 创建一个包
    uint8_t last = packet->number - getAudioDataFront()->num;

    // 如果中间丢了N个包 填充
    AudioData *audio;
    for (uint32_t num = packet->number - last + 1; num <= packet->number; num++)
    {
      audio = &data[data_pointer];
      data_pointer = (data_pointer + 1) % AUDIO_BUFFER_MAX_BUFFER_SIZE;
      audio->num = num;
      audio->size = 0;
    }

    // 写入最新包的数据
    audio->size = packet->size;
    // 这里可能接收到的不是第一个part 可能丢包
    if (packet->part != 0)
    {
      // 前面的part填充为0
      memset(audio->data, 0, (packet->part - 1) * PACKET_WIFI_AUDIO_DATA_MAX_SIZE);
    }
    memcpy(audio->data + packet->part * PACKET_WIFI_AUDIO_DATA_MAX_SIZE, packet->data, packet->size);
  }
}

static uint32_t decoderLastNumber = 0;
AudioData *audio::buffer::getDecoderData()
{
  // 寻找是否有合适的包
  AudioData *audio = getAudioDataBufferFromNumber(decoderLastNumber);
  if (audio != nullptr)
  {
    decoderLastNumber++;
    return audio;
  }

  // 找不到则找最前面的包
  audio = getAudioDataBufferFront();
  if (audio != nullptr)
  {
    decoderLastNumber = audio->num;
    return audio;
  }

  // 都没有返回空
  return nullptr;
}

static uint32_t usbLastNumber = 0;
AudioData *audio::buffer::getUSBData()
{
  // 寻找是否有合适的包
  AudioData *audio = getAudioDataBufferFromNumber(usbLastNumber);
  if (audio != nullptr)
  {
    usbLastNumber++;
    return audio;
  }

  // 找不到则找最前面的包
  audio = getAudioDataBufferFront();
  if (audio != nullptr)
  {
    usbLastNumber = audio->num;
    return audio;
  }

  // 都没有返回空
  return nullptr;
}

void audio::buffer::restart()
{
  for (int i = 0; i < AUDIO_BUFFER_MAX_BUFFER_SIZE; i++)
  {
    data[i].num = 0;
    data[i].size = 0;
  }
  data_pointer = 0;
  decoderLastNumber = 0;
}

void audio::buffer::setup()
{
  data = (AudioData *)heap_caps_malloc(sizeof(AudioData) * AUDIO_BUFFER_MAX_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
  AudioWhiteData = (AudioData *)heap_caps_malloc(sizeof(AudioData), MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
  AudioWhiteData->num = 0;
  AudioWhiteData->size = AUDIO_BUFFER_MAX_DATA_SIZE;
  memset(AudioWhiteData->data, 0, AUDIO_BUFFER_MAX_DATA_SIZE);
  restart();
  logger::debugln("Audio Buffer is started.");
}