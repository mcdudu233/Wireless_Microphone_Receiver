#include "config.h"
#include "logger.h"
#include "module/usb/usb.h"
#include "module/usb/usb_device_msc.h"
#include "module/usb/usb_device_uac.h"
#include "module/usb/usb_descriptors.h"
#include "module/tf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_private/usb_phy.h"
#include "esp_timer.h"
#include "hal/usb_serial_jtag_ll.h"
#include "tusb.h"
#include "device/dcd.h"
#include <atomic>

static std::atomic<USBMode> usb_active_mode{USB_MODE_NONE};
static bool usb_tusb_on;
static bool cleanup_pending;
static usb_phy_handle_t usb_phy;
static portMUX_TYPE request_lock = portMUX_INITIALIZER_UNLOCKED;
struct USBRequest
{
  USBMode mode;
  usb::AudioFormat format;
  uint32_t generation;
};
static USBRequest desired = {USB_MODE_NONE, {AUDIO_RATE_48000, AUDIO_BIT_16, AUDIO_CHANNEL_SINGLE}, 0};
static usb::AudioFormat active_format = desired.format;
static uint32_t applied_generation;
static int64_t next_audio_frame;
static int64_t retry_after;

static usb::AudioFormat input_format()
{
  return {config::config.audio.rate, config::config.audio.bit, config::config.audio.channel};
}

static USBRequest requested()
{
  portENTER_CRITICAL(&request_lock);
  const USBRequest request = desired;
  portEXIT_CRITICAL(&request_lock);
  return request;
}

// Called only by the USB owner, after tud_task_ext has returned. No callbacks
// or PCM writes can race deletion of TinyUSB's queues/FIFOs/controller.
static bool stop_device()
{
  cleanup_pending = true;
  usb::uac::_disconnect();
  if (usb_tusb_on)
  {
    tud_disconnect();
    // A failed legacy tusb_init can leave tud initialized before tusb records
    // its port role. In that case teardown cannot find the device role yet.
    if (!tusb_teardown() && tud_inited() && !tud_deinit(0))
    {
      LOGGER_WARN("USB device stack deinit failed");
      return false;
    }
    usb_tusb_on = false;
  }
  // Explicitly remove Serial/JTAG's pullup; releasing the shared PHY alone
  // does not disconnect the hardwired controller from the host.
  usb_serial_jtag_ll_phy_enable_pad(false);
  if (usb_phy != nullptr)
  {
    if (usb_del_phy(usb_phy) != ESP_OK)
    {
      LOGGER_WARN("USB PHY deletion failed");
      return false;
    }
    usb_phy = nullptr;
  }
  if (usb::active_mode() == USB_MODE_SD)
  {
    usb::msc::_disconnect();
    if (!tf::set_usb_storage_active(false))
    {
      LOGGER_WARN("TF filesystem restore failed");
      return false;
    }
  }
  usb_active_mode.store(USB_MODE_NONE);
  cleanup_pending = false;
  // Allow Windows to remove the old audio/MSC/Serial-JTAG interfaces before
  // attaching the new device, including when only the audio format changes.
  vTaskDelay(pdMS_TO_TICKS(250));
  return true;
}

static bool start_device(const USBRequest &request)
{
  if (request.mode == USB_MODE_NONE)
    return true;
  if (request.mode == USB_MODE_AUDIO && !request.format.supported())
    return false;
  if (request.mode == USB_MODE_SD && !tf::set_usb_storage_active(true))
  {
    LOGGER_WARN("TF card unavailable for USB storage");
    return false;
  }
  usb::descriptors::prepare(request.format);
  usb::uac::_prepare(request.format);
  cleanup_pending = true;
  // Publish the descriptor personality before enabling the controller.
  usb_active_mode.store(request.mode);
  usb_phy_config_t phy_config = {
      .controller = request.mode == USB_MODE_DEBUG ? USB_PHY_CTRL_SERIAL_JTAG : USB_PHY_CTRL_OTG,
      .target = USB_PHY_TARGET_INT,
      .otg_mode = USB_OTG_MODE_DEVICE,
      .otg_speed = USB_PHY_SPEED_FULL};
  if (usb_new_phy(&phy_config, &usb_phy) != ESP_OK)
  {
    LOGGER_WARN("USB PHY init failed");
    stop_device();
    return false;
  }
  if (request.mode == USB_MODE_DEBUG)
    usb_serial_jtag_ll_phy_enable_pad(true);
  else
  {
    if (request.mode == USB_MODE_SD)
      usb::msc::_connect();
    if (!tusb_init())
    {
      LOGGER_WARN("USB device stack init failed");
      // A partial initialization may still own a controller/queue.
      usb_tusb_on = tud_inited();
      stop_device();
      return false;
    }
    usb_tusb_on = true;
  }
  active_format = request.format;
  cleanup_pending = false;
  next_audio_frame = esp_timer_get_time();
  retry_after = 0;
  LOGGER_INFO("USB mode=%u, audio=%luHz/%ubit/%uch", static_cast<unsigned>(request.mode),
              static_cast<unsigned long>(request.format.rate), static_cast<unsigned>(request.format.bit),
              static_cast<unsigned>(request.format.channels));
  return true;
}

static void service_device()
{
  USBRequest request = requested();
  if (request.generation != applied_generation && esp_timer_get_time() >= retry_after)
  {
    if (cleanup_pending || request.mode != usb::active_mode() ||
        (request.mode == USB_MODE_AUDIO && request.format != active_format))
    {
      if (!stop_device())
      {
        retry_after = esp_timer_get_time() + 1000000;
        return;
      }
      // Coalesce rapid UI changes made during the disconnect interval.
      request = requested();
      if (!start_device(request))
      {
        retry_after = esp_timer_get_time() + 1000000;
        return;
      }
    }
    applied_generation = request.generation;
  }
  if (usb_tusb_on)
  {
    tud_task_ext(0, false);
    const int64_t now = esp_timer_get_time();
    if (usb::active_mode() == USB_MODE_AUDIO && now >= next_audio_frame)
    {
      usb::uac::_loop();
      next_audio_frame += TASK_USB_PERIOD * 1000;
      // A delayed task must not burst-feed old frames after a stall.
      if (next_audio_frame <= now)
        next_audio_frame = now + TASK_USB_PERIOD * 1000;
    }
  }
}

static void usb_handle(void *)
{
#ifdef BUILD_DEBUG
  int64_t next_stack_report = 0;
#endif
  while (true)
  {
    service_device();
#ifdef BUILD_DEBUG
    if (esp_timer_get_time() >= next_stack_report)
    {
      LOGGER_INFO("USB task minimum free stack: %u bytes", static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
      next_stack_report = esp_timer_get_time() + 10000000;
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void usb::setup()
{
  // Historical settings may select an unsupported format. Keep capture intact
  // and turn off USB instead of repeatedly attaching an invalid device.
  if (config::config.usb.mode == USB_MODE_AUDIO && !input_format().supported())
  {
    LOGGER_WARN("Stored audio format exceeds USB bandwidth; USB disabled");
    config::config.usb.mode = USB_MODE_NONE;
    config::save();
  }
  usb_serial_jtag_ll_phy_enable_pad(false);
  on();
  if (xTaskCreatePinnedToCore(usb_handle, "usb_handle", TASK_USB_STACK, nullptr, TASK_USB_PRIORITY,
                              nullptr, TASK_USB_CORE) != pdPASS)
    LOGGER_ERROR("USB task creation failed");
}

void usb::on() { on(config::config.usb.mode); }
void usb::on(USBMode mode)
{
  if (mode > USB_MODE_SD)
    return;
  const AudioFormat format = input_format();
  if (mode == USB_MODE_AUDIO && !format.supported())
    return;
  portENTER_CRITICAL(&request_lock);
  desired.mode = mode;
  desired.format = format;
  ++desired.generation;
  portEXIT_CRITICAL(&request_lock);
}
void usb::off() { on(USB_MODE_NONE); }
USBMode usb::active_mode() { return usb_active_mode.load(); }
void usb::audio_format_changed()
{
  const AudioFormat format = input_format();
  portENTER_CRITICAL(&request_lock);
  desired.format = format;
  ++desired.generation;
  portEXIT_CRITICAL(&request_lock);
}

void ui_setting_usb_page_rcb(USBMode &mode) { mode = config::config.usb.mode; }
bool ui_setting_usb_page_scb(USBMode mode)
{
  if (mode == USB_MODE_AUDIO && !input_format().supported())
    return false;
  config::config.usb.mode = mode;
  config::save();
  usb::on(mode);
  return true;
}

void tud_mount_cb() { LOGGER_INFO("USB mounted"); }
void tud_event_hook_cb(uint8_t, uint32_t event_id, bool)
{
  if (event_id == DCD_EVENT_BUS_RESET || event_id == DCD_EVENT_UNPLUGGED)
  {
    // This hook can run in ISR context. Only publish atomic state here.
    usb::uac::_disconnect();
  }
}
void tud_umount_cb()
{
  usb::uac::_disconnect();
  LOGGER_INFO("USB unmounted");
}
void tud_suspend_cb(bool)
{
  // Preserve the selected alternate interface so resume needs no SET_INTERFACE.
  LOGGER_INFO("USB suspended");
}
void tud_resume_cb() { LOGGER_INFO("USB resumed"); }
