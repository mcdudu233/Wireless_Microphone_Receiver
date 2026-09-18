#pragma once
#include "freertos/FreeRTOS.h"
using StaticSemaphore_t = unsigned;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *p) { *p=0;return p; }
