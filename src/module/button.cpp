#include "logger.h"
#include "config.h"
#include "module/button.h"

static unsigned long right_last_time = millis();
static unsigned long left_last_time = millis();
static bool right_last = false;
static bool left_last = false;

static bool right_flag = false;
static bool left_flag = false;
static unsigned long right_time = millis();
static unsigned long left_time = millis();

// 左、右和OK键是否被按下
static bool right_down = false;
static bool left_down = false;
static bool ok_down = false;
// OK 组合键一旦成立，必须等两颗实体键都松开后才重新接受单键输入。
// 否则先松开的按键会结束 OK，仍按住的另一键会在下一次扫描中被误判为方向键。
static bool ok_latched = false;

static void latch_ok()
{
  ok_down = true;
  ok_latched = true;
  right_down = false;
  left_down = false;
  right_flag = false;
  left_flag = false;
}

static void button_update(unsigned long now_time, bool right_now, bool left_now)
{
  // 组合键释放期间吞掉两颗按键的所有单键边沿；两颗都松开后才解锁。
  if (ok_latched)
  {
    ok_down = right_now && left_now;
    right_down = false;
    left_down = false;
    right_flag = false;
    left_flag = false;
    right_last = right_now;
    left_last = left_now;
    if (!right_now && !left_now)
    {
      ok_latched = false;
    }
    return;
  }

  // 取消点击
  if (right_flag && (now_time - right_time >= BUTTON_RIGHT_TIME))
  {
    right_flag = false;
    right_down = false;
  }
  if (left_flag && (now_time - left_time >= BUTTON_LEFT_TIME))
  {
    left_flag = false;
    left_down = false;
  }

  // 检测右键第一次按下
  if (right_now && !right_last)
  {
    right_last_time = now_time;
    right_last = true;
    // 检查左右键是否都按下，则为OK键
    if (left_last && (now_time - left_last_time) <= BUTTON_OK_TIME)
    {
      latch_ok();
    }
  }
  if (right_last && !ok_down && (now_time - right_last_time) > BUTTON_OK_TIME)
  {
    right_down = true;
  }
  // 检测右键释放
  if (!right_now && right_last)
  {
    right_last = false;
    if (right_down)
    {
      right_down = false;
    }
    else
    {
      right_time = now_time;
      right_flag = true;
      right_down = true;
    }
  }

  // 检测左键第一次按下
  if (left_now && !left_last)
  {
    left_last_time = now_time;
    left_last = true;
    // 检查左右键是否都按下，则为OK键
    if (right_last && (now_time - right_last_time) <= BUTTON_OK_TIME)
    {
      latch_ok();
    }
  }
  if (left_last && !ok_down && (now_time - left_last_time) > BUTTON_OK_TIME)
  {
    left_down = true;
  }
  // 检测左键释放
  if (!left_now && left_last)
  {
    left_last = false;
    if (left_down)
    {
      left_down = false;
    }
    else
    {
      left_time = now_time;
      left_flag = true;
      left_down = true;
    }
  }
}

// 检测按钮线程
static void button_handle(void *arg)
{
  (void)arg;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_BUTTON_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    unsigned long now_time = millis();
    bool right_now = !digitalRead(BUTTON_RIGHT_IO);
    bool left_now = !digitalRead(BUTTON_LEFT_IO);
    button_update(now_time, right_now, left_now);
  }
}

void button::setup()
{
  LOGGER_INFO("Button is starting...");
  pinMode(BUTTON_RIGHT_IO, INPUT_PULLUP);
  pinMode(BUTTON_LEFT_IO, INPUT_PULLUP);
  xTaskCreatePinnedToCore(button_handle, "button_handle", TASK_BUTTON_STACK, NULL, TASK_BUTTON_PRIORITY, NULL, TASK_BUTTON_CORE);
  LOGGER_INFO("Button is started.");
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
