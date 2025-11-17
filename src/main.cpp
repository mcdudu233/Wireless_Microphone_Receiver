#include "logger.h"
#include "module/screen.h"

void setup()
{
  logger::setup();
  screen::setup();
  logger::infoln("All modules are started now!");
}

void loop()
{
  delay(100);
}
