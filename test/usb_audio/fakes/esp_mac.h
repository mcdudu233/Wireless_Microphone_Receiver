#pragma once
#include <cstdint>
inline int esp_efuse_mac_get_default(uint8_t *mac) {
  for (unsigned i = 0; i < 6; ++i) mac[i] = 0x10 + i;
  return 0;
}
