#pragma once

#include "project.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

// RIFF PCM with standard LIST/INFO metadata. Serialized explicitly rather than
// relying on native struct alignment; 24-bit PCM stays packed as three bytes.
namespace audio::wav
{
  static_assert(48 + 24 + 8 + 3 + sizeof(MIC_PROJECT_NAME) + sizeof(AUTHOR) +
                sizeof("Wireless Microphone Receiver") <= 256, "WAV metadata exceeds header storage");
  struct Header
  {
    uint8_t data[256] = {};
    uint32_t size = 0;
    uint32_t data_size_offset = 0;
  };
  inline void put16(uint8_t *p, uint16_t n)
  {
    p[0] = static_cast<uint8_t>(n);
    p[1] = static_cast<uint8_t>(n >> 8);
  }
  inline void put32(uint8_t *p, uint32_t n)
  {
    put16(p, static_cast<uint16_t>(n));
    put16(p + 2, static_cast<uint16_t>(n >> 16));
  }
  inline void info(Header &header, const char *tag, const char *value)
  {
    const uint32_t length = static_cast<uint32_t>(std::strlen(value)) + 1;
    std::memcpy(header.data + header.size, tag, 4);
    put32(header.data + header.size + 4, length);
    std::memcpy(header.data + header.size + 8, value, length);
    header.size += 8 + length + (length & 1U);
  }
  inline bool make(Header &header, uint32_t rate, uint8_t bit, uint8_t channels)
  {
    if ((rate != 48000 && rate != 96000 && rate != 192000) ||
        (bit != 16 && bit != 24 && bit != 32) || (channels != 1 && channels != 2))
      return false;
    header = {};
    std::memcpy(header.data, "RIFF", 4);
    std::memcpy(header.data + 8, "WAVEfmt ", 8);
    put32(header.data + 16, 16);
    put16(header.data + 20, 1); // WAVE_FORMAT_PCM (signed little-endian).
    put16(header.data + 22, channels);
    put32(header.data + 24, rate);
    const uint16_t alignment = channels * (bit / 8);
    put32(header.data + 28, rate * alignment);
    put16(header.data + 32, alignment);
    put16(header.data + 34, bit);
    std::memcpy(header.data + 36, "LIST", 4);
    std::memcpy(header.data + 44, "INFO", 4);
    header.size = 48;
    info(header, "INAM", MIC_PROJECT_NAME);
    info(header, "IART", AUTHOR);
    info(header, "ISFT", "Wireless Microphone Receiver");
    put32(header.data + 40, header.size - 44);
    std::memcpy(header.data + header.size, "data", 4);
    header.data_size_offset = header.size + 4;
    header.size += 8;
    put32(header.data + 4, header.size - 8);
    return true;
  }
  inline void finish(Header &header, uint32_t bytes)
  {
    put32(header.data + 4, header.size - 8 + bytes + (bytes & 1U));
    put32(header.data + header.data_size_offset, bytes);
  }
}
