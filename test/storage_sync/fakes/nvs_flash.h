#pragma once
#include "esp_partition.h"
enum { ESP_ERR_NVS_NO_FREE_PAGES=-10, ESP_ERR_NVS_NEW_VERSION_FOUND=-11, ESP_PARTITION_SUBTYPE_DATA_NVS=2 };
inline int nvs_flash_init() { return ESP_OK; }
inline int esp_partition_erase_range(const esp_partition_t *,size_t,size_t) { return ESP_OK; }
