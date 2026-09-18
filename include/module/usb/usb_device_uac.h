#pragma once
#include "module/usb/usb_audio_format.h"

namespace usb::uac
{
  void _prepare(const AudioFormat &format);
  bool connected();
  void _connect();
  void _disconnect();
  void _loop();
}
