#include "module/usb/usb_device_uac.h"
#include "module/usb/usb_descriptors.h"
#include "module/audio/buffer.h"
#include "logger.h"
#include "tusb.h"
#include <atomic>
#include <cstring>

static_assert(TASK_USB_PERIOD == AUDIO_DECODER_POLLING_CYCLE, "USB frame pacing mismatch");
static usb::AudioFormat stream_format = {AUDIO_RATE_48000, AUDIO_BIT_16, AUDIO_CHANNEL_SINGLE};
static std::atomic<bool> streaming{false};
static uint8_t mute[3];
// Allocated at startup, never allocate or convert PCM on the audio hot path.
static uint8_t pcm_frame[4 * usb::AUDIO_EP_MAX_BYTES];

bool usb::uac::connected() { return streaming.load(); }

void usb::uac::_prepare(const AudioFormat &format)
{
    stream_format = format;
    memset(mute, 0, sizeof(mute));
    _disconnect();
}

void usb::uac::_connect()
{
    audio::buffer::resetUSBReader();
    streaming.store(true);
}

void usb::uac::_disconnect()
{
    streaming.store(false);
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, const tusb_control_request_t *request)
{
    const uint8_t entity = tu_u16_high(tu_le16toh(request->wIndex));
    const uint8_t itf = tu_u16_low(tu_le16toh(request->wIndex));
    const uint8_t selector = tu_u16_high(tu_le16toh(request->wValue));
    const uint8_t channel = tu_u16_low(tu_le16toh(request->wValue));
    if (itf != ITF_NUM_AUDIO_CONTROL || request->bmRequestType != 0xA1)
        return false;
    if (entity == UAC2_ENTITY_CLOCK && channel == 0)
    {
        if (selector == AUDIO20_CS_CTRL_SAM_FREQ)
        {
            if (request->bRequest == AUDIO20_CS_REQ_CUR)
            {
                audio20_control_cur_4_t current = {static_cast<int32_t>(tu_htole32(stream_format.rate))};
                return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &current, sizeof(current));
            }
            if (request->bRequest == AUDIO20_CS_REQ_RANGE)
            {
                audio20_control_range_4_n_t(1) range = {
                    .wNumSubRanges = tu_htole16(1), .subrange = {{
                        static_cast<int32_t>(tu_htole32(stream_format.rate)),
                        static_cast<int32_t>(tu_htole32(stream_format.rate)), 0}}};
                return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &range, sizeof(range));
            }
        }
        if (selector == AUDIO20_CS_CTRL_CLK_VALID && request->bRequest == AUDIO20_CS_REQ_CUR)
        {
            uint8_t valid = 1;
            return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &valid, sizeof(valid));
        }
    }
    if (entity == UAC2_ENTITY_FEATURE_UNIT && channel <= stream_format.channels &&
        selector == AUDIO20_FU_CTRL_MUTE && request->bRequest == AUDIO20_CS_REQ_CUR)
        return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &mute[channel], 1);
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t, const tusb_control_request_t *request, uint8_t *data)
{
    const uint8_t entity = tu_u16_high(tu_le16toh(request->wIndex));
    const uint8_t itf = tu_u16_low(tu_le16toh(request->wIndex));
    const uint8_t selector = tu_u16_high(tu_le16toh(request->wValue));
    const uint8_t channel = tu_u16_low(tu_le16toh(request->wValue));
    if (data == nullptr || itf != ITF_NUM_AUDIO_CONTROL || request->bmRequestType != 0x21 ||
        request->bRequest != AUDIO20_CS_REQ_CUR)
        return false;
    // Some hosts set the fixed clock to its current value. Accept that only.
    if (entity == UAC2_ENTITY_CLOCK && channel == 0 && selector == AUDIO20_CS_CTRL_SAM_FREQ &&
        tu_le16toh(request->wLength) == 4)
        return tu_unaligned_read32(data) == stream_format.rate;
    if (entity == UAC2_ENTITY_FEATURE_UNIT && channel <= stream_format.channels &&
        selector == AUDIO20_FU_CTRL_MUTE && tu_le16toh(request->wLength) == 1 && data[0] <= 1)
    {
        mute[channel] = data[0];
        return true;
    }
    return false;
}

bool tud_audio_set_itf_cb(uint8_t, const tusb_control_request_t *request)
{
    if (tu_le16toh(request->wIndex) != ITF_NUM_AUDIO_STREAMING || tu_le16toh(request->wValue) > 1)
        return false;
    if (tu_le16toh(request->wValue) == 1)
    {
        // Use a format-dependent target; a fixed 5 KB target is excessive for
        // mono 48 kHz. The FIFO corrects small RF/USB clock differences.
        tud_audio_n_set_ep_in_fifo_threshold(0, 2 * stream_format.frame_bytes());
        usb::uac::_connect();
    }
    else
        usb::uac::_disconnect();
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t, const tusb_control_request_t *request)
{
    if (tu_le16toh(request->wIndex) != ITF_NUM_AUDIO_STREAMING)
        return false;
    usb::uac::_disconnect();
    return true;
}

void usb::uac::_loop()
{
    if (!connected() || !tud_ready())
        return;
    const uint16_t bytes = stream_format.frame_bytes();
    tu_fifo_t *fifo = tud_audio_n_get_ep_in_ff(0);
    // Do not partially write/overwrite sample frames when the host stops
    // consuming. Retain the RF reader until the whole next frame can fit.
    if (fifo == nullptr || tu_fifo_remaining(fifo) < bytes)
        return;
    if (!audio::buffer::readUSBFrame(pcm_frame, bytes))
        memset(pcm_frame, 0, bytes);
    for (uint8_t channel = 0; channel < stream_format.channels; ++channel)
    {
        if (mute[0] || mute[channel + 1])
            for (uint16_t offset = channel * (stream_format.bit / 8); offset < bytes; offset += stream_format.sample_bytes())
                memset(pcm_frame + offset, 0, stream_format.bit / 8);
    }
    const uint16_t written = tud_audio_write(pcm_frame, bytes);
    if (written != bytes)
        LOGGER_WARN("USB audio FIFO short write: %u/%u", written, bytes);
}
