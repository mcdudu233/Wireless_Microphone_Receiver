#pragma once
#include "freertos/FreeRTOS.h"
#define LOGGER_INFO(...) ((void)0)
#define LOGGER_DEBUG(...) ((void)0)
#define LOGGER_WARN(...) ((void)0)
#define LOGGER_ERROR(...) assert(false)
