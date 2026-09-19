#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define INPUT_PULLUP 0x05

unsigned long millis();
int digitalRead(int pin);
void pinMode(int pin, int mode);
