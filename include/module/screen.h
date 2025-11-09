#pragma once

// 屏幕分辨率
#define TFT_HOR_RES 160
#define TFT_VER_RES 80

// 屏幕引脚
#define TFT_RST 16
#define TFT_CS 15
#define TFT_DC 5
#define TFT_MOSI 21
#define TFT_CLK 18

namespace screen
{
  void setup();
}