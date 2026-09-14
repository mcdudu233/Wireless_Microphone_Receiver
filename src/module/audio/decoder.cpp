#include "config.h"
#include "logger.h"
#include "module/audio/buffer.h"
#include "module/audio/decoder.h"
#include "ui/ui_setting.h"

#include "cctype"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"

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

#ifdef BUILD_DEBUG
struct DecoderDebugSnapshot
{
  bool ready;
  bool jack;
  bool enabled;
  uint8_t mode;
  bool i2s;
  uint32_t valid_frames;
  uint32_t concealed_frames;
  uint32_t empty_frames;
  uint32_t incomplete_frames;
  uint32_t recovery_count;
  uint32_t bytes_written_total;
  uint32_t pcm_peak;
  uint32_t write_errors;
  uint32_t write_wait_average_us;
  uint32_t write_wait_max_us;
  int32_t mclk_hz;
  uint32_t water_mark;
  AudioBufferDebugStats buffer;
};

static portMUX_TYPE decoder_debug_mux = portMUX_INITIALIZER_UNLOCKED;
static DecoderDebugSnapshot decoder_debug_snapshot = {};

static void decoderDebugHandle(void *arg)
{
  (void)arg;
  // 与RF诊断错峰，且让串口格式化/输出始终运行在低优先级PSRAM栈任务中。
  vTaskDelay(pdMS_TO_TICKS(350));
  while (true)
  {
    DecoderDebugSnapshot snapshot = {};
    portENTER_CRITICAL(&decoder_debug_mux);
    if (decoder_debug_snapshot.ready)
    {
      snapshot = decoder_debug_snapshot;
      decoder_debug_snapshot.ready = false;
    }
    portEXIT_CRITICAL(&decoder_debug_mux);

    if (snapshot.ready)
    {
      LOGGER_INFO("Audio output jack=%u enabled=%u mode=%u i2s=%u valid=%lu concealed=%lu empty=%lu incomplete=%lu recovered=%lu bytes=%lu peak=%lu errors=%lu wait_avg_us=%lu wait_max_us=%lu mclk=%ld water=%lu%%",
                  snapshot.jack ? 1U : 0U, snapshot.enabled ? 1U : 0U,
                  static_cast<unsigned int>(snapshot.mode), snapshot.i2s ? 1U : 0U,
                  static_cast<unsigned long>(snapshot.valid_frames),
                  static_cast<unsigned long>(snapshot.concealed_frames),
                  static_cast<unsigned long>(snapshot.empty_frames),
                  static_cast<unsigned long>(snapshot.incomplete_frames),
                  static_cast<unsigned long>(snapshot.recovery_count),
                  static_cast<unsigned long>(snapshot.bytes_written_total),
                  static_cast<unsigned long>(snapshot.pcm_peak),
                  static_cast<unsigned long>(snapshot.write_errors),
                  static_cast<unsigned long>(snapshot.write_wait_average_us),
                  static_cast<unsigned long>(snapshot.write_wait_max_us),
                  static_cast<long>(snapshot.mclk_hz),
                  static_cast<unsigned long>(snapshot.water_mark));
      LOGGER_INFO("Audio buffer rx_parts=%lu rx_bytes=%lu complete=%lu gaps=%lu incomplete=%lu missing_parts=%lu dup=%lu late=%lu invalid=%lu overrun=%lu depth=%lu",
                  static_cast<unsigned long>(snapshot.buffer.rx_parts),
                  static_cast<unsigned long>(snapshot.buffer.rx_bytes),
                  static_cast<unsigned long>(snapshot.buffer.completed_frames),
                  static_cast<unsigned long>(snapshot.buffer.gap_frames),
                  static_cast<unsigned long>(snapshot.buffer.incomplete_playouts),
                  static_cast<unsigned long>(snapshot.buffer.missing_parts),
                  static_cast<unsigned long>(snapshot.buffer.duplicate_parts),
                  static_cast<unsigned long>(snapshot.buffer.late_parts),
                  static_cast<unsigned long>(snapshot.buffer.invalid_parts),
                  static_cast<unsigned long>(snapshot.buffer.overrun_frames),
                  static_cast<unsigned long>(snapshot.buffer.depth));
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

static void logI2SClock(i2s_chan_handle_t handle, uint32_t requested_rate, uint32_t mclk_multiple)
{
  i2s_chan_info_t info = {};
  i2s_tuning_info_t tuning = {};
  const esp_err_t info_result = i2s_channel_get_info(handle, &info);
  const esp_err_t tuning_result = i2s_channel_tune_rate(handle, nullptr, &tuning);
  if (info_result != ESP_OK || tuning_result != ESP_OK || mclk_multiple == 0)
  {
    LOGGER_INFO("Audio Decoder I2S clock query failed: info=%s tuning=%s",
                esp_err_to_name(info_result), esp_err_to_name(tuning_result));
    return;
  }

  const uint64_t effective_millihz = static_cast<uint64_t>(tuning.curr_mclk_hz) * 1000ULL / mclk_multiple;
  LOGGER_INFO("Audio Decoder I2S clock requested=%luHz source=%u sclk=%lu mclk=%ld bclk=%lu effective=%lu.%03luHz dma=%lu water=%lu%%",
              static_cast<unsigned long>(requested_rate), static_cast<unsigned int>(info.clk_src),
              static_cast<unsigned long>(info.sclk_hz), static_cast<long>(tuning.curr_mclk_hz),
              static_cast<unsigned long>(info.bclk_hz),
              static_cast<unsigned long>(effective_millihz / 1000ULL),
              static_cast<unsigned long>(effective_millihz % 1000ULL),
              static_cast<unsigned long>(info.total_dma_buf_size),
              static_cast<unsigned long>(tuning.water_mark));
}
#endif

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

static void scalePcmFrames(uint8_t *pcm, uint32_t first_frame, uint32_t frame_count,
                           bool fade_in)
{
  if (frame_count == 0)
  {
    return;
  }
  const uint32_t channels = static_cast<uint32_t>(i2s_channel);
  const uint32_t bytes_per_sample = getBytesPerSample(i2s_bit);
  const uint32_t bytes_per_frame = bytes_per_sample * channels;
  for (uint32_t frame = 0; frame < frame_count; frame++)
  {
    const int64_t scale = fade_in ? static_cast<int64_t>(frame + 1)
                                  : static_cast<int64_t>(frame_count - frame - 1);
    for (uint32_t channel = 0; channel < channels; channel++)
    {
      uint8_t *sample_data = pcm + (first_frame + frame) * bytes_per_frame +
                             channel * bytes_per_sample;
      const int32_t sample = static_cast<int32_t>(
          static_cast<int64_t>(readPcmSample(sample_data, i2s_bit)) * scale / frame_count);
      writePcmSample(sample_data, i2s_bit, sample);
    }
  }
}

// 保留已收到的PCM，只将缺失网络分片覆盖为静音，并在缺口两侧淡变。
static const uint8_t *preparePartialFrame(const AudioData *audio, bool &recovered)
{
  const uint32_t size = audio->size;
  const uint32_t channels = static_cast<uint32_t>(i2s_channel);
  const uint32_t bytes_per_sample = getBytesPerSample(i2s_bit);
  const uint32_t bytes_per_frame = bytes_per_sample * channels;
  if (bytes_per_frame == 0 || audio->payload_capacity == 0 || audio->expected_parts == 0)
  {
    recovered = false;
    return prepareConcealmentFrame(size);
  }

  memcpy(concealment_data, audio->data, size);
  const uint32_t total_frames = size / bytes_per_frame;
  const uint32_t fade_frames = getFadeFrames(size);
  const bool first_part_missing = (audio->received_parts & 1UL) == 0;
  recovered = loss_active && !first_part_missing;

  uint8_t part = 0;
  while (part < audio->expected_parts)
  {
    if ((audio->received_parts & (1UL << part)) != 0)
    {
      part++;
      continue;
    }

    const uint8_t missing_first = part;
    while (part < audio->expected_parts &&
           (audio->received_parts & (1UL << part)) == 0)
    {
      part++;
    }
    const uint8_t missing_end = part;
    const uint32_t first_byte = static_cast<uint32_t>(missing_first) * audio->payload_capacity;
    uint32_t end_byte = static_cast<uint32_t>(missing_end) * audio->payload_capacity;
    if (end_byte > size)
    {
      end_byte = size;
    }
    const uint32_t first_missing_frame = first_byte / bytes_per_frame;
    uint32_t end_missing_frame = (end_byte + bytes_per_frame - 1) / bytes_per_frame;
    if (end_missing_frame > total_frames)
    {
      end_missing_frame = total_frames;
    }

    memset(concealment_data + first_missing_frame * bytes_per_frame, 0,
           (end_missing_frame - first_missing_frame) * bytes_per_frame);

    const uint32_t fade_out_count = first_missing_frame < fade_frames
                                        ? first_missing_frame
                                        : fade_frames;
    scalePcmFrames(concealment_data, first_missing_frame - fade_out_count,
                   fade_out_count, false);
    const uint32_t available_after = total_frames - end_missing_frame;
    const uint32_t fade_in_count = available_after < fade_frames
                                       ? available_after
                                       : fade_frames;
    scalePcmFrames(concealment_data, end_missing_frame, fade_in_count, true);
  }

  if (first_part_missing && !loss_active)
  {
    const uint32_t first_gap_frames = (audio->payload_capacity + bytes_per_frame - 1) /
                                      bytes_per_frame;
    const uint32_t decay_frames = first_gap_frames < fade_frames ? first_gap_frames : fade_frames;
    for (uint32_t frame = 0; frame < decay_frames; frame++)
    {
      const int64_t scale = static_cast<int64_t>(decay_frames - frame - 1);
      for (uint32_t channel = 0; channel < channels; channel++)
      {
        const int32_t sample = static_cast<int32_t>(
            static_cast<int64_t>(last_output_sample[channel]) * scale / decay_frames);
        writePcmSample(concealment_data + frame * bytes_per_frame + channel * bytes_per_sample,
                       i2s_bit, sample);
      }
    }
  }
  else if (!first_part_missing && loss_active)
  {
    const uint32_t fade_in_count = total_frames < fade_frames ? total_frames : fade_frames;
    scalePcmFrames(concealment_data, 0, fade_in_count, true);
  }

  const bool last_part_missing =
      (audio->received_parts & (1UL << (audio->expected_parts - 1))) == 0;
  loss_active = last_part_missing;
  if (last_part_missing)
  {
    last_output_sample[0] = 0;
    last_output_sample[1] = 0;
  }
  else
  {
    rememberLastSamples(concealment_data, size);
  }
  return concealment_data;
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
  uint32_t write_wait_total_us = 0;
  uint32_t write_wait_max_us = 0;
  uint32_t write_calls = 0;
#endif

  while (true)
  {
    // 播放开始后由I2S DMA的阻塞写入定节拍，避免两台设备的4ms系统节拍误差耗尽抖动缓冲。
    if (!powerOn || !playback_started)
    {
      vTaskDelay(pdMS_TO_TICKS(TASK_AUDIO_DECODER_PERIOD));
    }

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
        const bool sized_frame = data != nullptr && data->size == frame_size;
        const bool complete = sized_frame && data->complete;
        const bool partial = sized_frame && !complete && data->received_parts != 0;
        bool recovered = false;
        const uint8_t *output_data = complete
                                         ? prepareValidFrame(data->data, frame_size, recovered)
                                         : (partial ? preparePartialFrame(data, recovered)
                                                    : prepareConcealmentFrame(frame_size));
#ifndef BUILD_DEBUG
        (void)recovered;
#endif
        size_t bytes_written = 0;
#ifdef BUILD_DEBUG
        const uint32_t write_start_us = micros();
#endif
        const esp_err_t result = i2s_channel_write(i2s_tx_handle, output_data, frame_size, &bytes_written,
                                                   pdMS_TO_TICKS(AUDIO_DECODER_POLLING_CYCLE * 2));
#ifdef BUILD_DEBUG
        const uint32_t write_wait_us = micros() - write_start_us;
        write_wait_total_us += write_wait_us;
        write_calls++;
        if (write_wait_us > write_wait_max_us)
        {
          write_wait_max_us = write_wait_us;
        }
#endif
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
          recovery_count += recovered ? 1U : 0U;
          if (complete)
          {
            valid_frames++;
            // 峰值只抽样约1/16帧，诊断足够且避免在192kHz下重复扫描全部PCM。
            if ((valid_frames & 0x0FU) == 1U)
            {
              const uint32_t frame_peak = getPcmPeak(output_data, frame_size, i2s_bit);
              if (frame_peak > pcm_peak)
              {
                pcm_peak = frame_peak;
              }
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
      i2s_tuning_info_t tuning = {};
      const esp_err_t tuning_result = powerOn && i2s_tx_handle != nullptr
                                          ? i2s_channel_tune_rate(i2s_tx_handle, nullptr, &tuning)
                                          : ESP_ERR_INVALID_STATE;
      DecoderDebugSnapshot snapshot = {};
      snapshot.ready = true;
      snapshot.jack = plugin;
      snapshot.enabled = config::config.audio_output.enabled;
      snapshot.mode = static_cast<uint8_t>(config::config.audio_output.mode);
      snapshot.i2s = powerOn;
      snapshot.valid_frames = valid_frames;
      snapshot.concealed_frames = concealed_frames;
      snapshot.empty_frames = empty_frames;
      snapshot.incomplete_frames = incomplete_frames;
      snapshot.recovery_count = recovery_count;
      snapshot.bytes_written_total = bytes_written_total;
      snapshot.pcm_peak = pcm_peak;
      snapshot.write_errors = write_errors;
      snapshot.write_wait_average_us = write_calls > 0 ? write_wait_total_us / write_calls : 0;
      snapshot.write_wait_max_us = write_wait_max_us;
      snapshot.mclk_hz = tuning_result == ESP_OK ? tuning.curr_mclk_hz : 0;
      snapshot.water_mark = tuning_result == ESP_OK ? tuning.water_mark : 0;
      snapshot.buffer = buffer_stats;
      portENTER_CRITICAL(&decoder_debug_mux);
      decoder_debug_snapshot = snapshot;
      portEXIT_CRITICAL(&decoder_debug_mux);
      valid_frames = 0;
      concealed_frames = 0;
      empty_frames = 0;
      incomplete_frames = 0;
      recovery_count = 0;
      write_errors = 0;
      bytes_written_total = 0;
      pcm_peak = 0;
      write_wait_total_us = 0;
      write_wait_max_us = 0;
      write_calls = 0;
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
#ifdef BUILD_DEBUG
  if (xTaskCreatePinnedToCoreWithCaps(decoderDebugHandle, "audio_decoder_debug", 3072, nullptr, 1, nullptr,
                                      1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
  {
    LOGGER_INFO("Audio Decoder debug task creation failed.");
  }
#endif
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
  const i2s_mclk_multiple_t mclk_multiple =
      (i2s_bit == AUDIO_BIT_24) ? I2S_MCLK_MULTIPLE_384 : I2S_MCLK_MULTIPLE_256;
  i2s_std_config_t std_cfg = {
      .clk_cfg = {
          .sample_rate_hz = (uint32_t)i2s_rate,
          .clk_src = I2S_CLK_SRC_PLL_160M,
          .ext_clk_freq_hz = 0,
          .mclk_multiple = mclk_multiple,
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

#ifdef BUILD_DEBUG
  logI2SClock(i2s_tx_handle, static_cast<uint32_t>(i2s_rate), static_cast<uint32_t>(mclk_multiple));
#endif
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
