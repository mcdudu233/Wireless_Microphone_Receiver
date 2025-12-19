#pragma once

enum USBMode
{
  USB_MODE_JTAG = 0,
  USB_MODE_AUDIO = 1,
  USB_MODE_SD = 2,
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