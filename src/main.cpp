#include "logger.h"
#include "module/screen.h"
#include "module/ble.h"
#include "module/button.h"
#include "module/audio/decoder.h"

void setup()
{
  logger::setup();
  screen::setup();
  button::setup();
  audio::decoder::setup();
  ble::setup();
  logger::infoln("All modules are started now!");
}

void loop()
{
  delay(100);
}
