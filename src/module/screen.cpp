#include "config.h"
#include "logger.h"
#include "module/screen.h"
#include "module/button.h"
#include "ui/ui_loading.h"
#include "ui/ui_setting.h"

#include "lvgl.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_st7735s.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 屏幕接口
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t lcd_panel_handle;

// 按键回调
static bool left = false;
static bool right = false;
static bool ok = false;
static volatile uint32_t last_activity_ms = 0;
static volatile bool backlight_off = false;
static volatile bool backlight_wake_fade = false; // 按键唤醒后请求渐亮(由背光任务执行)
static volatile float backlight_current = 0;      // 当前亮度(0-100)
static volatile uint32_t backlight_gen = 0;       // 直接设置亮度时递增,打断进行中的渐变
static bool suppress_wake_input = false;
static bool slider_repeat_enabled = false;
static int8_t slider_repeat_direction = 0;
static uint32_t slider_repeat_at_ms = 0;

static bool focused_object_is_slider(lv_indev_t *indev)
{
  lv_group_t *group = lv_indev_get_group(indev);
  lv_obj_t *focused = group ? lv_group_get_focused(group) : nullptr;
  return focused && lv_obj_check_type(focused, &lv_slider_class);
}

static void button_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  constexpr uint32_t SLIDER_REPEAT_DELAY_MS = 300;
  constexpr uint32_t SLIDER_REPEAT_PERIOD_MS = 40;

  const bool left_down = button::left();
  const bool right_down = button::right();
  const bool ok_down = button::ok();
  const bool any_button = left_down || right_down || ok_down;
  data->enc_diff = 0;

  if (any_button && screen::note_activity())
  {
    suppress_wake_input = true;
    slider_repeat_enabled = false;
    slider_repeat_direction = 0;
  }
  if (suppress_wake_input)
  {
    data->enc_diff = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    if (!any_button)
      suppress_wake_input = false;
    return;
  }

  // 往左往右
  const uint32_t now_ms = millis();
  if (left_down && !left)
  {
    left = true;
    data->enc_diff = -1;
    slider_repeat_enabled = focused_object_is_slider(indev);
    slider_repeat_direction = -1;
    slider_repeat_at_ms = now_ms + SLIDER_REPEAT_DELAY_MS;
  }
  else if (!left_down && left)
  {
    left = false;
    if (slider_repeat_direction == -1)
    {
      slider_repeat_enabled = false;
      slider_repeat_direction = 0;
    }
  }
  if (right_down && !right)
  {
    right = true;
    data->enc_diff = 1;
    slider_repeat_enabled = focused_object_is_slider(indev);
    slider_repeat_direction = 1;
    slider_repeat_at_ms = now_ms + SLIDER_REPEAT_DELAY_MS;
  }
  else if (!right_down && right)
  {
    right = false;
    if (slider_repeat_direction == 1)
    {
      slider_repeat_enabled = false;
      slider_repeat_direction = 0;
    }
  }

  if (data->enc_diff == 0 && slider_repeat_enabled && slider_repeat_direction != 0 &&
      ((slider_repeat_direction < 0 && left_down) || (slider_repeat_direction > 0 && right_down)) &&
      static_cast<int32_t>(now_ms - slider_repeat_at_ms) >= 0)
  {
    data->enc_diff = slider_repeat_direction;
    slider_repeat_at_ms = now_ms + SLIDER_REPEAT_PERIOD_MS;
  }
  // 按下
  if (ok_down && !ok)
  {
    ok = true;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else if (!ok_down && ok)
  {
    ok = false;
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// void log_cb(lv_log_level_t level, const char * buf)
// {
//   LOGGER_INFO("%s", buf);
// }

// 仅写背光占空比并记录当前亮度(渐变内部使用,不打断渐变代数)
static void backlight_write(float percent)
{
  if (percent < 0)
    percent = 0;
  else if (percent > 100)
    percent = 100;
  ledcWrite(TFT_BLK, (int)(percent / 100 * (pow(2.0, TFT_BLK_BIT) - 1)));
  backlight_current = percent;
}

void screen::backlight(float percent)
{
  backlight_write(percent);
  // 外部直接设置亮度(如设置页滑条):进行中的渐变立即作废
  // (volatile的++在C++20被弃用,这里显式读改写)
  backlight_gen = backlight_gen + 1U;
}

// 亮度渐变(渐亮/渐暗共用,每步1%/10ms):
// 被直接设置亮度打断,或(渐暗时)被按键活动打断则提前返回false
static bool backlight_fade(float target, bool abort_on_activity)
{
  const uint32_t gen = backlight_gen;
  const uint32_t act = last_activity_ms;
  float cur = backlight_current;
  const float step = cur < target ? 1.0f : -1.0f;
  while (cur != target)
  {
    if (backlight_gen != gen)
      return false;
    if (abort_on_activity && last_activity_ms != act)
      return false;
    cur += step;
    backlight_write(cur);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return true;
}

void screen::apply_settings()
{
  last_activity_ms = millis();
  backlight_off = false;
  backlight(config::config.screen.brightness);
}

bool screen::note_activity()
{
  last_activity_ms = millis();
  if (!backlight_off)
    return false;

  backlight_off = false;
  // 渐亮由背光任务执行,避免在输入回调里阻塞LVGL
  backlight_wake_fade = true;
  return true;
}

void ui_setting_screen_page_rcb(uint8_t &brightness, ScreenTimeout &timeout)
{
  brightness = config::config.screen.brightness;
  timeout = config::config.screen.timeout;
}

void ui_setting_screen_page_scb(uint8_t brightness, ScreenTimeout timeout)
{
  config::config.screen.brightness = brightness;
  config::config.screen.timeout = timeout;
  config::save();
  screen::apply_settings();
}

// LVGL 互斥锁
static SemaphoreHandle_t lv_mutex;
bool screen::lv_lock()
{
  return lvgl_port_lock(1);
}
void screen::lv_lock_wait()
{
  lvgl_port_lock(0);
}
void screen::lv_unlock()
{
  lvgl_port_unlock();
}

// 初始化显示屏
static void setup_lcd()
{
  esp_err_t ret;

  // 初始化SPI
  spi_bus_config_t spi_cfg = {
      .mosi_io_num = TFT_MOSI,
      .miso_io_num = GPIO_NUM_NC,
      .sclk_io_num = TFT_CLK,
      .quadwp_io_num = GPIO_NUM_NC,
      .quadhd_io_num = GPIO_NUM_NC,
      .max_transfer_sz = TFT_DRAW_BUFFER_PIXELS * sizeof(uint16_t),
  };
  ret = spi_bus_initialize(TFT_SPI_NUM, &spi_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK)
  {
    LOGGER_WARN("LCD SPI init failed! reason=%d", ret);
    return;
  }

  // 初始化LCD驱动
  const esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = TFT_CS,
      .dc_gpio_num = TFT_DC,
      .spi_mode = 0,
      .pclk_hz = TFT_SPI_FREQ,
      .trans_queue_depth = 4,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .flags = {
          .psram_dma_direct = true,
      },
  };
  ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)1, &io_config, &io_handle);
  if (ret != ESP_OK)
  {
    LOGGER_WARN("LCD IO init failed! reason=%d", ret);
    return;
  }
  const esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = TFT_RST,
      .color_space = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ret = esp_lcd_new_panel_st7735s(io_handle, &panel_config, &lcd_panel_handle);
  if (ret != ESP_OK)
  {
    LOGGER_WARN("LCD Driver init failed! reason=%d", ret);
    return;
  }
  ret = esp_lcd_panel_reset(lcd_panel_handle);
  if (ret != ESP_OK)
  {
    LOGGER_WARN("LCD Driver init failed! reason=%d", ret);
    return;
  }
  ret = esp_lcd_panel_init(lcd_panel_handle);
  if (ret != ESP_OK)
  {
    LOGGER_WARN("LCD Driver init failed! reason=%d", ret);
    return;
  }

  // 初始化背光
  LOGGER_INFO("TFT backlight now starting to init.");
  ledcAttach(TFT_BLK, TFT_BLK_FREQ, TFT_BLK_BIT);
  screen::backlight(0);
  LOGGER_INFO("TFT backlight is inited.");
}

// 初始化图形库
static void setup_lvgl()
{
  esp_err_t ret;

  // 初始化LVGL
  const lvgl_port_cfg_t lvgl_cfg = {
      .task_priority = TASK_SCREEN_PRIORITY, /* LVGL task priority */
      .task_stack = TASK_SCREEN_STACK,       /* LVGL task stack size */
      .task_affinity = TASK_SCREEN_CORE,     /* LVGL task pinned to core (-1 is no affinity) */
      .task_max_sleep_ms = 500,              /* Maximum sleep in LVGL task */
      .timer_period_ms = TASK_SCREEN_PERIOD  /* LVGL timer tick period in ms */
  };
  ret = lvgl_port_init(&lvgl_cfg);
  if (ret != ESP_OK)
  {
    LOGGER_WARN("LVGL init failed!");
    return;
  }

  // 初始化屏幕
  const lvgl_port_display_cfg_t disp_cfg = {
      .io_handle = io_handle,
      .panel_handle = lcd_panel_handle,
      .buffer_size = TFT_DRAW_BUFFER_PIXELS,
      .double_buffer = true,
      .trans_size = TFT_DRAW_BUFFER_PIXELS,
      .hres = TFT_HOR_RES,
      .vres = TFT_VER_RES,
      .monochrome = false,
      .rotation = {
          .swap_xy = true,
          .mirror_x = true,
          .mirror_y = false,
      },
      .color_format = LV_COLOR_FORMAT_RGB565,
      .flags = {
          .buff_dma = true,
          .buff_spiram = true,
          .swap_bytes = true,
      }};
  if (lvgl_port_add_disp(&disp_cfg) == nullptr)
  {
    LOGGER_WARN("LVGL display buffer allocation failed.");
    return;
  }
  logger::memory("after LCD/LVGL");

  // 初始化输入设备
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(indev, button_cb);
  lv_group_t *group = lv_group_create();
  lv_group_set_default(group);
  lv_indev_set_group(indev, group);

  LOGGER_INFO("LVGL is started.");
}

// 屏幕背光线程:上电渐亮;超时渐暗熄屏;按键唤醒渐亮
static void screen_backlight_on_handle(void *arg)
{
  // 先等待屏幕控件加载再渐亮
  vTaskDelay(pdMS_TO_TICKS(200));
  // 屏幕渐亮
  backlight_fade(config::config.screen.brightness, false);
  last_activity_ms = millis();

  while (true)
  {
    // 按键唤醒(note_activity置位):渐亮回工作亮度
    if (backlight_wake_fade)
    {
      backlight_wake_fade = false;
      backlight_fade(config::config.screen.brightness, false);
    }
    const uint32_t timeout_ms = static_cast<uint32_t>(config::config.screen.timeout) * 1000U;
    if (timeout_ms > 0 && !backlight_off && millis() - last_activity_ms >= timeout_ms)
    {
      // 超时渐暗熄屏;渐暗期间若有按键活动则中止并渐亮回工作亮度
      // (此时熄屏标志尚未置位,该次按键作为正常输入生效,不会被当作唤醒消费)
      if (backlight_fade(0, true))
      {
        backlight_off = true;
      }
      else
      {
        backlight_fade(config::config.screen.brightness, false);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void screen::setup()
{
  LOGGER_INFO("Screen is starting...");
  setup_lcd();
  setup_lvgl();

  // 创建图形库处理线程
  xTaskCreatePinnedToCore(screen_backlight_on_handle, "screen_backlight_on_handle", TASK_SCREEN_BACKLIGHT_STACK, NULL, TASK_SCREEN_PRIORITY, NULL, TASK_SCREEN_CORE);

  LV_LOCK();
  ui_loading_init();
  LV_UNLOCK();

  LOGGER_INFO("Screen is started.");
}

/*****************************
       LVGL 内存分配函数
      采用 PSRAM 分配内存
*****************************/
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM

void lv_mem_init(void)
{
  return; /*Nothing to init*/
}

void lv_mem_deinit(void)
{
  return; /*Nothing to deinit*/
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
  /*Not supported*/
  LV_UNUSED(mem);
  LV_UNUSED(bytes);
  return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
  /*Not supported*/
  LV_UNUSED(pool);
  return;
}

void *lv_malloc_core(size_t size)
{
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void *lv_realloc_core(void *p, size_t new_size)
{
  return heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void lv_free_core(void *p)
{
  heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
  /*Not supported*/
  LV_UNUSED(mon_p);
  return;
}

lv_result_t lv_mem_test_core(void)
{
  /*Not supported*/
  return LV_RESULT_OK;
}

#endif /*LV_STDLIB_CLIB*/
