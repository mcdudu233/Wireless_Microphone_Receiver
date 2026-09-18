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
  // Serialize external Stream calls against owner-task teardown; never wait.
  bool cdc_lock();
  void cdc_unlock();
  // 等一下进入下载模式 用于多线程
  void download_later();
}
