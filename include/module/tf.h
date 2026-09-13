#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>

#define TF_CLK_IO GPIO_NUM_12
#define TF_CMD_IO GPIO_NUM_11
#define TF_D0_IO GPIO_NUM_13
#define TF_D1_IO GPIO_NUM_14
#define TF_D2_IO GPIO_NUM_9
#define TF_D3_IO GPIO_NUM_10

#define TF_1BIT_MODE false
#define TF_FREQ 40000
#define TF_MOUNT_POINT "/card"
#define TF_DATA_ROOT "/WirelessMic"
#define TF_RECORDINGS_ROOT TF_DATA_ROOT "/recordings"
#define TF_CONFIG_ROOT TF_DATA_ROOT "/config"
#define TF_LOG_ROOT TF_DATA_ROOT "/logs"

#define TF_FILE_NAME_MAX 256
#define TF_PATH_MAX 512
#define TF_FILE_LIST_MAX 24

namespace tf
{
  enum class CardType : uint8_t
  {
    NONE,
    MMC,
    SD,
    SDHC,
    UNKNOWN
  };

  struct StorageInfo
  {
    bool mounted;
    bool usb_active;
    CardType type;
    uint64_t capacity_bytes;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint32_t sector_count;
    uint16_t sector_size;
  };

  struct FileEntry
  {
    char name[TF_FILE_NAME_MAX];
    uint64_t size;
    bool directory;
    bool name_complete;
  };

  enum class RequestStatus : uint8_t
  {
    IDLE,
    BUSY,
    LIST_READY,
    REMOVE_READY
  };

  void setup();
  bool is_mounted();
  void get_info(StorageInfo &info);
  bool set_usb_storage_active(bool active);
  bool is_usb_storage_active();
  bool read_sector(uint32_t sector, uint8_t *buffer);
  bool write_sector(uint32_t sector, const uint8_t *buffer);
  bool is_deletable_path(const char *path);
  bool list(const char *path, FileEntry *entries, size_t capacity, size_t &count, bool &truncated);
  bool remove_file(const char *path);
  bool make_recording_path(time_t timestamp, char *path, size_t path_size);
  bool request_list(const char *path, uint32_t request_id);
  bool take_list_result(FileEntry *entries, size_t capacity, size_t &count, bool &truncated, bool &success, uint32_t &request_id);
  bool request_remove_file(const char *path, uint32_t request_id);
  bool take_remove_result(bool &success, uint32_t &request_id);
}
