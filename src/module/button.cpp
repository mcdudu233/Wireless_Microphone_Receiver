#include "logger.h"
#include "config.h"
#include "module/button.h"

static unsigned long right_last_time = millis();
static unsigned long left_last_time = millis();
static bool right_last = false;
static bool left_last = false;
static unsigned long right_time = millis();
static unsigned long left_time = millis();

// 左、右和OK键是否被按下
static bool right_down = false;
static bool left_down = false;
static bool ok_down = false;

// 检测按钮线程
static void button_handle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_BUTTON_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    unsigned long now_time = millis();
    bool right_now = !digitalRead(BUTTON_RIGHT_IO);
    bool left_now = !digitalRead(BUTTON_LEFT_IO);

    if (right_down && (now_time - right_time >= BUTTON_RIGHT_TIME))
    {
      right_down = false;
    }
    if (left_down && (now_time - left_time >= BUTTON_LEFT_TIME))
    {
      left_down = false;
    }

    // 检测右键第一次按下
    if (right_now && !right_last)
    {
      right_last_time = now_time;
      right_last = right_now;
    }
    // 按下后检查左键是否也在时间内按下
    if (right_now && (now_time - right_last_time <= BUTTON_OK_TIME))
    {
      ok_down = true;
      left_down = false;
      right_down = false;
    }

    // 检测左键第一次按下
    if (left_now && !left_last)
    {
      left_last_time = now_time;
      left_last = left_now;
    }
    if (left_now && (now_time - left_last_time <= BUTTON_OK_TIME))
    {
      ok_down = true;
      left_down = false;
      right_down = false;
    }

    // 检测右键释放
    if (!right_now && right_last)
    {
      right_last = false;

      // 如果OK键激活，释放OK键
      if (ok_down)
      {
        ok_down = false;
      }
      else
      {
        right_down = true;
        right_time = now_time;
      }
    }

    // 检测左键释放
    if (!left_now && left_last)
    {
      left_last = false;

      if (ok_down)
      {
        ok_down = false;
      }
      else
      {
        left_down = true;
        left_time = now_time;
      }
    }
  }
}

void button::setup()
{
  logger::debugln("Button is starting...");
  pinMode(BUTTON_RIGHT_IO, INPUT);
  pinMode(BUTTON_LEFT_IO, INPUT);
  xTaskCreatePinnedToCore(button_handle, "button_handle", TASK_BUTTON_STACK, NULL, TASK_BUTTON_PRIORITY, NULL, TASK_BUTTON_CORE);
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
