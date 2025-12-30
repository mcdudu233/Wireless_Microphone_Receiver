#include "config.h"
#include "logger.h"
#include "module/usb/usb.h"
#include "module/usb/usb_device_cdc.h"
#include "module/usb/usb_device_uac.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_private/usb_phy.h"
#include "esp_timer.h"
#include "tusb.h"

static bool usb_on = false;
static bool usb_tusb_on = false;
static bool usb_download_later = false;
static TaskHandle_t usb_task;
static usb_phy_handle_t usb_phy;

static void usb_jtag_mode();
static void usb_audio_mode();
static void usb_sd_mode();

static void usb_handle(void *arg)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_USB_PERIOD);
  while (true)
  {
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 处理 UAC
    // usb::uac::_loop();

    // 进入下载模式
    if (usb_download_later)
    {
      usb::on(USB_MODE_JTAG);
      usb_download_later = false;
    }
  }
}

static void tusb_handle(void *arg)
{
  while (true)
  {
    tud_task();
  }
}

void usb::setup()
{
  xTaskCreatePinnedToCore(usb_handle, "usb_handle", TASK_USB_STACK, NULL, TASK_USB_PRIORITY, NULL, TASK_TUSB_CORE);
  on();
}

void usb::on()
{
  on(config::config.usb.mode);
}

void usb::on(USBMode mode)
{
  if (usb_on)
  {
    off();
  }
  switch (mode)
  {
  case USB_MODE_JTAG:
  {
    usb_jtag_mode();
    break;
  }
  case USB_MODE_AUDIO:
  {
    usb_audio_mode();
    break;
  }
  case USB_MODE_SD:
  {
    usb_sd_mode();
    break;
  }
  case USB_MODE_NONE:
  default:
  {
    break;
  }
  }
}

void usb::off()
{
  // 关闭TUSB
  if (usb_tusb_on)
  {
    usb::uac::_disconnect();
    USBCDCSerial._disconnect();
    vTaskDelete(usb_task);
    if (!tusb_teardown())
    {
      logger::warnln("USB device stack deinit fail!");
      return;
    }
    usb_tusb_on = false;
  }

  // 恢复物理接口
  esp_err_t ret = usb_del_phy(usb_phy);
  if (ret != ESP_OK)
  {
    logger::warnln("USB PHY delete fail!");
    return;
  }
  usb_on = false;
}

void usb::download_later()
{
  usb_download_later = true;
}

static void usb_jtag_mode()
{
  usb_phy_config_t phy_conf = {
      .controller = USB_PHY_CTRL_SERIAL_JTAG,
  };
  esp_err_t ret = usb_new_phy(&phy_conf, &usb_phy);
  if (ret != ESP_OK)
  {
    logger::warnln("USB PHY init fail!");
    return;
  }
  usb_on = true;
}

static void usb_audio_mode()
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
  usb_on = true;

  // 初始化TUSB
  if (!tusb_init())
  {
    logger::warnln("USB device stack init fail!");
    return;
  }
  usb_tusb_on = true;
  xTaskCreatePinnedToCore(tusb_handle, "tusb_handle", TASK_TUSB_STACK, NULL, TASK_TUSB_PRIORITY, &usb_task, TASK_TUSB_CORE);
}

static void usb_sd_mode()
{
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