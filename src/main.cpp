#include "logger.h"
#include "config.h"
#include "ui/ui_loading.h"
#include "module/screen.h"
#include "module/rf.h"
#include "module/tf.h"
#include "module/button.h"
#include "module/audio/decoder.h"

void setup()
{
  logger::setup();
  config::setup();
  screen::setup();
  ui_loading_set_part("屏幕");
  ui_loading_set_percent(25);
  button::setup();
  ui_loading_set_part("按钮");
  ui_loading_set_percent(30);
  audio::decoder::setup();
  ui_loading_set_part("音频解码器");
  ui_loading_set_percent(50);
  rf::setup();
  ui_loading_set_part("蓝牙");
  ui_loading_set_percent(80);
  tf::setup();
  ui_loading_set_part("TF卡");
  ui_loading_set_percent(100);
  logger::infoln("All modules are started now!");

  audio::decoder::on(48000, 32);
}

void loop()
{
  // IRAM
  logger::debugln("Internal:\n");
  logger::debugln("  Total: %d bytes\n", heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
  logger::debugln("  Free: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  logger::debugln("  Min Free: %d bytes\n", heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
  // PSRAM
  logger::debugln("PSRAM:\n");
  logger::debugln("  Total: %d bytes\n", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
  logger::debugln("  Free: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  logger::debugln("  Min Free: %d bytes\n", heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
  delay(10000);
}
