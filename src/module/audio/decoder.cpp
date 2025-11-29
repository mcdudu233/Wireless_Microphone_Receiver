#include "config.h"
#include "logger.h"
#include "module/audio/decoder.h"

#include "cctype"
#include "ESP_I2S.h"

// I2S配置
static const i2s_chan_config_t i2s_chan_cfg = {
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 10,   // 多少个DMA
    .dma_frame_num = 384, // 每个DMA大小
    .auto_clear_after_cb = true,
    .auto_clear_before_cb = false,
    .allow_pd = false,
    .intr_priority = 0,
};
static const i2s_std_gpio_config_t i2s_gpio_cfg = {
    .mclk = I2S_GPIO_UNUSED,
    .bclk = AUDIO_DECODER_CLK,
    .ws = AUDIO_DECODER_WS,
    .dout = AUDIO_DECODER_SD,
    .din = I2S_GPIO_UNUSED,
    .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
    }};

static i2s_chan_handle_t i2s_tx_handle;
static uint32_t i2s_rate;
static i2s_data_bit_width_t i2s_bit;
static bool powerOn = false;

// 循环缓冲区
static AudioData *data;
static uint8_t data_pointer;
static uint32_t data_number;
static uint32_t data_write_number;

// 实时处理音频数据
static unsigned long last_time = millis();
static int last_ok = 0;
static int last_fail = 0;
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

      uint32_t first_pointer = (data_pointer + AUDIO_DECODER_MAX_BUFFER_SIZE - 1) % AUDIO_DECODER_MAX_BUFFER_SIZE;
      if (data[first_pointer].size > 0)
      {
        if (data[first_pointer].num > data_write_number)
        {
          if ((data[first_pointer].num - data_write_number) > 1)
          {
            uint8_t lost_number = data[first_pointer].num - data_write_number;
            // 缓冲区有多个数据包没有写入
            // logger::warnln("Audio Decoder has %d packet not write!", lost_number);
            // delay(AUDIO_DECODER_POLLING_CYCLE * lost_number);
            last_fail++;
          }
          else
          {
            if (i2s_channel_write(i2s_tx_handle, data[first_pointer].data, data[first_pointer].size, NULL, AUDIO_DECODER_POLLING_CYCLE * 2) != ESP_OK)
            {
              logger::warnln("Audio Decoder write fail!");
            }
            last_ok++;
          }
          unsigned long now_time = millis();
          if (now_time - last_time > 1000)
          {
            last_time = now_time;
            logger::warnln("Audio Decoder write %d/%d!", last_ok, last_fail + last_ok);
            last_ok = 0;
            last_fail = 0;
          }
          data_write_number = data[first_pointer].num;
        }
      }
    }
  }
}

void audio::decoder::setup()
{
  logger::debugln("Audio Decoder is starting...");
  data = (AudioData *)heap_caps_malloc(sizeof(AudioData) * AUDIO_DECODER_MAX_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
  pinMode(AUDIO_DECODER_MUTE, OUTPUT);
  pinMode(AUDIO_DECODER_FLT, OUTPUT);
  digitalWrite(AUDIO_DECODER_MUTE, LOW);
  digitalWrite(AUDIO_DECODER_FLT, LOW);
  xTaskCreatePinnedToCore(audioHandle, "audio_decoder_handle", TASK_AUDIO_DECODER_STACK, NULL, TASK_AUDIO_DECODER_PRIORITY, NULL, TASK_AUDIO_DECODER_CORE);
  logger::debugln("Audio Decoder is started!");
}

void audio::decoder::on(uint32_t rate, uint32_t bit)
{
  // 刷新缓存
  for (int i = 0; i < AUDIO_DECODER_MAX_BUFFER_SIZE; i++)
  {
    data[i].num = 0;
    data[i].size = 0;
  }
  data_pointer = 0;
  data_number = 0;
  data_write_number = 0;
  // 启动 i2s
  i2s_rate = rate;
  i2s_bit = (i2s_data_bit_width_t)bit;
  i2s_new_channel(&i2s_chan_cfg, &i2s_tx_handle, NULL);
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(i2s_rate),
      .slot_cfg = {
          .data_bit_width = i2s_bit,
          .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
          .slot_mode = I2S_SLOT_MODE_STEREO,
          .slot_mask = I2S_STD_SLOT_BOTH,
          .ws_width = I2S_SLOT_BIT_WIDTH_32BIT,
          .ws_pol = false,
          .bit_shift = true,
          .left_align = false,
          .big_endian = false,
          .bit_order_lsb = false},
      .gpio_cfg = i2s_gpio_cfg,
  };
  i2s_channel_init_std_mode(i2s_tx_handle, &std_cfg);
  i2s_channel_enable(i2s_tx_handle);
  powerOn = true;
  setMute(false);
  logger::debugln("Audio Decoder is on.");
}

void audio::decoder::off()
{
  setMute(true);
  i2s_channel_disable(i2s_tx_handle);
  i2s_del_channel(i2s_tx_handle);
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

uint32_t getDataNumber()
{
  return data[(data_pointer + AUDIO_DECODER_MAX_BUFFER_SIZE - 1) % AUDIO_DECODER_MAX_BUFFER_SIZE].num;
}

bool audio::decoder::writeData(uint8_t *waitData)
{
  size_t size = writeSize();
  // AudioData *buffer = &data[data_pointer];
  // data_pointer = (data_pointer + 1) % AUDIO_DECODER_MAX_BUFFER_SIZE;
  // buffer->num = data_number++ % UINT32_MAX;
  // buffer->size = size;
  // memcpy(buffer->data, waitData, size);

  if (i2s_channel_write(i2s_tx_handle, waitData, size, NULL, AUDIO_DECODER_POLLING_CYCLE * 2) != ESP_OK)
  {
    logger::warnln("Audio Decoder write fail!");
  }
  last_ok++;
  unsigned long now_time = millis();
  if (now_time - last_time > 1000)
  {
    last_time = now_time;
    logger::warnln("Audio Decoder write %d/%d!", last_ok, last_fail + last_ok);
    last_ok = 0;
    last_fail = 0;
  }
  return true;
}

size_t audio::decoder::writeSize()
{
  return i2s_rate * i2s_bit * AUDIO_DECODER_CHANNEL * AUDIO_DECODER_POLLING_CYCLE / 8 / 1000;
}
