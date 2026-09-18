#include "logger.h"
#include "config.h"
#include "config_file.h"
#include "module/rf.h"
#include "module/screen.h"
#include "module/usb/usb.h"
#include "ui/ui_setting.h"

#include "nvs_flash.h"
#include "Preferences.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

config::ConfigValue config::config;

static Preferences prefs;
// save()会被多个任务(界面/解码器/USB/协议切换)并发调用,
// 同一NVS句柄并发读写会导致句柄状态破坏而崩溃,必须串行化
static SemaphoreHandle_t saveMutex = nullptr;
static config::ConfigValue persisted;
static uint32_t generation = 0;

// v0.11 的持久化布局；升级时保留用户已有的音频、无线和 USB 设置。
struct ConfigValueV000B
{
  struct
  {
    AudioChannel channel;
    AudioRate rate;
    AudioBit bit;
    AudioMode mode;
    AudioGain gain;
  } audio;
  struct
  {
    RFMode mode;
  } rf;
  struct
  {
    USBMode mode;
  } usb;
};

void config::setup()
{
  LOGGER_INFO("Config is starting...");

  saveMutex = xSemaphoreCreateMutex();
  if (saveMutex == nullptr)
  {
    LOGGER_ERROR("Config save mutex creation failed!");
  }

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    if (partition != NULL)
    {
      err = esp_partition_erase_range(partition, 0, partition->size);
      if (!err)
      {
        err = nvs_flash_init();
      }
      else
      {
        LOGGER_WARN("Config failed to format the broken NVS partition!");
      }
    }
    else
    {
      LOGGER_WARN("Config could not find NVS partition");
    }
  }
  if (err)
  {
    LOGGER_WARN("Config failed to initialize NVS! Error: %u", err);
  }

  prefs.begin(CONFIG_NAME);

  if (!prefs.isKey(CONFIG_VERSION_NAME))
  {
    // 不存则新建配置
    prefs.putUShort(CONFIG_VERSION_NAME, CONFIG_VERSION_VALUE);
    prefs.putBytes(CONFIG_DATA_NAME, &config, sizeof(ConfigValue));
  }
  else
  {
    const uint16_t stored_version = prefs.getUShort(CONFIG_VERSION_NAME);
    if (stored_version == 0x000B)
    {
      ConfigValueV000B old_config{};
      if (prefs.getBytesLength(CONFIG_DATA_NAME) == sizeof(old_config) &&
          prefs.getBytes(CONFIG_DATA_NAME, &old_config, sizeof(old_config)) == sizeof(old_config))
      {
        config.audio.channel = old_config.audio.channel;
        config.audio.rate = old_config.audio.rate;
        config.audio.bit = old_config.audio.bit;
        config.audio.mode = old_config.audio.mode;
        config.audio.gain = old_config.audio.gain;
        config.rf.mode = old_config.rf.mode;
        config.usb.mode = old_config.usb.mode;
      }
      prefs.putUShort(CONFIG_VERSION_NAME, CONFIG_VERSION_VALUE);
      prefs.putBytes(CONFIG_DATA_NAME, &config, sizeof(ConfigValue));
    }
    else if (stored_version == 0x000C)
    {
      // v1.0 与 v0.12 配置布局完全一致，升级时保留用户已有设置，仅更新版本号
      ConfigValue old_config{};
      if (prefs.getBytesLength(CONFIG_DATA_NAME) == sizeof(old_config) &&
          prefs.getBytes(CONFIG_DATA_NAME, &old_config, sizeof(old_config)) == sizeof(old_config))
      {
        config = old_config;
      }
      prefs.putUShort(CONFIG_VERSION_NAME, CONFIG_VERSION_VALUE);
      prefs.putBytes(CONFIG_DATA_NAME, &config, sizeof(ConfigValue));
    }
    else if (stored_version != CONFIG_VERSION_VALUE)
    {
      // 版本不一致重置配置
      prefs.clear();
      prefs.putUShort(CONFIG_VERSION_NAME, CONFIG_VERSION_VALUE);
      prefs.putBytes(CONFIG_DATA_NAME, &config, sizeof(ConfigValue));
    }
  }

  // 读取配置
  prefs.getBytes(CONFIG_DATA_NAME, &config, sizeof(ConfigValue));
  persisted = config;
  generation = 1;

  LOGGER_INFO("Config is started.");
}

void config::save()
{
  if (saveMutex == nullptr || xSemaphoreTake(saveMutex, portMAX_DELAY) != pdTRUE)
  {
    // Do not use an unprotected NVS handle after mutex initialization failure.
    LOGGER_WARN("Config save unavailable.");
    return;
  }
  const ConfigValue saving = config;
  if (prefs.putBytes(CONFIG_DATA_NAME, &saving, sizeof(ConfigValue)) == sizeof(ConfigValue))
  {
    persisted = saving;
    ++generation;
  }
  else LOGGER_WARN("Config Preferences write failed.");
  xSemaphoreGive(saveMutex);
}

bool config::snapshot(ConfigValue &value, uint32_t &revision)
{
  if (!saveMutex || xSemaphoreTake(saveMutex, portMAX_DELAY) != pdTRUE) return false;
  value = persisted;
  revision = generation;
  xSemaphoreGive(saveMutex);
  return revision != 0;
}

bool config::import_from_tf(const ConfigValue &value, uint32_t expected_generation)
{
  if (!file::valid(value) || !saveMutex) return false;
  // Never call while holding the TF card mutex: USB owner may need that mutex
  // while a UI event holds LVGL. Serialize with UI events only after media IO.
  LV_LOCK();
  if (rf::settings_busy()) { LV_UNLOCK(); return false; }
  xSemaphoreTake(saveMutex, portMAX_DELAY);
  if (generation != expected_generation ||
      prefs.putBytes(CONFIG_DATA_NAME, &value, sizeof(value)) != sizeof(value))
  {
    xSemaphoreGive(saveMutex); LV_UNLOCK(); return false;
  }
  const ConfigValue previous = config;
  config = persisted = value;
  ++generation;
  xSemaphoreGive(saveMutex);
  if (!rf::apply_settings(previous))
  {
    config = previous;
    save();
    LV_UNLOCK();
    return false;
  }
  screen::apply_settings();
  usb::on();
  ui_setting_refresh_config();
  LV_UNLOCK();
  LOGGER_INFO("TF configuration imported into Preferences.");
  return true;
}
