#include "logger.h"
#include "module/button.h"

static unsigned long right_down_time = millis();
static unsigned long left_down_time = millis();
static volatile bool right_down_tmp = false;
static volatile bool left_down_tmp = false;

// 左、右、OK键是否被按下
static volatile bool right_down = false;
static volatile bool left_down = false;
static volatile bool ok_down = false;

void IRAM_ATTR onRightButtonUp()
{
  right_down_tmp = right_down = false;
  if (ok_down)
  {
    ok_down = false;
  }
  else
  {
    right_down = true;
  }
}

void IRAM_ATTR onRightButtonDown()
{
  right_down_time = millis();
  right_down_tmp = true;
  if (left_down_tmp)
  {
    ok_down = true;
  }
}

void IRAM_ATTR onLeftButtonUp()
{
  left_down_tmp = left_down = false;
  if (ok_down)
  {
    ok_down = false;
  }
  else
  {
    left_down = true;
  }
}

void IRAM_ATTR onLeftButtonDown()
{
  left_down_time = millis();
  left_down_tmp = true;
}

void button::setup()
{
  logger::debugln("Button is starting...");
  pinMode(BUTTON_RIGHT_IO, INPUT_PULLUP);
  pinMode(BUTTON_LEFT_IO, INPUT_PULLUP);
  attachInterrupt(BUTTON_RIGHT_IO, onRightButtonDown, ONLOW_WE);
  attachInterrupt(BUTTON_LEFT_IO, onLeftButtonDown, ONLOW_WE);
  attachInterrupt(BUTTON_RIGHT_IO, onRightButtonUp, ONHIGH_WE);
  attachInterrupt(BUTTON_LEFT_IO, onLeftButtonUp, ONHIGH_WE);
  logger::debugln("Button is started.");
}

bool button::left()
{
  return left_down;
}

bool button::right()
{
  return right_down;
}

bool button::ok()
{
  return ok_down;
}
