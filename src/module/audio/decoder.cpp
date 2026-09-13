#include "config.h"
#include "logger.h"
#include "module/audio/buffer.h"
#include "module/audio/decoder.h"
#include "ui/ui_setting.h"

#include "cctype"
#include "driver/i2s_std.h"
#include "esp_err.h"

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

static i2s_chan_handle_t i2s_tx_handle = nullptr;
static AudioChannel i2s_channel;
static AudioRate i2s_rate;
static AudioBit i2s_bit;
static bool powerOn = false;
static bool plugin = false;

static uint32_t getPcmPeak(const uint8_t *pcm, size_t size, AudioBit bit)
{
  uint32_t peak = 0;
  const size_t bytes_per_sample = static_cast<size_t>(bit) / 8;
  if (bytes_per_sample == 0)
  {
    return 0;
  }

  for (size_t offset = 0; offset + bytes_per_sample <= size; offset += bytes_per_sample)
  {
    int64_t sample = 0;
    if (bit == AUDIO_BIT_16)
    {
      sample = static_cast<int16_t>(static_cast<uint16_t>(pcm[offset]) |
                                    (static_cast<uint16_t>(pcm[offset + 1]) << 8));
    }
    else if (bit == AUDIO_BIT_24)
    {
      int32_t value = static_cast<int32_t>(pcm[offset]) |
                      (static_cast<int32_t>(pcm[offset + 1]) << 8) |
                      (static_cast<int32_t>(pcm[offset + 2]) << 16);
      if ((value & 0x00800000) != 0)
      {
        value |= 0xFF000000;
      }
      sample = value;
    }
    else
    {
      sample = static_cast<int32_t>(static_cast<uint32_t>(pcm[offset]) |
                                    (static_cast<uint32_t>(pcm[offset + 1]) << 8) |
                                    (static_cast<uint32_t>(pcm[offset + 2]) << 16) |
                                    (static_cast<uint32_t>(pcm[offset + 3]) << 24));
    }

    const uint32_t magnitude = static_cast<uint32_t>(sample < 0 ? -sample : sample);
    if (magnitude > peak)
    {
      peak = magnitude;
    }
  }
  return peak;
}

// 记录丢包率
// static unsigned long last_time = millis();
// static int last_ok = 0;
// static int last_fail = 0;

// 实时处理音频数据
static void audioHandle(void *arg)
{
  // 音频数据
  AudioData *data;
#ifdef BUILD_DEBUG
  uint32_t report_time = millis();
  uint32_t frames_written = 0;
  uint32_t frames_missing = 0;
  uint32_t write_errors = 0;
  uint32_t bytes_written_total = 0;
  uint32_t pcm_peak = 0;
#endif

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_AUDIO_DECODER_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 判断是否插入3.5mm接口
    bool isPlugin = !digitalRead(AUDIO_DECODER_ON);
    if (isPlugin != plugin)
    {
      plugin = isPlugin;
      LOGGER_INFO("%s", isPlugin ? "Audio Decoder found 3.5mm plug in!" : "Audio Decoder found 3.5mm plug out!");
    }

    // 自动模式由插孔检测控制；始终开启模式可绕过异常的检测脚。
    const bool should_power_on = config::config.audio_output.enabled &&
                                 (plugin || config::config.audio_output.mode == AUDIO_OUTPUT_ALWAYS_ON);
    if (should_power_on && !powerOn)
    {
      audio::decoder::on(config::config.audio.rate, config::config.audio.bit, config::config.audio.channel);
    }
    else if (!should_power_on && powerOn)
    {
      audio::decoder::off();
    }

    // 启动了芯片才读取数据
    if (powerOn)
    {
      // 判断音频配置是否更改
      if (config::config.audio.rate != i2s_rate || config::config.audio.bit != i2s_bit || config::config.audio.channel != i2s_channel)
      {
        audio::decoder::off();
        audio::decoder::on(config::config.audio.rate, config::config.audio.bit, config::config.audio.channel);
        LOGGER_INFO("Audio Decoder found audio config changed.");
      }

      // 获取数据
      data = audio::buffer::getDecoderData();
      if (data != nullptr)
      {
        size_t bytes_written = 0;
        const esp_err_t result = i2s_channel_write(i2s_tx_handle, data->data, data->size, &bytes_written,
                                                   AUDIO_DECODER_POLLING_CYCLE * 2);
        if (result != ESP_OK || bytes_written != data->size)
        {
          LOGGER_WARN("Audio Decoder write failed: %s, %u/%u bytes",
                      esp_err_to_name(result), static_cast<unsigned int>(bytes_written),
                      static_cast<unsigned int>(data->size));
#ifdef BUILD_DEBUG
          write_errors++;
#endif
        }
#ifdef BUILD_DEBUG
        else
        {
          frames_written++;
          bytes_written_total += bytes_written;
          const uint32_t frame_peak = getPcmPeak(data->data, data->size, i2s_bit);
          if (frame_peak > pcm_peak)
          {
            pcm_peak = frame_peak;
          }
        }
#endif
      }
#ifdef BUILD_DEBUG
      else
      {
        frames_missing++;
      }
#endif
    }

#ifdef BUILD_DEBUG
    if (millis() - report_time >= 1000)
    {
      report_time = millis();
      LOGGER_DEBUG("Audio output jack=%u enabled=%u mode=%u i2s=%u frames=%lu bytes=%lu peak=%lu empty=%lu errors=%lu",
                   plugin ? 1U : 0U, config::config.audio_output.enabled ? 1U : 0U,
                   static_cast<unsigned int>(config::config.audio_output.mode), powerOn ? 1U : 0U,
                   static_cast<unsigned long>(frames_written), static_cast<unsigned long>(bytes_written_total),
                   static_cast<unsigned long>(pcm_peak), static_cast<unsigned long>(frames_missing),
                   static_cast<unsigned long>(write_errors));
      frames_written = 0;
      frames_missing = 0;
      write_errors = 0;
      bytes_written_total = 0;
      pcm_peak = 0;
    }
#endif
  }
}

void audio::decoder::setup()
{
  LOGGER_INFO("Audio Decoder is starting...");
  pinMode(AUDIO_DECODER_MUTE, OUTPUT);
  pinMode(AUDIO_DECODER_FLT, OUTPUT);
  digitalWrite(AUDIO_DECODER_MUTE, LOW);
  digitalWrite(AUDIO_DECODER_FLT, LOW);
  // 启动插入检测
  pinMode(AUDIO_DECODER_ON, INPUT);
  xTaskCreatePinnedToCore(audioHandle, "audio_decoder_handle", TASK_AUDIO_DECODER_STACK, NULL, TASK_AUDIO_DECODER_PRIORITY, NULL, TASK_AUDIO_DECODER_CORE);
  LOGGER_INFO("Audio Decoder is started!");
}

bool audio::decoder::on(AudioRate rate, AudioBit bit, AudioChannel channel)
{
  if (powerOn)
  {
    return true;
  }

  setMute(true);
  // 启动 i2s
  i2s_rate = rate;
  i2s_bit = bit;
  i2s_channel = channel;
  i2s_std_config_t std_cfg = {
      .clk_cfg = {
          .sample_rate_hz = (uint32_t)i2s_rate,
          .clk_src = I2S_CLK_SRC_PLL_240M,
          .ext_clk_freq_hz = 0,
          .mclk_multiple = ((i2s_bit == AUDIO_BIT_24) ? I2S_MCLK_MULTIPLE_576 : I2S_MCLK_MULTIPLE_512),
          .bclk_div = 0,
      },
      .slot_cfg = {
          .data_bit_width = (i2s_data_bit_width_t)i2s_bit,
          .slot_bit_width = (i2s_slot_bit_width_t)i2s_bit,
          .slot_mode = (i2s_slot_mode_t)i2s_channel,
          .slot_mask = I2S_STD_SLOT_BOTH,
          .ws_width = (uint32_t)i2s_bit,
          .ws_pol = false,
          .bit_shift = true,
          .left_align = true,
          .big_endian = false,
          .bit_order_lsb = false,
      },
      .gpio_cfg = i2s_gpio_cfg,
  };

  esp_err_t result = i2s_new_channel(&i2s_chan_cfg, &i2s_tx_handle, NULL);
  if (result != ESP_OK)
  {
    i2s_tx_handle = nullptr;
    LOGGER_WARN("Audio Decoder cannot create I2S channel: %s", esp_err_to_name(result));
    return false;
  }

  result = i2s_channel_init_std_mode(i2s_tx_handle, &std_cfg);
  if (result != ESP_OK)
  {
    LOGGER_WARN("Audio Decoder cannot configure I2S: %s", esp_err_to_name(result));
    i2s_del_channel(i2s_tx_handle);
    i2s_tx_handle = nullptr;
    return false;
  }

  result = i2s_channel_enable(i2s_tx_handle);
  if (result != ESP_OK)
  {
    LOGGER_WARN("Audio Decoder cannot enable I2S: %s", esp_err_to_name(result));
    i2s_del_channel(i2s_tx_handle);
    i2s_tx_handle = nullptr;
    return false;
  }

  powerOn = true;
  setMute(false);
  LOGGER_INFO("Audio Decoder is on: %luHz/%ubit/%uch, jack=%u.", static_cast<unsigned long>(i2s_rate),
              static_cast<unsigned int>(i2s_bit), static_cast<unsigned int>(i2s_channel), plugin ? 1U : 0U);
  return true;
}

void audio::decoder::off()
{
  setMute(true);
  powerOn = false;
  if (i2s_tx_handle != nullptr)
  {
    const esp_err_t disable_result = i2s_channel_disable(i2s_tx_handle);
    if (disable_result != ESP_OK)
    {
      LOGGER_WARN("Audio Decoder cannot disable I2S: %s", esp_err_to_name(disable_result));
    }
    const esp_err_t delete_result = i2s_del_channel(i2s_tx_handle);
    if (delete_result != ESP_OK)
    {
      LOGGER_WARN("Audio Decoder cannot delete I2S channel: %s", esp_err_to_name(delete_result));
    }
    i2s_tx_handle = nullptr;
  }
  LOGGER_INFO("Audio Decoder is off.");
}

bool audio::decoder::isOn()
{
  return powerOn;
}

void ui_setting_audio_output_page_rcb(bool &enabled, AudioOutputMode &mode)
{
  enabled = config::config.audio_output.enabled;
  mode = config::config.audio_output.mode;
}

void ui_setting_audio_output_page_scb(bool enabled, AudioOutputMode mode)
{
  config::config.audio_output.enabled = enabled;
  config::config.audio_output.mode = mode;
  config::save();
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
