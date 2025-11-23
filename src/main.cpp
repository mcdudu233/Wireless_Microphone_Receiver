#include "logger.h"
#include "module/screen.h"
#include "module/ble.h"
#include "module/button.h"

void setup()
{
  logger::setup();
  button::setup();
  screen::setup();
  ble::setup();
  logger::infoln("All modules are started now!");
}

void loop()
{
  delay(100);
}
