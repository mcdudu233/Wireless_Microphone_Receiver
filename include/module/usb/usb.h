#pragma once

enum USBMode
{
  USB_MODE_NONE = 0,
  USB_MODE_JTAG = 1,
  USB_MODE_AUDIO = 2,
  USB_MODE_SD = 3,
};

namespace usb
{
  void setup();
  void on();
  void on(USBMode mode);
  void off();
  // 等一下进入下载模式 用于多线程
  void download_later();
}