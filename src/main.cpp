#include "logger.h"
#include "module/screen.h"
#include "module/rf.h"
#include "module/button.h"
#include "module/audio/decoder.h"

void setup()
{
  logger::setup();
  screen::setup();
  button::setup();
  audio::decoder::setup();
  rf::setup();
  logger::infoln("All modules are started now!");
}

void loop()
{
  delay(100);
}
