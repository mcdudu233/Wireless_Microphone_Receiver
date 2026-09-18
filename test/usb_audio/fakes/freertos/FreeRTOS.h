#pragma once
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
using TickType_t = uint32_t;
using SemaphoreHandle_t = unsigned *;
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) (++*(x))
#define portEXIT_CRITICAL(x) (--*(x))
#define portMAX_DELAY UINT32_MAX
#define pdPASS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_32BIT 2
inline void *heap_caps_malloc(size_t size, unsigned) { return std::malloc(size); }
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new unsigned(0); }
inline bool xSemaphoreTake(SemaphoreHandle_t lock, unsigned timeout) { if(*lock && !timeout) return false; assert(*lock == 0); ++*lock; return true; }
inline void xSemaphoreGive(SemaphoreHandle_t lock) { assert(*lock == 1); --*lock; }
