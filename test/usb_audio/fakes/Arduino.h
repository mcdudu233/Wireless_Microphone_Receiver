#pragma once
#include "freertos/FreeRTOS.h"
inline void heap_caps_free(void *memory) { std::free(memory); }
