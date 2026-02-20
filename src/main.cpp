#include "logger.h"
#include "config.h"
#include "sys.h"
#include "ui/ui_loading.h"
#include "module/screen.h"
#include "module/rf.h"
#include "module/tf.h"
#include "module/button.h"
#include "module/usb/usb.h"
#include "module/audio/buffer.h"
#include "module/audio/decoder.h"

#include "esp_mac.h"

extern "C" void app_main()
{
  logger::setup();
  config::setup();
  screen::setup();
  ui_loading_set_part("屏幕");
  ui_loading_set_percent(20);
  // usb::setup();
  ui_loading_set_part("USB");
  ui_loading_set_percent(30);
  sys::setup();
  ui_loading_set_part("系统");
  ui_loading_set_percent(35);
  button::setup();
  ui_loading_set_part("按钮");
  ui_loading_set_percent(40);
  audio::buffer::setup();
  audio::decoder::setup();
  ui_loading_set_part("音频解码器");
  ui_loading_set_percent(50);
  rf::setup();
  ui_loading_set_part("蓝牙");
  ui_loading_set_percent(80);
  tf::setup();
  ui_loading_set_part("TF卡");
  ui_loading_set_percent(100);
  uint8_t mac[6];
  esp_base_mac_addr_get(mac);
  LOGGER_INFO("%2x:%2x:%2x:%2x:%2x:%2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  LOGGER_INFO("All modules are started now!");
}