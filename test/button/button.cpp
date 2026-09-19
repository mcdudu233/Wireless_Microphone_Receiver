#include "../../src/module/button.cpp"

#include <cassert>
#include <cstdio>

unsigned long millis() { return 0; }
int digitalRead(int) { return 1; }
void pinMode(int, int) {}

static void reset_button_state()
{
  right_last_time = 0;
  left_last_time = 0;
  right_last = false;
  left_last = false;
  right_flag = false;
  left_flag = false;
  right_time = 0;
  left_time = 0;
  right_down = false;
  left_down = false;
  ok_down = false;
  ok_latched = false;
}

static void assert_releasing_ok_does_not_emit_direction(bool release_right_first)
{
  reset_button_state();
  button_update(10, true, false);
  button_update(50, true, true);
  assert(button::ok());
  assert(!button::left() && !button::right());

  if (release_right_first)
  {
    button_update(60, false, true);
    button_update(200, false, true);
  }
  else
  {
    button_update(60, true, false);
    button_update(200, true, false);
  }
  assert(!button::ok());
  assert(!button::left() && !button::right());

  button_update(210, false, false);
  assert(!button::ok());
  assert(!button::left() && !button::right());
}

int main()
{
  assert_releasing_ok_does_not_emit_direction(true);
  assert_releasing_ok_does_not_emit_direction(false);

  // 两键同一轮释放也不能泄漏单键事件。
  reset_button_state();
  button_update(10, false, true);
  button_update(50, true, true);
  assert(button::ok());
  button_update(60, false, false);
  assert(!button::ok() && !button::left() && !button::right());

  // 组合键完全释放后，普通短按仍按原时序产生一次方向键脉冲。
  button_update(100, true, false);
  button_update(140, false, false);
  assert(button::right());
  button_update(189, false, false);
  assert(button::right());
  button_update(190, false, false);
  assert(!button::right());

  std::puts("Button state-machine QA passed");
  return 0;
}
