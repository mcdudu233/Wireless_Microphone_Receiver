#include "config.h"
#include "logger.h"
#include "module/audio/decoder.h"

#include "cctype"
#include "ESP_I2S.h"

static bool powerOn = false;

static I2SClass I2S;
static uint32_t i2s_rate;
static i2s_data_bit_width_t i2s_bit;

// 循环缓冲区
static AudioData *data;
static bool data_start;
static uint8_t data_pointer;
static uint32_t data_number;

// 实时处理音频数据
static uint32_t write_number;
static void audioHandle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_AUDIO_DECODER_PERIOD);
  while (true)
  {
    // 启动了芯片才读取数据
    if (powerOn)
    {
      xTaskDelayUntil(&xLastWakeTime, xFrequency);

      if (data_start)
      {
        uint32_t now_number = data[data_pointer - 1].num;
        if (write_number < now_number)
        {
          // TODO 向前写入数据
          I2S.write(data->data, data->size);
          write_number = now_number;
        }
      }
    }
    else
    {
      vTaskDelay(10);
    }
  }
}

void audio::decoder::setup()
{
  logger::debugln("Audio Decoder is starting...");
  data = (AudioData *)heap_caps_malloc(sizeof(AudioData) * AUDIO_ENCODER_MAX_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
  pinMode(AUDIO_DECODER_MUTE, OUTPUT);
  pinMode(AUDIO_DECODER_FLT, OUTPUT);
  digitalWrite(AUDIO_DECODER_MUTE, LOW);
  digitalWrite(AUDIO_DECODER_FLT, LOW);
  I2S.setPins(AUDIO_DECODER_CLK, AUDIO_DECODER_WS, AUDIO_DECODER_SD, -1, -1); // SCK, WS, SDOUT, SDIN, MCLK
  xTaskCreatePinnedToCore(audioHandle, "audio_decoder_handle", TASK_AUDIO_DECODER_STACK, NULL, TASK_AUDIO_DECODER_PRIORITY, NULL, TASK_AUDIO_DECODER_CORE);
  logger::debugln("Audio Decoder is started!");
}

void audio::decoder::on(uint32_t rate, uint32_t bit)
{
  data_start = false;
  data_pointer = 0;
  data_number = 0;
  write_number = 0;
  I2S.begin(I2S_MODE_STD, rate, (i2s_data_bit_width_t)bit, I2S_SLOT_MODE_STEREO);
  i2s_rate = rate;
  i2s_bit = (i2s_data_bit_width_t)bit;
  powerOn = true;
  setMute(false);
  logger::debugln("Audio Decoder is on.");
}

void audio::decoder::off()
{
  setMute(true);
  I2S.end();
  powerOn = false;
  logger::debugln("Audio Decoder is off.");
}

bool audio::decoder::isOn()
{
  return powerOn;
}

void audio::decoder::setMute(bool on)
{
  if (on)
  {
    digitalWrite(AUDIO_DECODER_MUTE, LOW);
  }
  else
  {
    digitalWrite(AUDIO_DECODER_MUTE, HIGH);
  }
}

void audio::decoder::setFLT(bool on)
{
  if (on)
  {
    digitalWrite(AUDIO_DECODER_FLT, HIGH);
  }
  else
  {
    digitalWrite(AUDIO_DECODER_FLT, LOW);
  }
}

bool audio::decoder::writeData(uint8_t *waitData)
{
  size_t size = writeSize();
  AudioData *buffer = &data[data_pointer];
  data_pointer = (data_pointer + 1) % AUDIO_ENCODER_MAX_BUFFER_SIZE;
  buffer->num = data_number++ % UINT32_MAX;
  buffer->size = size;
  memcpy(buffer->data, waitData, size);
  data_start = true;
  return true;
}

size_t audio::decoder::writeSize()
{
  return i2s_rate * i2s_bit * 2 / 8 / 1000;
}
