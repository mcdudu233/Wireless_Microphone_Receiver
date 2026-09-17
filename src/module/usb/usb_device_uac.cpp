#include "config.h"
#include "logger.h"
#include "module/audio/buffer.h"
#include "module/usb/usb_device_uac.h"
#include "module/usb/tusb_config.h"
#include "module/usb/usb_descriptors.h"

#include "cstring"

#include "esp_heap_caps.h"
#include "tusb.h"

// USB任务按TASK_USB_PERIOD节拍每周期供给一帧音频, 而音频缓冲每帧代表
// AUDIO_DECODER_POLLING_CYCLE毫秒的采样, 两者不一致时供给速率会与主机消耗速率失配
static_assert(TASK_USB_PERIOD == AUDIO_DECODER_POLLING_CYCLE, "USB audio frame pacing mismatch");

// 已经连接上(主机已打开音频流接口的非零备用设置)
static bool isConnected = false;
// 转换输出帧缓冲(PSRAM, 首次使用时分配一次)
static uint8_t *usbFrame = nullptr;
static bool usbFrameAllocWarned = false;

/**********************************************/
/*               音频设备信息回调              */
/**********************************************/
// 音频的输出频率
static const uint32_t supported_freq[] = {48000, 96000, 192000};
#define SUPPORTED_FREQ_SIZE TU_ARRAY_SIZE(supported_freq)
// 主机未设置时使用第一个通告的频率; 由TinyUSB任务写入、USB任务读取
static volatile int32_t current_freq = 48000;
// 音频的输出比特
static const uint8_t supported_bit[CFG_TUD_AUDIO_FUNC_1_N_FORMATS] = {
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX,
    CFG_TUD_AUDIO_FUNC_1_FORMAT_2_RESOLUTION_RX,
    CFG_TUD_AUDIO_FUNC_1_FORMAT_3_RESOLUTION_RX};
static volatile uint8_t current_bit = supported_bit[0];
// 音频的音量
enum
{
    VOLUME_CTRL_0_DB = 0,
    VOLUME_CTRL_10_DB = 2560,
    VOLUME_CTRL_20_DB = 5120,
    VOLUME_CTRL_30_DB = 7680,
    VOLUME_CTRL_40_DB = 10240,
    VOLUME_CTRL_50_DB = 12800,
    VOLUME_CTRL_60_DB = 15360,
    VOLUME_CTRL_70_DB = 17920,
    VOLUME_CTRL_80_DB = 20480,
    VOLUME_CTRL_90_DB = 23040,
    VOLUME_CTRL_100_DB = 25600,
    VOLUME_CTRL_SILENCE = 0x8000,
};
static volatile int8_t mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];    // +1 for master channel 0
static volatile int16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; // +1 for master channel 0

// 获取音频信息的回调
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;

    // 获取音频的频率信息
    if (request->bEntityID == UAC2_ENTITY_CLOCK)
    {
        switch (request->bControlSelector)
        {
        case AUDIO_CS_CTRL_SAM_FREQ:
        {
            switch (request->bRequest)
            {
            case AUDIO_CS_REQ_CUR:
            {
                LOGGER_INFO("Clock get current freq %lu\r\n", current_freq);
                audio_control_cur_4_t curf = {(int32_t)tu_htole32(current_freq)};
                return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &curf, sizeof(curf));
            }
            case AUDIO_CS_REQ_RANGE:
            {
                audio_control_range_4_n_t(SUPPORTED_FREQ_SIZE) rangef = {.wNumSubRanges = tu_htole16(SUPPORTED_FREQ_SIZE)};
                LOGGER_INFO("Clock get %d freq ranges\r\n", SUPPORTED_FREQ_SIZE);
                for (uint8_t i = 0; i < SUPPORTED_FREQ_SIZE; i++)
                {
                    rangef.subrange[i].bMin = (int32_t)supported_freq[i];
                    rangef.subrange[i].bMax = (int32_t)supported_freq[i];
                    rangef.subrange[i].bRes = 0;
                    LOGGER_INFO("Range %d (%d, %d, %d)\r\n", i, (int)rangef.subrange[i].bMin, (int)rangef.subrange[i].bMax, (int)rangef.subrange[i].bRes);
                }
                return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &rangef, sizeof(rangef));
            }
            }
        }
        case AUDIO_CS_CTRL_CLK_VALID:
        {
            audio_control_cur_1_t cur_valid = {.bCur = 1};
            LOGGER_INFO("Clock get is valid %u\r\n", cur_valid.bCur);
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_valid, sizeof(cur_valid));
        }
        default:
        {
            LOGGER_INFO("Clock get request not supported, entity = %u, selector = %u, request = %u\r\n", request->bEntityID, request->bControlSelector, request->bRequest);
            return false;
        }
        }
    }
    // 获取音频音量信息
    else if (request->bEntityID == UAC2_ENTITY_FEATURE_UNIT)
    {
        TU_VERIFY(request->bChannelNumber <= CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
        switch (request->bControlSelector)
        {
        case AUDIO_FU_CTRL_MUTE:
        {
            audio_control_cur_1_t mute1 = {.bCur = mute[request->bChannelNumber]};
            TU_LOG1("Get channel %u mute %d\r\n", request->bChannelNumber, mute1.bCur);
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &mute1, sizeof(mute1));
        }
        case AUDIO_FU_CTRL_VOLUME:
        {
            switch (request->bRequest)
            {
            case AUDIO_CS_REQ_RANGE:
            {
                audio_control_range_2_n_t(1) range_vol = {
                    .wNumSubRanges = tu_htole16(1),
                    .subrange = {
                        {.bMin = tu_htole16(-VOLUME_CTRL_50_DB), .bMax = tu_htole16(VOLUME_CTRL_0_DB), .bRes = (256)}},
                };
                LOGGER_INFO("Get channel %u volume range (%d, %d, %u) dB\r\n", request->bChannelNumber, range_vol.subrange[0].bMin / 256, range_vol.subrange[0].bMax / 256, range_vol.subrange[0].bRes / 256);
                return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &range_vol, sizeof(range_vol));
            }
            case AUDIO_CS_REQ_CUR:
            {
                audio_control_cur_2_t cur_vol = {
                    .bCur = tu_htole16(volume[request->bChannelNumber])};
                LOGGER_INFO("Get channel %u volume %d dB\r\n", request->bChannelNumber, cur_vol.bCur / 256);
                return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_vol, sizeof(cur_vol));
            }
            }
        }
        default:
        {
            LOGGER_INFO("Feature unit get request not supported, entity = %u, selector = %u, request = %u\r\n",
                            request->bEntityID, request->bControlSelector, request->bRequest);
            return false;
        }
        }
    }

    LOGGER_INFO("Get request not handled, entity = %d, selector = %d, request = %d\r\n",
                    request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
}

// 设置音频信息的回调
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf)
{
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;

    // 设置音频频率
    if (request->bEntityID == UAC2_ENTITY_CLOCK)
    {
        switch (request->bControlSelector)
        {
        case AUDIO_CS_CTRL_SAM_FREQ:
        {
            TU_VERIFY(request->wLength == sizeof(audio_control_cur_4_t));
            const uint32_t freq = (uint32_t)((audio_control_cur_4_t const *)buf)->bCur;
            // 频率超出可转换范围时拒绝, 主机会回退到通告范围内的有效值
            if (freq < 8000 || freq > CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE)
            {
                LOGGER_WARN("Clock set invalid freq %lu\r\n", (unsigned long)freq);
                return false;
            }
            current_freq = (int32_t)freq;
            LOGGER_INFO("Clock set current freq: %ld\r\n", current_freq);
            return true;
        }
        default:
        {
            LOGGER_INFO("Clock set request not supported, entity = %u, selector = %u, request = %u\r\n",
                            request->bEntityID, request->bControlSelector, request->bRequest);
            return false;
        }
        }
    }
    // 设置音频音量
    else if (request->bEntityID == UAC2_ENTITY_FEATURE_UNIT)
    {
        TU_VERIFY(request->bChannelNumber <= CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);

        switch (request->bControlSelector)
        {
        case AUDIO_FU_CTRL_MUTE:
        {
            TU_VERIFY(request->wLength == sizeof(audio_control_cur_1_t));
            mute[request->bChannelNumber] = ((audio_control_cur_1_t const *)buf)->bCur;
            LOGGER_INFO("Set speaker channel %d Mute: %d\r\n", request->bChannelNumber, mute[request->bChannelNumber]);
            return true;
        }
        case AUDIO_FU_CTRL_VOLUME:
        {
            TU_VERIFY(request->wLength == sizeof(audio_control_cur_2_t));
            volume[request->bChannelNumber] = ((audio_control_cur_2_t const *)buf)->bCur;
            int volume_db = volume[request->bChannelNumber] / 256; // Convert to dB
            int volume_percent = (volume_db + 50) * 2;             // Map to range 0 to 100
            LOGGER_INFO("Set speaker channel %d volume: %d dB (%d)\r\n", request->bChannelNumber, volume_db, volume_percent);
            return true;
        }
        default:
        {
            LOGGER_INFO("Feature unit set request not supported, entity = %u, selector = %u, request = %u\r\n",
                            request->bEntityID, request->bControlSelector, request->bRequest);
            return false;
        }
        }
    }

    LOGGER_INFO("Set request not handled, entity = %d, selector = %d, request = %d\r\n",
                    request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
}

// 音频流格式变换回调
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));

    LOGGER_INFO("Set interface %d alt %d\r\n", itf, alt);

    // Clear buffer when streaming format is changed
    if (alt != 0)
    {
        current_bit = supported_bit[alt - 1];
        // 主机打开了非零备用设置(音频流已激活), 开始向端点供给音频数据
        usb::uac::_connect();
    }

    return true;
}

// 收到关闭音频流信息
bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    usb::uac::_disconnect();
    return true;
}
/**********************************************/
/**********************************************/
/**********************************************/

/**********************************************/
/*            音频流格式转换与供给            */
/**********************************************/
// 0 ~ -20 dB 每 1 dB 一档的 Q15 线性增益表(描述符通告的音量范围为 -20 ~ 0 dB)
static const int32_t volume_lut[21] = {
    32768, 29205, 26029, 23198, 20675, 18427, 16423, 14637, 13045, 11627,
    10362, 9235, 8231, 7336, 6538, 5827, 5193, 4629, 4125, 3677, 3277};

// 音量值(1/256 dB)转Q15线性增益, 范围外钳制(只衰减不放大)
static int32_t volumeToGain(int16_t vol)
{
    if (vol >= 0)
    {
        return 32768; // 0 dB
    }
    const int32_t attenuation = -static_cast<int32_t>(vol); // 衰减量, 单位 1/256 dB
    if (attenuation > 20 * 256)
    {
        return 0; // 低于-20 dB按静音处理
    }
    const uint32_t index = static_cast<uint32_t>(attenuation >> 8);
    const uint32_t frac = static_cast<uint32_t>(attenuation & 0xFF);
    int32_t gain = volume_lut[index];
    if (index < 20)
    {
        gain += static_cast<int32_t>((static_cast<int64_t>(volume_lut[index + 1] - volume_lut[index]) * frac) >> 8);
    }
    return gain;
}

// 读取指定位深的PCM样本(小端)
static int32_t readPcmSample(const uint8_t *pcm, uint8_t bit)
{
    if (bit == 16)
    {
        return static_cast<int16_t>(static_cast<uint16_t>(pcm[0]) |
                                    (static_cast<uint16_t>(pcm[1]) << 8));
    }
    if (bit == 24)
    {
        const int32_t value = static_cast<int32_t>(pcm[0]) |
                              (static_cast<int32_t>(pcm[1]) << 8) |
                              (static_cast<int32_t>(pcm[2]) << 16);
        return (value & 0x00800000) != 0 ? (value | 0xFF000000) : value;
    }
    return static_cast<int32_t>(static_cast<uint32_t>(pcm[0]) |
                                (static_cast<uint32_t>(pcm[1]) << 8) |
                                (static_cast<uint32_t>(pcm[2]) << 16) |
                                (static_cast<uint32_t>(pcm[3]) << 24));
}

// 写入指定位深的PCM样本(小端, 超出位深范围时钳制)
static void writePcmSample(uint8_t *pcm, uint8_t bit, int32_t sample)
{
    if (bit == 16)
    {
        if (sample > 32767)
            sample = 32767;
        else if (sample < -32768)
            sample = -32768;
        const uint16_t value = static_cast<uint16_t>(static_cast<int16_t>(sample));
        pcm[0] = static_cast<uint8_t>(value & 0xFF);
        pcm[1] = static_cast<uint8_t>(value >> 8);
    }
    else if (bit == 24)
    {
        if (sample > 8388607)
            sample = 8388607;
        else if (sample < -8388608)
            sample = -8388608;
        const uint32_t value = static_cast<uint32_t>(sample);
        pcm[0] = static_cast<uint8_t>(value & 0xFF);
        pcm[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        pcm[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    }
    else
    {
        const uint32_t value = static_cast<uint32_t>(sample);
        pcm[0] = static_cast<uint8_t>(value & 0xFF);
        pcm[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        pcm[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        pcm[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }
}

// 流式线性重采样状态: 下一个输出样本相对当前帧起点的位置(源采样帧, Q16定点)。
// 每帧处理结束后相位整体左移一帧长度, 数值保持有界, 长时间连续传输不会溢出。
// 采样率相同时相位恒为整数, 退化为逐样本无损直通。
static int64_t resample_phase = 0;
// 上一帧末尾样本(已上混到立体声), 用于跨帧插值衔接
static int32_t resample_hist[2] = {0, 0};
// 上次转换使用的格式, 变化时重置重采样状态
static uint32_t format_src_rate = 0;
static uint32_t format_src_bit = 0;
static uint32_t format_src_channel = 0;
static uint32_t format_host_rate = 0;
static uint32_t format_host_bit = 0;

static void resetResampler()
{
    resample_phase = 0;
    resample_hist[0] = 0;
    resample_hist[1] = 0;
}

// 主机格式下每4ms帧应有的字节数(固定双声道)
static uint32_t getHostFrameSize()
{
    const uint32_t rate = static_cast<uint32_t>(current_freq);
    const uint32_t bytes_per_sample = current_bit / 8;
    if (rate == 0 || bytes_per_sample == 0)
    {
        return 0;
    }
    const uint64_t bytes = static_cast<uint64_t>(rate) * bytes_per_sample *
                           CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX * AUDIO_DECODER_POLLING_CYCLE / 1000;
    return bytes <= AUDIO_BUFFER_MAX_DATA_SIZE ? static_cast<uint32_t>(bytes) : 0;
}

// 将一帧RF音频(流格式: config::config.audio)转换为主机选择的格式
// (固定双声道 + current_bit位深 + current_freq采样率), 并施加Feature Unit音量。
// 线性重采样保持跨帧相位, 采样率一致时为无损直通。
// 返回写入usbFrame的字节数(分数重采样下每帧输出样本数在名义值附近浮动)。
static uint32_t convertFrame(const uint8_t *src, uint32_t src_size)
{
    const uint32_t src_rate = static_cast<uint32_t>(config::config.audio.rate);
    const uint8_t src_bit = static_cast<uint8_t>(config::config.audio.bit);
    const uint32_t src_channel = static_cast<uint32_t>(config::config.audio.channel);
    const uint32_t host_rate = static_cast<uint32_t>(current_freq);
    const uint8_t host_bit = current_bit;

    const uint32_t src_bytes_per_sample = src_bit / 8;
    const uint32_t host_bytes_per_sample = host_bit / 8;
    const uint32_t src_frame_bytes = src_bytes_per_sample * src_channel;
    const uint32_t host_frame_bytes = host_bytes_per_sample * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX;
    if (src_rate == 0 || host_rate == 0 || src_frame_bytes == 0 || host_frame_bytes == 0)
    {
        return 0;
    }

    // 格式变化时重置重采样状态, 从新帧边界重新对齐
    if (src_rate != format_src_rate || src_bit != format_src_bit || src_channel != format_src_channel ||
        host_rate != format_host_rate || host_bit != format_host_bit)
    {
        resetResampler();
        format_src_rate = src_rate;
        format_src_bit = src_bit;
        format_src_channel = src_channel;
        format_host_rate = host_rate;
        format_host_bit = host_bit;
    }

    const uint32_t src_frames = src_size / src_frame_bytes;
    if (src_frames == 0)
    {
        return 0;
    }

    // 帧开始时快照音量配置, 避免逐样本读取volatile并保证帧内一致
    bool channel_mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX];
    int32_t channel_gain[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX];
    for (uint32_t channel = 0; channel < CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX; channel++)
    {
        channel_mute[channel] = mute[0] != 0 || mute[channel + 1] != 0;
        channel_gain[channel] = (volumeToGain(volume[0]) * volumeToGain(volume[channel + 1])) >> 15;
    }

    // 每输出一个采样帧, 相位推进 src_rate/host_rate (Q16)
    const int64_t step = (static_cast<int64_t>(src_rate) << 16) / host_rate;
    // 插值起点最多为本帧倒数第二帧, 保证终点帧仍在帧内
    const int64_t emit_limit = (static_cast<int64_t>(src_frames) - 1) << 16;

    uint32_t written = 0;
    while (written + host_frame_bytes <= AUDIO_BUFFER_MAX_DATA_SIZE && resample_phase < emit_limit)
    {
        // index为-1表示跨帧插值: 使用上一帧末尾样本
        const int32_t index = static_cast<int32_t>(resample_phase >> 16); // -1 .. src_frames-2
        const uint32_t frac = static_cast<uint32_t>(resample_phase & 0xFFFF);
        const uint8_t *frame_a = index < 0 ? nullptr : src + static_cast<uint32_t>(index) * src_frame_bytes;
        const uint8_t *frame_b = src + static_cast<uint32_t>(index + 1) * src_frame_bytes;
        for (uint32_t channel = 0; channel < CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX; channel++)
        {
            // 单声道源复制到双声道
            const uint32_t src_channel_index = channel < src_channel ? channel : 0;
            const int32_t sample_a = frame_a != nullptr
                                         ? readPcmSample(frame_a + src_channel_index * src_bytes_per_sample, src_bit)
                                         : resample_hist[channel];
            const int32_t sample_b = readPcmSample(frame_b + src_channel_index * src_bytes_per_sample, src_bit);
            int32_t sample = sample_a + static_cast<int32_t>(
                                            ((static_cast<int64_t>(sample_b) - sample_a) * frac) >> 16);
            if (channel_mute[channel])
            {
                sample = 0;
            }
            else
            {
                sample = static_cast<int32_t>((static_cast<int64_t>(sample) * channel_gain[channel]) >> 15);
            }
            writePcmSample(usbFrame + written + channel * host_bytes_per_sample, host_bit, sample);
        }
        written += host_frame_bytes;
        resample_phase += step;
    }
    // 帧结束: 相位整体左移一帧长度, 使下一帧从自身起点重新度量
    resample_phase -= static_cast<int64_t>(src_frames) << 16;

    // 记录本帧末尾样本供下一帧跨帧插值
    const uint8_t *last_frame = src + (src_frames - 1) * src_frame_bytes;
    for (uint32_t channel = 0; channel < CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX; channel++)
    {
        const uint32_t src_channel_index = channel < src_channel ? channel : 0;
        resample_hist[channel] = readPcmSample(last_frame + src_channel_index * src_bytes_per_sample, src_bit);
    }
    return written;
}

bool usb::uac::connected()
{
    return isConnected;
}

void usb::uac::_connect()
{
    isConnected = true;
}

void usb::uac::_disconnect()
{
    isConnected = false;
    // 断开后重置重采样状态, 重连时从新帧边界重新对齐
    resetResampler();
}

void usb::uac::_loop()
{
    if (!isConnected)
    {
        return;
    }
    if (usbFrame == nullptr)
    {
        // 转换缓冲仅首次使用时分配, 不在热路径反复分配
        usbFrame = static_cast<uint8_t *>(
            heap_caps_malloc(AUDIO_BUFFER_MAX_DATA_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (usbFrame == nullptr)
        {
            if (!usbFrameAllocWarned)
            {
                usbFrameAllocWarned = true;
                LOGGER_WARN("USB audio frame buffer allocation failed.");
            }
            return;
        }
    }

    const uint32_t host_frame_size = getHostFrameSize();
    if (host_frame_size == 0)
    {
        return;
    }

    AudioData *data = audio::buffer::getUSBData();
    const uint32_t frame_size = audio::buffer::getFrameSize();
    // 不完整帧绝不能作为PCM输出，否则缺失分片会让样本边界错位并产生爆音。
    const bool complete = data != nullptr && frame_size > 0 && data->complete && data->size == frame_size;

    uint32_t written = 0;
    if (complete)
    {
        written = convertFrame(data->data, frame_size);
    }
    if (written == 0)
    {
        // 无完整数据时输出静音帧保持主机侧消耗节拍;
        // 重采样相位保持不变, 数据恢复后自动续接, 期间的主机侧缺数由其缓冲吸收
        memset(usbFrame, 0, host_frame_size);
        written = host_frame_size;
    }
    tud_audio_write(usbFrame, static_cast<uint16_t>(written));
}
