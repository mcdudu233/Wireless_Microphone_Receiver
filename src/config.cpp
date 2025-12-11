#include "logger.h"
#include "config.h"

#include "Preferences.h"

config::ConfigValue config::value;

static Preferences prefs;

void config::setup()
{
  logger::debugln("Config is starting...");

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