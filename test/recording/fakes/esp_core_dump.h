#pragma once
#include "esp_partition.h"
struct esp_core_dump_summary_t {
  char exc_task[16]="audio";uint32_t exc_pc=0x123456;uint8_t app_elf_sha256[65]={};
  struct { uint32_t bt[16]={0x40381234,0x40385678};uint32_t depth=2;bool corrupted=false; } exc_bt_info;
  struct { uint32_t exc_cause=28,exc_vaddr=0xdeadbeef; } ex_info;
};
inline int esp_core_dump_image_get(size_t *addr,size_t *size) { *addr=0xff0000;*size=fake::crash.size();return *size ? ESP_OK : ESP_ERR_NOT_FOUND; }
inline int esp_core_dump_image_check() { return fake::crash_valid ? ESP_OK : -3; }
inline int esp_core_dump_image_erase() { ++fake::erases;fake::crash.clear();return ESP_OK; }
inline int esp_core_dump_get_summary(esp_core_dump_summary_t *p) { *p={};std::memset(p->app_elf_sha256,'a',64);return ESP_OK; }
inline int esp_core_dump_get_panic_reason(char *p,size_t n) { std::strncpy(p,"test panic",n);return ESP_OK; }
