#pragma once

#include "config.h"

namespace usb
{
  void setup();
  void on();
  void on(USBMode mode);
  void off();
  USBMode active_mode();
  // 等一下进入下载模式 用于多线程
  void download_later();
}
