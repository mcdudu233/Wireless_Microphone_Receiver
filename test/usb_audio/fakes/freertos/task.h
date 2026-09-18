#pragma once
#include "FreeRTOS.h"
void vTaskDelay(TickType_t ticks);
inline int xTaskCreatePinnedToCore(void (*)(void *), const char *, unsigned, void *, unsigned, void *, unsigned) { return pdPASS; }
