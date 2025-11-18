#pragma once

// 屏幕分辨率
#define TFT_HOR_RES 160
#define TFT_VER_RES 80

// 屏幕引脚
#define TFT_RST 16
#define TFT_CS 15
#define TFT_DC 17
#define TFT_MOSI 21
#define TFT_CLK 18
#define TFT_SPI_FREQ 80 * 1000 * 1000

// 屏幕背光
#define TFT_BLK 7
#define TFT_BLK_FREQ 100 * 1000
#define TFT_BLK_BIT 8
#define TFT_BLK_TIME 100

// LVGL 互斥锁
#define LV_LOCK() screen::lv_lock_wait()
#define LV_UNLOCK() screen::lv_unlock()

namespace screen
{
  void setup();

  // 设置屏幕背光亮度
  void backlight(float percent);

  // LVGL 互斥锁
  bool lv_lock();
  void lv_lock_wait();
  void lv_unlock();
}