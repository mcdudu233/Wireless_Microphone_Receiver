#pragma once
#include "config.h"
#include "module/usb/usb_audio_format.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>

// Text schema is independent of the binary Preferences layout. Hash only
// explicit fields, never C++ padding, so a saved baseline survives reboots.
namespace config::file
{
  constexpr size_t CAPACITY = 2048;
  inline uint32_t fingerprint(const ConfigValue &v)
  {
    uint32_t hash = 2166136261U;
    const uint32_t values[] = {v.audio.channel, v.audio.rate, v.audio.bit, v.audio.mode,
      static_cast<uint32_t>(static_cast<int32_t>(v.audio.gain)), v.rf.mode, v.usb.mode,
      v.audio_output.enabled, v.audio_output.mode, v.screen.brightness, v.screen.timeout};
    for (uint32_t value : values)
      for (unsigned i = 0; i < 4; ++i) { hash = (hash ^ (value & 255)) * 16777619U; value >>= 8; }
    return hash;
  }
  inline bool valid(const ConfigValue &v)
  {
    return (v.audio.channel == AUDIO_CHANNEL_SINGLE || v.audio.channel == AUDIO_CHANNEL_STEREO) &&
      (v.audio.rate == AUDIO_RATE_48000 || v.audio.rate == AUDIO_RATE_96000 || v.audio.rate == AUDIO_RATE_192000) &&
      (v.audio.bit == AUDIO_BIT_16 || v.audio.bit == AUDIO_BIT_24 || v.audio.bit == AUDIO_BIT_32) &&
      v.audio.mode <= AUDIO_MODE_MANUAL && v.audio.gain >= -64 && v.audio.gain <= 63 &&
      (v.rf.mode == RF_MODE_WIFI || v.rf.mode == RF_MODE_BLE) && v.usb.mode <= USB_MODE_SD &&
      v.audio_output.mode <= AUDIO_OUTPUT_ALWAYS_ON && v.screen.brightness >= 10 && v.screen.brightness <= 100 &&
      (v.screen.timeout == SCREEN_TIMEOUT_NEVER || v.screen.timeout == SCREEN_TIMEOUT_30_SECONDS ||
       v.screen.timeout == SCREEN_TIMEOUT_1_MINUTE || v.screen.timeout == SCREEN_TIMEOUT_5_MINUTES) &&
      (v.rf.mode != RF_MODE_BLE || (v.audio.rate == AUDIO_RATE_48000 && v.audio.bit == AUDIO_BIT_16 &&
                                  v.audio.channel == AUDIO_CHANNEL_SINGLE)) &&
      (v.usb.mode != USB_MODE_AUDIO || usb::AudioFormat{v.audio.rate, v.audio.bit, v.audio.channel}.supported());
  }
  inline size_t encode(const ConfigValue &v, char *text, size_t capacity)
  {
    const int n = std::snprintf(text, capacity,
      "; Wireless Microphone Receiver - editable UTF-8 INI\n"
      "; Keep schema and sync_base unchanged when editing settings.\n"
      "; Numeric values: RF 1=BLE/2=WiFi; USB 0=off/1=debug/2=audio/3=reader.\n"
      "; Audio mode 0=auto/1=peak/2=manual; gain -64..63 dB.\n"
      "; Output mode 0=jack detect/1=always; timeout 0/30/60/300 seconds.\n"
      "; Screen brightness 10..100 percent; channel 1=mono/2=stereo.\n"
      "[sync]\nschema=1\nsync_base=%lu\n"
      "[audio]\nchannel=%u\nrate=%lu\nbit=%u\nmode=%u\ngain=%d\n"
      "[rf]\nmode=%u\n[usb]\nmode=%u\n"
      "[audio_output]\nenabled=%u\nmode=%u\n"
      "[screen]\nbrightness=%u\ntimeout=%u\n",
      static_cast<unsigned long>(fingerprint(v)), unsigned(v.audio.channel), static_cast<unsigned long>(v.audio.rate),
      unsigned(v.audio.bit), unsigned(v.audio.mode), int(v.audio.gain), unsigned(v.rf.mode), unsigned(v.usb.mode),
      v.audio_output.enabled ? 1U : 0U, unsigned(v.audio_output.mode), unsigned(v.screen.brightness), unsigned(v.screen.timeout));
    return n > 0 && size_t(n) < capacity ? size_t(n) : 0;
  }
  inline char *trim(char *s)
  {
    while (*s == ' ' || *s == '\t' || *s == '\r') ++s;
    char *end = s + std::strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) --end;
    *end = 0; return s;
  }
  inline bool decode(char *text, ConfigValue &v, uint32_t &baseline)
  {
    if (std::strncmp(text, "\xef\xbb\xbf", 3) == 0) text += 3;
    char section[24] = {};
    uint32_t seen = 0;
    for (char *line = text; line && *line; )
    {
      char *next = std::strchr(line, '\n'); if (next) *next++ = 0;
      char *s = trim(line); line = next;
      if (!*s || *s == ';' || *s == '#') continue;
      if (*s == '[')
      {
        char *end = std::strchr(s, ']');
        if (!end || *trim(end + 1) || size_t(end - s - 1) >= sizeof(section)) return false;
        *end = 0; std::strcpy(section, s + 1); continue;
      }
      char *eq = std::strchr(s, '='); if (!eq) return false;
      *eq++ = 0; char *key = trim(s), *value = trim(eq);
      char *comment = std::strpbrk(value, ";#"); if (comment) { *comment = 0; value = trim(value); }
      char *end; errno = 0; const long long number = std::strtoll(value, &end, 10);
      if (errno || end == value || *end) return false;
      int index = -1; long long lo = 0, hi = 0;
      if (!std::strcmp(section,"sync") && !std::strcmp(key,"schema")) { index=0;lo=hi=1; }
      else if (!std::strcmp(section,"sync") && !std::strcmp(key,"sync_base")) { index=1;hi=UINT32_MAX; }
      else if (!std::strcmp(section,"audio")) {
        if (!std::strcmp(key,"channel")) { index=2;lo=1;hi=2; }
        else if (!std::strcmp(key,"rate")) { index=3;hi=192000; }
        else if (!std::strcmp(key,"bit")) { index=4;hi=32; }
        else if (!std::strcmp(key,"mode")) { index=5;hi=2; }
        else if (!std::strcmp(key,"gain")) { index=6;lo=-64;hi=63; }
      }
      else if (!std::strcmp(section,"rf") && !std::strcmp(key,"mode")) { index=7;lo=1;hi=2; }
      else if (!std::strcmp(section,"usb") && !std::strcmp(key,"mode")) { index=8;hi=3; }
      else if (!std::strcmp(section,"audio_output")) {
        if (!std::strcmp(key,"enabled")) { index=9;hi=1; }
        else if (!std::strcmp(key,"mode")) { index=10;hi=1; }
      }
      else if (!std::strcmp(section,"screen")) {
        if (!std::strcmp(key,"brightness")) { index=11;lo=10;hi=100; }
        else if (!std::strcmp(key,"timeout")) { index=12;hi=300; }
      }
      if (index < 0 || number < lo || number > hi || (seen & (1U << index))) return false;
      seen |= 1U << index;
      switch (index) {
        case 1: baseline=uint32_t(number);break;
        case 2: v.audio.channel=AudioChannel(number);break;
        case 3: v.audio.rate=AudioRate(number);break;
        case 4: v.audio.bit=AudioBit(number);break;
        case 5: v.audio.mode=AudioMode(number);break;
        case 6: v.audio.gain=AudioGain(number);break;
        case 7: v.rf.mode=RFMode(number);break;
        case 8: v.usb.mode=USBMode(number);break;
        case 9: v.audio_output.enabled=number!=0;break;
        case 10: v.audio_output.mode=AudioOutputMode(number);break;
        case 11: v.screen.brightness=uint8_t(number);break;
        case 12: v.screen.timeout=ScreenTimeout(number);break;
      }
    }
    return seen == 0x1fff && valid(v);
  }
}
