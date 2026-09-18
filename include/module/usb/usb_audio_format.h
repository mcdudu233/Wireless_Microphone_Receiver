#pragma once

#include "config.h"

namespace usb
{
  // ESP32-S3 DWC2: 256 FIFO words, 62 RX + 16 EP0 IN, leaving
  // 178 words (712 bytes) for the audio-only IN endpoint.
  // Keep one extra sample frame for TinyUSB's asynchronous clock correction.
  constexpr uint16_t AUDIO_EP_MAX_BYTES = 712;

  struct AudioFormat
  {
    AudioRate rate;
    AudioBit bit;
    AudioChannel channels;

    constexpr uint16_t sample_bytes() const { return static_cast<uint16_t>(bit / 8 * channels); }
    constexpr uint16_t packet_bytes() const { return static_cast<uint16_t>((rate / 1000 + 1) * sample_bytes()); }
    constexpr uint16_t frame_bytes() const { return static_cast<uint16_t>(rate / 1000 * TASK_USB_PERIOD * sample_bytes()); }
    constexpr bool supported() const
    {
      return (rate == AUDIO_RATE_48000 || rate == AUDIO_RATE_96000 || rate == AUDIO_RATE_192000) &&
             (bit == AUDIO_BIT_16 || bit == AUDIO_BIT_24 || bit == AUDIO_BIT_32) &&
             (channels == AUDIO_CHANNEL_SINGLE || channels == AUDIO_CHANNEL_STEREO) &&
             packet_bytes() <= AUDIO_EP_MAX_BYTES;
    }
    constexpr bool operator==(const AudioFormat &other) const
    {
      return rate == other.rate && bit == other.bit && channels == other.channels;
    }
    constexpr bool operator!=(const AudioFormat &other) const { return !(*this == other); }
  };
}
