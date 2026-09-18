#pragma once

#include "config.h"

namespace usb
{
  void setup();
  void on();
  void on(USBMode mode);
  void off();
  USBMode active_mode();
  // Publish a completed input-format update, including BLE's fixed format.
  void audio_format_changed();
}
