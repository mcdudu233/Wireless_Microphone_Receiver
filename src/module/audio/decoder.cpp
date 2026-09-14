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
    .dma_desc_num = AUDIO_DECODER_DMA_DESC_NUM,
    .dma_frame_num = AUDIO_DECODER_DMA_FRAME_NUM,
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
static uint8_t *concealment_data = nullptr;
static int32_t last_output_sample[2] = {0, 0};
static bool loss_active = true;
static bool playback_started = false;

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

static uint8_t getBytesPerSample(AudioBit bit)
{
  return static_cast<uint8_t>(bit) / 8;
}

static int32_t readPcmSample(const uint8_t *pcm, AudioBit bit)
{
  if (bit == AUDIO_BIT_16)
  {
    return static_cast<int16_t>(static_cast<uint16_t>(pcm[0]) |
                                (static_cast<uint16_t>(pcm[1]) << 8));
  }
  if (bit == AUDIO_BIT_24)
  {
    int32_t value = static_cast<int32_t>(pcm[0]) |
                    (static_cast<int32_t>(pcm[1]) << 8) |
                    (static_cast<int32_t>(pcm[2]) << 16);
    return (value & 0x00800000) != 0 ? (value | 0xFF000000) : value;
  }
  return static_cast<int32_t>(static_cast<uint32_t>(pcm[0]) |
                              (static_cast<uint32_t>(pcm[1]) << 8) |
                              (static_cast<uint32_t>(pcm[2]) << 16) |
                              (static_cast<uint32_t>(pcm[3]) << 24));
}

static void writePcmSample(uint8_t *pcm, AudioBit bit, int32_t sample)
{
  pcm[0] = static_cast<uint8_t>(sample);
  pcm[1] = static_cast<uint8_t>(sample >> 8);
  if (bit != AUDIO_BIT_16)
  {
    pcm[2] = static_cast<uint8_t>(sample >> 16);
  }
  if (bit == AUDIO_BIT_32)
  {
    pcm[3] = static_cast<uint8_t>(sample >> 24);
  }
}

static uint32_t getFadeFrames(uint32_t frame_size)
{
  const uint32_t bytes_per_frame = getBytesPerSample(i2s_bit) * static_cast<uint32_t>(i2s_channel);
  if (bytes_per_frame == 0)
  {
    return 0;
  }
  const uint32_t sample_frames = frame_size / bytes_per_frame;
  const uint32_t fade_frames = static_cast<uint32_t>(i2s_rate) * AUDIO_DECODER_LOSS_FADE_MS / 1000U;
  return fade_frames < sample_frames ? fade_frames : sample_frames;
}

static void rememberLastSamples(const uint8_t *pcm, uint32_t size)
{
  const uint32_t channels = static_cast<uint32_t>(i2s_channel);
  const uint32_t bytes_per_sample = getBytesPerSample(i2s_bit);
  const uint32_t bytes_per_frame = bytes_per_sample * channels;
  if (channels == 0 || channels > 2 || bytes_per_frame == 0 || size < bytes_per_frame)
  {
    return;
  }
  const uint8_t *last_frame = pcm + size - bytes_per_frame;
  for (uint32_t channel = 0; channel < channels; channel++)
  {
    last_output_sample[channel] = readPcmSample(last_frame + channel * bytes_per_sample, i2s_bit);
  }
  if (channels == 1)
  {
    last_output_sample[1] = last_output_sample[0];
  }
}

static const uint8_t *prepareConcealmentFrame(uint32_t size)
{
  memset(concealment_data, 0, size);
  if (!loss_active)
  {
    const uint32_t channels = static_cast<uint32_t>(i2s_channel);
    const uint32_t bytes_per_sample = getBytesPerSample(i2s_bit);
    const uint32_t bytes_per_frame = bytes_per_sample * channels;
    const uint32_t fade_frames = getFadeFrames(size);
    for (uint32_t frame = 0; frame < fade_frames; frame++)
    {
      const int64_t scale = static_cast<int64_t>(fade_frames - frame - 1);
      for (uint32_t channel = 0; channel < channels; channel++)
      {
        const int32_t sample = static_cast<int32_t>(
            static_cast<int64_t>(last_output_sample[channel]) * scale / fade_frames);
        writePcmSample(concealment_data + frame * bytes_per_frame + channel * bytes_per_sample,
                       i2s_bit, sample);
      }
    }
  }
  last_output_sample[0] = 0;
  last_output_sample[1] = 0;
  loss_active = true;
  return concealment_data;
}

static const uint8_t *prepareValidFrame(const uint8_t *pcm, uint32_t size, bool &recovered)
{
  recovered = loss_active;
  if (loss_active)
  {
    memcpy(concealment_data, pcm, size);
    const uint32_t channels = static_cast<uint32_t>(i2s_channel);
    const uint32_t bytes_per_sample = getBytesPerSample(i2s_bit);
    const uint32_t bytes_per_frame = bytes_per_sample * channels;
    const uint32_t fade_frames = getFadeFrames(size);
    for (uint32_t frame = 0; frame < fade_frames; frame++)
    {
      const int64_t scale = static_cast<int64_t>(frame + 1);
      for (uint32_t channel = 0; channel < channels; channel++)
      {
        uint8_t *sample_data = concealment_data + frame * bytes_per_frame + channel * bytes_per_sample;
        const int32_t sample = static_cast<int32_t>(
            static_cast<int64_t>(readPcmSample(sample_data, i2s_bit)) * scale / fade_frames);
        writePcmSample(sample_data, i2s_bit, sample);
      }
    }
    pcm = concealment_data;
  }
  loss_active = false;
  rememberLastSamples(pcm, size);
  return pcm;
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
  uint32_t valid_frames = 0;
  uint32_t concealed_frames = 0;
  uint32_t empty_frames = 0;
  uint32_t incomplete_frames = 0;
  uint32_t recovery_count = 0;
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
      const uint32_t frame_size = audio::buffer::getFrameSize();
      if (data != nullptr)
      {
        playback_started = true;
      }
      // 初次收到音频帧前让I2S DMA自行保持零输出，避免以192kHz持续搬运无意义静音。
      if (frame_size > 0 && playback_started)
      {
        const bool complete = data != nullptr && data->complete && data->size == frame_size;
        bool recovered = false;
        const uint8_t *output_data = complete
                                         ? prepareValidFrame(data->data, frame_size, recovered)
                                         : prepareConcealmentFrame(frame_size);
#ifndef BUILD_DEBUG
        (void)recovered;
#endif
        size_t bytes_written = 0;
        const esp_err_t result = i2s_channel_write(i2s_tx_handle, output_data, frame_size, &bytes_written,
                                                   AUDIO_DECODER_POLLING_CYCLE * 2);
        if (result != ESP_OK || bytes_written != frame_size)
        {
          LOGGER_WARN("Audio Decoder write failed: %s, %u/%u bytes",
                      esp_err_to_name(result), static_cast<unsigned int>(bytes_written),
                      static_cast<unsigned int>(frame_size));
#ifdef BUILD_DEBUG
          write_errors++;
#endif
        }
#ifdef BUILD_DEBUG
        else
        {
          bytes_written_total += bytes_written;
          if (complete)
          {
            valid_frames++;
            recovery_count += recovered ? 1U : 0U;
            const uint32_t frame_peak = getPcmPeak(output_data, frame_size, i2s_bit);
            if (frame_peak > pcm_peak)
            {
              pcm_peak = frame_peak;
            }
          }
          else
          {
            concealed_frames++;
            if (data == nullptr)
            {
              empty_frames++;
            }
            else
            {
              incomplete_frames++;
            }
          }
        }
#endif
      }
    }

#ifdef BUILD_DEBUG
    if (millis() - report_time >= 1000)
    {
      report_time = millis();
      AudioBufferDebugStats buffer_stats = {};
      audio::buffer::getDebugStats(buffer_stats);
      LOGGER_INFO("Audio output jack=%u enabled=%u mode=%u i2s=%u valid=%lu concealed=%lu empty=%lu incomplete=%lu recovered=%lu bytes=%lu peak=%lu errors=%lu",
                   plugin ? 1U : 0U, config::config.audio_output.enabled ? 1U : 0U,
                   static_cast<unsigned int>(config::config.audio_output.mode), powerOn ? 1U : 0U,
                   static_cast<unsigned long>(valid_frames), static_cast<unsigned long>(concealed_frames),
                   static_cast<unsigned long>(empty_frames), static_cast<unsigned long>(incomplete_frames),
                   static_cast<unsigned long>(recovery_count), static_cast<unsigned long>(bytes_written_total),
                   static_cast<unsigned long>(pcm_peak),
                   static_cast<unsigned long>(write_errors));
      LOGGER_INFO("Audio buffer rx_parts=%lu rx_bytes=%lu complete=%lu gaps=%lu incomplete=%lu missing_parts=%lu dup=%lu late=%lu invalid=%lu overrun=%lu depth=%lu",
                  static_cast<unsigned long>(buffer_stats.rx_parts), static_cast<unsigned long>(buffer_stats.rx_bytes),
                  static_cast<unsigned long>(buffer_stats.completed_frames), static_cast<unsigned long>(buffer_stats.gap_frames),
                  static_cast<unsigned long>(buffer_stats.incomplete_playouts), static_cast<unsigned long>(buffer_stats.missing_parts),
                  static_cast<unsigned long>(buffer_stats.duplicate_parts),
                  static_cast<unsigned long>(buffer_stats.late_parts), static_cast<unsigned long>(buffer_stats.invalid_parts),
                  static_cast<unsigned long>(buffer_stats.overrun_frames), static_cast<unsigned long>(buffer_stats.depth));
      valid_frames = 0;
      concealed_frames = 0;
      empty_frames = 0;
      incomplete_frames = 0;
      recovery_count = 0;
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
  concealment_data = static_cast<uint8_t *>(
      heap_caps_malloc(AUDIO_BUFFER_MAX_DATA_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (concealment_data == nullptr)
  {
    LOGGER_ERROR("Audio Decoder concealment buffer allocation failed.");
    return;
  }
  memset(concealment_data, 0, AUDIO_BUFFER_MAX_DATA_SIZE);
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
  last_output_sample[0] = 0;
  last_output_sample[1] = 0;
  loss_active = true;
  playback_started = false;
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
  logger::memory("after audio I2S on");
  return true;
}

void audio::decoder::off()
{
  setMute(true);
  powerOn = false;
  last_output_sample[0] = 0;
  last_output_sample[1] = 0;
  loss_active = true;
  playback_started = false;
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
