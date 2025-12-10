#include "logger.h"
#include "config.h"

#include "nvs_flash.h"
#include "Preferences.h"

config::ConfigValue config::value;

static Preferences prefs;

void config::setup()
{
  logger::debugln("Config is starting...");

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  prefs.begin(CONFIG_NAME);
  // TODO:
  prefs.clear();
  if (prefs.isKey(CONFIG_VALUE_NAME))
  {
    prefs.getBytes(CONFIG_VALUE_NAME, &value, sizeof(ConfigValue));
  }
  else
  {
    prefs.putBytes(CONFIG_VALUE_NAME, &value, sizeof(ConfigValue));
  }

  logger::debugln("Config is started.");
}