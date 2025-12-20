#pragma once

namespace usb::uac
{
  bool connected();
  void _connect();
  void _disconnect();
  void _loop();
}