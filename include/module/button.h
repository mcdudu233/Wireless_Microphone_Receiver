#pragma once

#define BUTTON_RIGHT_IO 6
#define BUTTON_LEFT_IO 8

// 两个按键一起按下判断为 OK 键的时间(毫秒)
#define BUTTON_TIME_OK 500

namespace button
{
  void setup();

  bool left();
  bool right();
  bool ok();
}