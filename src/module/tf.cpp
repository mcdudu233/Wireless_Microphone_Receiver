#include "logger.h"
#include "config.h"
#include "module/tf.h"

#include "FS.h"
#include "SD_MMC.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstring>

namespace
{
  volatile bool mounted = false;
  volatile bool usb_storage_active = false;
  tf::StorageInfo storage_info = {};
  StaticSemaphore_t card_mutex_buffer;
  SemaphoreHandle_t card_mutex = nullptr;
  StaticSemaphore_t request_mutex_buffer;
  SemaphoreHandle_t request_mutex = nullptr;
  TaskHandle_t worker_handle = nullptr;
  enum class RequestKind : uint8_t { NONE, LIST, REMOVE };
  volatile RequestKind request_kind = RequestKind::NONE;
  char request_path[TF_PATH_MAX];
  tf::FileEntry result_entries[TF_FILE_LIST_MAX];
  size_t result_count = 0;
  bool result_truncated = false;
  bool result_success = false;
  uint32_t result_request_id = 0;
  uint32_t request_id = 0;
  volatile tf::RequestStatus request_status = tf::RequestStatus::IDLE;

  class CardLock
  {
  public:
    CardLock() : locked(card_mutex && xSemaphoreTake(card_mutex, pdMS_TO_TICKS(250)) == pdTRUE) {}
    ~CardLock()
    {
      if (locked)
        xSemaphoreGive(card_mutex);
    }
    bool acquired() const { return locked; }

  private:
    bool locked;
  };

  bool valid_path(const char *path)
  {
    if (!path || path[0] != '/')
      return false;

    const char *segment = path + 1;
    const size_t length = std::strlen(path);
    if (length == 0 || (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')))
      return false;
    while (*segment)
    {
      const char *end = std::strchr(segment, '/');
      size_t length = end ? static_cast<size_t>(end - segment) : std::strlen(segment);
      if (length == 0 || (length == 1 && segment[0] == '.') ||
          (length == 2 && segment[0] == '.' && segment[1] == '.') ||
          std::memchr(segment, '\\', length) ||
          segment[length - 1] == '.' || segment[length - 1] == ' ')
        return false;
      if (!end)
        break;
      segment = end + 1;
    }
    if (length == 1)
      return true;
    return length < TF_PATH_MAX;
  }

  bool path_has_prefix(const char *path, const char *prefix)
  {
    while (*prefix && *path)
    {
      char path_char = *path++;
      char prefix_char = *prefix++;
      if (path_char >= 'a' && path_char <= 'z')
        path_char = static_cast<char>(path_char - 'a' + 'A');
      if (prefix_char >= 'a' && prefix_char <= 'z')
        prefix_char = static_cast<char>(prefix_char - 'a' + 'A');
      if (path_char != prefix_char)
        return false;
    }
    return *prefix == '\0' && (*path == '\0' || *path == '/');
  }

  bool deletable_path(const char *path)
  {
    return path_has_prefix(path, TF_RECORDINGS_ROOT) || path_has_prefix(path, TF_LOG_ROOT);
  }

  tf::CardType current_card_type()
  {
    switch (SD_MMC.cardType())
    {
    case CARD_MMC:
      return tf::CardType::MMC;
    case CARD_SD:
      return tf::CardType::SD;
    case CARD_SDHC:
      return tf::CardType::SDHC;
    default:
      return tf::CardType::UNKNOWN;
    }
  }

  void refresh_storage_info()
  {
    const int sector_size = SD_MMC.sectorSize();
    const int sector_count = SD_MMC.numSectors();
    storage_info.type = current_card_type();
    storage_info.capacity_bytes = SD_MMC.cardSize();
    storage_info.total_bytes = SD_MMC.totalBytes();
    storage_info.used_bytes = SD_MMC.usedBytes();
    storage_info.sector_size = sector_size > 0 ? static_cast<uint16_t>(sector_size) : 0;
    storage_info.sector_count = sector_count > 0 ? static_cast<uint32_t>(sector_count) : 0;
  }

  bool ensure_directory(const char *path)
  {
    if (SD_MMC.exists(path))
    {
      File existing = SD_MMC.open(path);
      const bool is_directory = existing && existing.isDirectory();
      if (existing)
        existing.close();
      return is_directory;
    }
    return SD_MMC.mkdir(path);
  }

  void write_default_config()
  {
    const char *config_path = TF_CONFIG_ROOT "/device.ini";
    if (SD_MMC.exists(config_path))
      return;

    File file = SD_MMC.open(config_path, FILE_WRITE);
    if (!file)
    {
      LOGGER_WARN("TF default config creation failed.");
      return;
    }
    file.print("; Wireless Microphone storage settings\n"
               "; UTF-8 text file, reserved for future runtime configuration\n"
               "[recording]\n"
               "format=wav\n"
               "filename_time=local\n"
               "[storage]\n"
               "recordings_dir=/WirelessMic/recordings\n");
    file.close();
  }

  bool copy_entry_name(const char *source, char *destination)
  {
    const char *name = std::strrchr(source, '/');
    name = name ? name + 1 : source;
    const int written = std::snprintf(destination, TF_FILE_NAME_MAX, "%s", name);
    return written >= 0 && static_cast<size_t>(written) < TF_FILE_NAME_MAX;
  }

  void tf_worker(void *)
  {
    for (;;)
    {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      RequestKind kind;
      uint32_t current_request_id;
      char path[TF_PATH_MAX];
      xSemaphoreTake(request_mutex, portMAX_DELAY);
      kind = request_kind;
      current_request_id = request_id;
      std::snprintf(path, sizeof(path), "%s", request_path);
      xSemaphoreGive(request_mutex);

      if (kind == RequestKind::LIST)
      {
        size_t count = 0;
        bool truncated = false;
        const bool success = tf::list(path, result_entries, TF_FILE_LIST_MAX, count, truncated);
        xSemaphoreTake(request_mutex, portMAX_DELAY);
        result_count = count;
        result_truncated = truncated;
        result_success = success;
        result_request_id = current_request_id;
        request_status = tf::RequestStatus::LIST_READY;
        xSemaphoreGive(request_mutex);
      }
      else if (kind == RequestKind::REMOVE)
      {
        const bool success = tf::remove_file(path);
        xSemaphoreTake(request_mutex, portMAX_DELAY);
        result_success = success;
        result_request_id = current_request_id;
        request_status = tf::RequestStatus::REMOVE_READY;
        xSemaphoreGive(request_mutex);
      }
    }
  }
}

void tf::setup()
{
  LOGGER_INFO("TF card is starting...");
  card_mutex = xSemaphoreCreateMutexStatic(&card_mutex_buffer);
  request_mutex = xSemaphoreCreateMutexStatic(&request_mutex_buffer);

  if (TF_1BIT_MODE)
  {
    if (!SD_MMC.setPins(TF_CLK_IO, TF_CMD_IO, TF_D0_IO))
    { // 1-bit line version
      LOGGER_INFO("TF card pin change failed!");
      return;
    }
  }
  else if (!SD_MMC.setPins(TF_CLK_IO, TF_CMD_IO, TF_D0_IO, TF_D1_IO, TF_D2_IO, TF_D3_IO))
  { // 4-bit line version
    LOGGER_INFO("TF card pin change failed!");
    return;
  }

  // 检测TF卡是否插入
  if (SD_MMC.begin(TF_MOUNT_POINT, TF_1BIT_MODE, false, TF_FREQ))
  {
    LOGGER_INFO("TF card is mounted.");
    refresh_storage_info();
    switch (storage_info.type)
    {
    case CardType::MMC:
    {
      LOGGER_INFO("TF card type is MMC.");
      break;
    }
    case CardType::SD:
    {
      LOGGER_INFO("TF card type is SD.");
      break;
    }
    case CardType::SDHC:
    {
      LOGGER_INFO("TF card type is SDHC.");
      break;
    }
    default:
    {
      LOGGER_WARN("TF card type is error!");
      break;
    }
    }
    LOGGER_INFO("TF card Size: %dMB\n", SD_MMC.cardSize() / (1024 * 1024));

    const bool directories_ready = ensure_directory(TF_DATA_ROOT) &&
                                   ensure_directory(TF_RECORDINGS_ROOT) &&
                                   ensure_directory(TF_CONFIG_ROOT) &&
                                   ensure_directory(TF_LOG_ROOT);
    if (!directories_ready)
    {
      LOGGER_WARN("TF storage directory initialization failed.");
    }
    else
    {
      write_default_config();
      mounted = true;
      storage_info.mounted = true;
      if (xTaskCreatePinnedToCore(tf_worker, "tf_worker", TASK_TF_STACK, nullptr, TASK_TF_PRIORITY, &worker_handle, TASK_TF_CORE) != pdPASS)
      {
        worker_handle = nullptr;
        mounted = false;
        storage_info.mounted = false;
        LOGGER_WARN("TF worker creation failed.");
      }
    }
  }
  else
  {
    LOGGER_WARN("TF card mount failed.");
  }

  LOGGER_INFO("TF card is started.");
}

bool tf::is_mounted()
{
  return mounted;
}

void tf::get_info(StorageInfo &info)
{
  info = storage_info;
  info.mounted = mounted;
  info.usb_active = usb_storage_active;
}

bool tf::set_usb_storage_active(bool active)
{
  if (!card_mutex || !request_mutex)
    return !active;

  xSemaphoreTake(request_mutex, portMAX_DELAY);
  if (active && (!mounted || request_status != RequestStatus::IDLE ||
                 storage_info.sector_size != 512 || storage_info.sector_count == 0))
  {
    xSemaphoreGive(request_mutex);
    return false;
  }

  CardLock lock;
  if (!lock.acquired())
  {
    xSemaphoreGive(request_mutex);
    return false;
  }

  usb_storage_active = active;
  storage_info.usb_active = active;
  if (!active && mounted)
  {
    mounted = false;
    storage_info.mounted = false;
    SD_MMC.end();
    if (SD_MMC.begin(TF_MOUNT_POINT, TF_1BIT_MODE, false, TF_FREQ))
    {
      refresh_storage_info();
      mounted = true;
      storage_info.mounted = true;
      if (!(ensure_directory(TF_DATA_ROOT) &&
            ensure_directory(TF_RECORDINGS_ROOT) &&
            ensure_directory(TF_CONFIG_ROOT) &&
            ensure_directory(TF_LOG_ROOT)))
        LOGGER_WARN("TF storage directory remount check failed.");
      else
        write_default_config();
    }
    else
    {
      storage_info.type = CardType::NONE;
      LOGGER_WARN("TF card remount after USB storage failed.");
    }
  }
  xSemaphoreGive(request_mutex);
  return true;
}

bool tf::is_usb_storage_active()
{
  return usb_storage_active;
}

bool tf::read_sector(uint32_t sector, uint8_t *buffer)
{
  if (!usb_storage_active || !buffer || sector >= storage_info.sector_count)
    return false;
  CardLock lock;
  return lock.acquired() && usb_storage_active && SD_MMC.readRAW(buffer, sector);
}

bool tf::write_sector(uint32_t sector, const uint8_t *buffer)
{
  if (!usb_storage_active || !buffer || sector >= storage_info.sector_count)
    return false;
  CardLock lock;
  return lock.acquired() && usb_storage_active && SD_MMC.writeRAW(const_cast<uint8_t *>(buffer), sector);
}

bool tf::is_deletable_path(const char *path)
{
  return mounted && !usb_storage_active && valid_path(path) && deletable_path(path);
}

bool tf::list(const char *path, FileEntry *entries, size_t capacity, size_t &count, bool &truncated)
{
  count = 0;
  truncated = false;
  if (!mounted || usb_storage_active || !entries || capacity == 0 || !valid_path(path))
    return false;

  CardLock lock;
  if (!lock.acquired() || usb_storage_active)
    return false;

  File root = SD_MMC.open(path);
  if (!root || !root.isDirectory())
  {
    if (root)
      root.close();
    return false;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (count < capacity)
    {
      entries[count].name_complete = copy_entry_name(file.name(), entries[count].name);
      entries[count].size = file.isDirectory() ? 0 : file.size();
      entries[count].directory = file.isDirectory();
      ++count;
    }
    else
    {
      truncated = true;
      file.close();
      break;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  for (size_t i = 1; i < count; ++i)
  {
    FileEntry value = entries[i];
    size_t j = i;
    while (j > 0 && entries[j - 1].directory < value.directory)
    {
      entries[j] = entries[j - 1];
      --j;
    }
    while (j > 0 && entries[j - 1].directory == value.directory &&
           std::strcmp(entries[j - 1].name, value.name) > 0)
    {
      entries[j] = entries[j - 1];
      --j;
    }
    entries[j] = value;
  }
  return true;
}

bool tf::remove_file(const char *path)
{
  if (usb_storage_active || !is_deletable_path(path))
    return false;

  CardLock lock;
  if (!lock.acquired() || usb_storage_active)
    return false;
  File file = SD_MMC.open(path);
  if (!file || file.isDirectory())
  {
    if (file)
      file.close();
    return false;
  }
  file.close();
  return SD_MMC.remove(path);
}

bool tf::request_list(const char *path, uint32_t request_id_value)
{
  if (!mounted || usb_storage_active || !valid_path(path) || !request_mutex || !worker_handle)
    return false;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  if (usb_storage_active || request_status != RequestStatus::IDLE)
  {
    xSemaphoreGive(request_mutex);
    return false;
  }
  std::snprintf(request_path, sizeof(request_path), "%s", path);
  request_id = request_id_value;
  request_kind = RequestKind::LIST;
  request_status = RequestStatus::BUSY;
  xSemaphoreGive(request_mutex);
  xTaskNotifyGive(worker_handle);
  return true;
}

bool tf::take_list_result(FileEntry *entries, size_t capacity, size_t &count, bool &truncated, bool &success, uint32_t &request_id_value)
{
  if (!entries || capacity == 0 || !request_mutex)
    return false;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  if (request_status != RequestStatus::LIST_READY)
  {
    xSemaphoreGive(request_mutex);
    return false;
  }
  count = result_count < capacity ? result_count : capacity;
  std::memcpy(entries, result_entries, count * sizeof(FileEntry));
  truncated = result_truncated || result_count > capacity;
  success = result_success;
  request_id_value = result_request_id;
  request_status = RequestStatus::IDLE;
  xSemaphoreGive(request_mutex);
  return true;
}

bool tf::request_remove_file(const char *path, uint32_t request_id_value)
{
  if (!mounted || usb_storage_active || !valid_path(path) || !request_mutex || !worker_handle)
    return false;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  if (usb_storage_active || request_status != RequestStatus::IDLE)
  {
    xSemaphoreGive(request_mutex);
    return false;
  }
  std::snprintf(request_path, sizeof(request_path), "%s", path);
  request_id = request_id_value;
  request_kind = RequestKind::REMOVE;
  request_status = RequestStatus::BUSY;
  xSemaphoreGive(request_mutex);
  xTaskNotifyGive(worker_handle);
  return true;
}

bool tf::take_remove_result(bool &success, uint32_t &request_id_value)
{
  if (!request_mutex)
    return false;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  if (request_status != RequestStatus::REMOVE_READY)
  {
    xSemaphoreGive(request_mutex);
    return false;
  }
  success = result_success;
  request_id_value = result_request_id;
  request_status = RequestStatus::IDLE;
  xSemaphoreGive(request_mutex);
  return true;
}

bool tf::make_recording_path(time_t timestamp, char *path, size_t path_size)
{
  if (!mounted || usb_storage_active || !path || path_size == 0)
    return false;

  struct tm local_time;
  if (!localtime_r(&timestamp, &local_time) || local_time.tm_year < 124)
    return false;

  char day_path[TF_PATH_MAX];
  std::snprintf(day_path, sizeof(day_path), "%s/%04d/%02d/%02d", TF_RECORDINGS_ROOT,
                local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday);

  CardLock lock;
  if (!lock.acquired() || usb_storage_active || !ensure_directory(TF_RECORDINGS_ROOT))
    return false;
  char year_path[TF_PATH_MAX];
  char month_path[TF_PATH_MAX];
  std::snprintf(year_path, sizeof(year_path), "%s/%04d", TF_RECORDINGS_ROOT, local_time.tm_year + 1900);
  std::snprintf(month_path, sizeof(month_path), "%s/%02d", year_path, local_time.tm_mon + 1);
  if (!ensure_directory(year_path) || !ensure_directory(month_path) || !ensure_directory(day_path))
    return false;

  for (unsigned int suffix = 0; suffix < 100; ++suffix)
  {
    int written;
    if (suffix == 0)
      written = std::snprintf(path, path_size, "%s/REC_%04d%02d%02d_%02d%02d%02d.wav", day_path,
                              local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
                              local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    else
      written = std::snprintf(path, path_size, "%s/REC_%04d%02d%02d_%02d%02d%02d_%02u.wav", day_path,
                              local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
                              local_time.tm_hour, local_time.tm_min, local_time.tm_sec, suffix);
    if (written < 0 || static_cast<size_t>(written) >= path_size)
      return false;
    if (!SD_MMC.exists(path))
    {
      File reserved = SD_MMC.open(path, FILE_WRITE);
      if (!reserved)
        return false;
      reserved.close();
      return true;
    }
  }
  return false;
}
