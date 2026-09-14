#include "logger.h"
#include "module/usb/usb_device_cdc.h"

#include "esp_heap_caps.h"

void logger::setup()
{
  // 根据构建类型设置日志级别
#if defined(BUILD_RELEASE)
  esp_log_level_set("*", ESP_LOG_WARN);
#elif defined(BUILD_DEBUG)
  esp_log_level_set("*", ESP_LOG_INFO);
#endif

  // 重定向 ESP-IDF 日志输出
  // esp_log_set_vprintf(esp_apptrace_vprintf);
  // vprintf();

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
  // TODO: 程序遇到了严重错误 显示屏提示
}
