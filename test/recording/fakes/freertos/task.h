#pragma once
#include "freertos/FreeRTOS.h"
using TaskHandle_t=void *;
inline void vTaskDelay(TickType_t) {}
inline int xTaskCreatePinnedToCore(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *handle, unsigned) { *handle=reinterpret_cast<void *>(1);return pdPASS; }
inline void xTaskNotifyGive(TaskHandle_t) {}
inline unsigned ulTaskNotifyTake(bool, TickType_t) { return 0; }
inline unsigned uxTaskGetStackHighWaterMark(void *) { return 2048; }
