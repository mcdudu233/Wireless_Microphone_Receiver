#include "logger.h"
#include "module/screen.h"
#include "module/ble.h"

void setup()
{
  logger::setup();
  screen::setup();
  ble::setup();
  logger::infoln("All modules are started now!");
}

void loop()
{
  delay(100);
}
