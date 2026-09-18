#include "logger.h"

#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "log_buffer.h"
#include <cstdarg>
#include <cstdio>

static EXT_RAM_BSS_ATTR logger::Buffer log_buffer;
static portMUX_TYPE log_lock = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t serial_output;
static bool log_hook_installed;

static int capture_log(const char *format, va_list args)
{
  char text[384];
  va_list copy;
  va_copy(copy, args);
  const int size = std::vsnprintf(text, sizeof(text), format, copy);
  va_end(copy);
  if (size > 0)
  {
    const size_t length = size_t(size) < sizeof(text) ? size_t(size) : sizeof(text) - 1;
    if (size_t(size) >= sizeof(text)) { text[length - 4]='.';text[length - 3]='.';text[length - 2]='.';text[length - 1]='\n'; }
    portENTER_CRITICAL(&log_lock);
    log_buffer.append(text, length);
    portEXIT_CRITICAL(&log_lock);
  }
  return serial_output ? serial_output(format, args) : size;
}

size_t logger::take_logs(uint8_t *out, size_t capacity, uint32_t &dropped)
{
  portENTER_CRITICAL(&log_lock);
  const size_t size = log_buffer.take(out, capacity, dropped);
  portEXIT_CRITICAL(&log_lock);
  return size;
}

void logger::setup()
{
  // 根据构建类型设置日志级别
#if defined(BUILD_RELEASE)
  esp_log_level_set("*", ESP_LOG_WARN);
#elif defined(BUILD_DEBUG)
  esp_log_level_set("*", ESP_LOG_INFO);
#endif

  if (!log_hook_installed)
  {
    serial_output = esp_log_set_vprintf(capture_log);
    log_hook_installed = true;
  }

  LOGGER_INFO("Logger is started!");
}

void logger::memory(const char *stage)
{
#ifdef BUILD_DEBUG
  const uint32_t internal_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  const uint32_t dma_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
  const uint32_t psram_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  LOGGER_INFO("Memory %s internal=%u largest=%u minimum=%u dma=%u dma_largest=%u psram=%u psram_largest=%u",
              stage ? stage : "unknown",
              static_cast<unsigned int>(heap_caps_get_free_size(internal_caps)),
              static_cast<unsigned int>(heap_caps_get_largest_free_block(internal_caps)),
              static_cast<unsigned int>(heap_caps_get_minimum_free_size(internal_caps)),
              static_cast<unsigned int>(heap_caps_get_free_size(dma_caps)),
              static_cast<unsigned int>(heap_caps_get_largest_free_block(dma_caps)),
              static_cast<unsigned int>(heap_caps_get_free_size(psram_caps)),
              static_cast<unsigned int>(heap_caps_get_largest_free_block(psram_caps)));
#else
  (void)stage;
#endif
}

void logger::error(char *str)
{
  (void)str;
  // TODO: 程序遇到了严重错误 显示屏提示
}
