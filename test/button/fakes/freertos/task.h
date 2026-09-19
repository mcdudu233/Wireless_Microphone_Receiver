#pragma once

#include "FreeRTOS.h"

inline TickType_t xTaskGetTickCount() { return 0; }
inline void xTaskDelayUntil(TickType_t *, TickType_t) {}
inline int xTaskCreatePinnedToCore(void (*)(void *), const char *, unsigned, void *, unsigned, void *, unsigned)
{
  return pdPASS;
}
