#include "config.h"
#include "logger.h"
#include "module/usb/usb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_private/usb_phy.h"
#include "esp_timer.h"
#include "tusb.h"

static TaskHandle_t usb_task;
static usb_phy_handle_t usb_phy;

static void tusb_handle(void *arg)
{
  while (true)
  {
    tud_task();
  }
}

void usb::setup()
{
  // 初始化USBOTG
  usb_phy_config_t phy_conf = {
      .controller = USB_PHY_CTRL_OTG,
      .target = USB_PHY_TARGET_INT,
      .otg_mode = USB_OTG_MODE_DEVICE,
      .otg_speed = USB_PHY_SPEED_FULL,
  };
  esp_err_t ret = usb_new_phy(&phy_conf, &usb_phy);
  if (ret != ESP_OK)
  {
    logger::warnln("USB PHY init fail!");
    return;
  }

  // 初始化TUSB
  if (!tusb_init())
  {
    logger::warnln("USB device stack init fail!");
    return;
  }
  xTaskCreatePinnedToCore(tusb_handle, "tusb_handle", TASK_TUSB_STACK, NULL, TASK_TUSB_PRIORITY, &usb_task, TASK_TUSB_CORE);
}

void usb::remove()
{
  // 关闭TUSB
  vTaskDelete(usb_task);
  if (!tusb_teardown())
  {
    logger::warnln("USB device stack deinit fail!");
    return;
  }

  // 恢复物理接口
  esp_err_t ret = usb_del_phy(usb_phy);
  if (ret != ESP_OK)
  {
    logger::warnln("USB PHY delete fail!");
    return;
  }
  // 切换到下载模式
  usb_phy_config_t phy_conf = {
      .controller = USB_PHY_CTRL_SERIAL_JTAG,
  };
  usb_new_phy(&phy_conf, &usb_phy);
}

/*               USB设备回调              */
// Invoked when device is mounted
void tud_mount_cb(void)
{
  logger::infoln("USB mounted");
}
// Invoked when device is unmounted
void tud_umount_cb(void)
{
  logger::infoln("USB unmounted");
}
// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  logger::infoln("USB suspended");
}
// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  logger::infoln("USB resumed");
}
/******************************************/