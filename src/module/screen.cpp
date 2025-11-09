#include "config.h"
#include "module/screen.h"

#include "lvgl.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"
#include "Adafruit_miniTFTWing.h"

Adafruit_miniTFTWing ss;
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

// 时间回调
static uint32_t tick_cb(void)
{
  return esp_timer_get_time() / 1000;
}

// 屏幕回调
static void screen_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.drawRGBBitmap(area->x1, area->x2, (uint16_t *)px_map, w, h);

  lv_display_flush_ready(disp);
}

//
static void button_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  // 往左往右
  data->enc_diff = 1;
  data->enc_diff = -1;
  // 按下
  data->state = LV_INDEV_STATE_PRESSED;
  data->state = LV_INDEV_STATE_RELEASED;
}

// 初始化显示屏
static void setup_tft()
{
  if (!ss.begin())
  {
    Serial.println("seesaw init error!");
    while (1)
      ;
  }
  else
    Serial.println("seesaw started");

  ss.tftReset();
  ss.setBacklight(0x0); // set the backlight fully on

  // Use this initializer (uncomment) if you're using a 0.96" 180x60 TFT
  tft.initR(INITR_MINI160x80); // initialize a ST7735S chip, mini display
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
}

// 初始化图形库
static void setup_lvgl()
{
  // 初始化 LVGL
  lv_init();

  // 设置时间
  lv_tick_set_cb(tick_cb);

  // 创建屏幕 RGB565
  lv_display_t *display = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
  static uint8_t buf[TFT_HOR_RES * TFT_VER_RES / 10 * 2]; /* x2 because of 16-bit color depth */
  lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, screen_cb);

  // 创建输入设备 按钮操作
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(indev, button_cb);
}

static void screen_handle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_SCREEN_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 图形库处理
    lv_timer_handler();
  }
}

void screen::setup()
{
  setup_tft();
  setup_lvgl();

  // 创建图形库处理线程
  xTaskCreate(screen_handle, "screen_handle", TASK_SCREEN_STACK, NULL, TASK_SCREEN_PRIORITY, NULL);
}