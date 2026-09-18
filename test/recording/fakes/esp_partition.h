#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
using esp_err_t=int;
enum { ESP_OK=0, ESP_ERR_NOT_FOUND=-1, ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_DATA_COREDUMP=3 };
struct esp_partition_t { uint32_t address=0xff0000,size=65536; };
namespace fake { inline std::vector<uint8_t> crash; inline bool crash_valid=true; inline unsigned erases=0; }
inline const esp_partition_t *esp_partition_find_first(int,int,void *) { static esp_partition_t p;return &p; }
inline int esp_partition_read(const esp_partition_t *,size_t offset,void *buffer,size_t size) {
  if(offset+size>fake::crash.size())return -2;
  std::memcpy(buffer,fake::crash.data()+offset,size);return ESP_OK;
}
inline const char *esp_err_to_name(int) { return "fake"; }
