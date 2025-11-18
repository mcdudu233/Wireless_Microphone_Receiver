#include "config.h"
#include "logger.h"
#include "module/screen.h"

#include "lvgl.h"
#include "SPI.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

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
  tft.drawRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

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

// void log_cb(lv_log_level_t level, const char *buf)
// {
//   logger::debugln("%s", buf);
// }

void screen::backlight(float percent)
{
  ledcWrite(TFT_BLK, percent * (pow(2, TFT_BLK_BIT) - 1));
}

// LVGL 互斥锁
static SemaphoreHandle_t lv_mutex;
bool screen::lv_lock()
{
  return xSemaphoreTake(lv_mutex, 1) == pdTRUE;
}
void screen::lv_lock_wait()
{
  xSemaphoreTake(lv_mutex, portMAX_DELAY);
}
void screen::lv_unlock()
{
  xSemaphoreGive(lv_mutex);
}

// 初始化显示屏
static void setup_tft()
{
  logger::debugln("TFT now starting to init.");
  // 先初始化 SPI
  SPI.begin(TFT_CLK, -1, TFT_MOSI, -1);
  // 初始化 TFT
  tft.initR(INITR_MINI160x80);
  // tft.initR(INITR_MINI160x80_PLUGIN); // 颜色翻转
  tft.setSPISpeed(TFT_SPI_FREQ);
  tft.setRotation(3);
  logger::debugln("TFT is inited.");

  tft.fillScreen(ST77XX_BLACK);
  logger::debugln("TFT is started in black.");

  // 初始化背光
  logger::debugln("TFT backlight now starting to init.");
  ledcAttach(TFT_BLK, TFT_BLK_FREQ, TFT_BLK_BIT);
  screen::backlight(50);
  logger::debugln("TFT backlight is inited.");

  // 测试帧率
  // unsigned long last = millis();
  // int i = 0;
  // while (true)
  // {
  //   if (i % 2 == 0)
  //   {
  //     tft.fillScreen(ST77XX_BLACK);
  //   }
  //   else
  //   {
  //     tft.fillScreen(ST77XX_GREEN);
  //   }
  //   i++;
  //   unsigned long now = millis();
  //   if (now - last > 1000)
  //   {
  //     last = now;
  //     logger::debugln("%d", i);
  //     i = 0;
  //   }
  //   delay(1);
  // }
}

// 初始化图形库
static uint8_t screen_buffer[TFT_HOR_RES * TFT_VER_RES * 2]; /* x2 because of 16-bit color depth */
static void setup_lvgl()
{
  // 初始化 LVGL
  lv_init();
  // lv_log_register_print_cb(log_cb);
  lv_mutex = xSemaphoreCreateMutex();

  // 设置时间
  lv_tick_set_cb(tick_cb);

  // 创建屏幕 RGB565
  lv_display_t *display = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
  lv_display_set_buffers(display, screen_buffer, NULL, sizeof(screen_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, screen_cb);
  // lv_display_set_rotation(display, LV_DISP_ROTATION_270);

  // 创建输入设备 按钮操作
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(indev, button_cb);

  logger::debugln("LVGL is started.");
}

static void screen_handle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_SCREEN_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 图形库处理
    LV_LOCK();
    lv_timer_handler();
    LV_UNLOCK();
  }
}

void screen::setup()
{
  setup_tft();
  setup_lvgl();

  // 创建图形库处理线程
  xTaskCreatePinnedToCore(screen_handle, "screen_handle", TASK_SCREEN_STACK, NULL, TASK_SCREEN_PRIORITY, NULL, TASK_SCREEN_CORE);

  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "I LOVE YOU");
  lv_obj_center(label);

  lv_obj_t *sw = lv_switch_create(lv_screen_active());
  lv_obj_add_flag(sw, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_align(sw, LV_ALIGN_CENTER, 0, 25);
  logger::debugln("Module screen is started!");
}