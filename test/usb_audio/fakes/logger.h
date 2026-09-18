#pragma once
#include "freertos/FreeRTOS.h"
#include <cstddef>
namespace logger {
void setup();
void memory(const char *);
void error(char *);
size_t take_logs(uint8_t *, size_t, uint32_t &);
}
#define LOGGER_INFO(...) ((void)0)
#define LOGGER_DEBUG(...) ((void)0)
#define LOGGER_WARN(...) ((void)0)
#define LOGGER_ERROR(...) assert(false)
