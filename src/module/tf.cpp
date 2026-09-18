#include "logger.h"
#include "config.h"
#include "config_file.h"
#include "module/tf.h"
#include "module/audio/buffer.h"
#include "module/audio/wav.h"

#include "FS.h"
#include "SD_MMC.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_core_dump.h"
#include "esp_partition.h"
#include "esp_system.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

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
  EXT_RAM_BSS_ATTR tf::FileEntry result_entries[TF_FILE_LIST_MAX];
  size_t result_count = 0;
  bool result_truncated = false;
  bool result_success = false;
  uint32_t result_request_id = 0;
  uint32_t request_id = 0;
  volatile tf::RequestStatus request_status = tf::RequestStatus::IDLE;
  tf::RecordingInfo recording_info = {};
  AudioRate recording_rate;
  AudioBit recording_bit;
  AudioChannel recording_channels;
  File recording_file;
  bool recording_open = false;
  audio::wav::Header recording_header;
  audio::buffer::RecordingReader recording_reader = {};
  EXT_RAM_BSS_ATTR uint8_t recording_data[32768];
  uint32_t recording_bytes = 0;
  uint32_t recording_sync_time = 0;
  uint32_t next_recording_number = 1;
  bool crash_export_pending = true;
  bool config_needs_check = true;
  uint32_t config_synced_generation = 0;
  uint32_t config_attempted_generation = 0;
  uint32_t config_write_failed_generation = 0;
  uint32_t last_invalid_config = 0;
  uint32_t last_config_attempt = 0;
  char config_text[config::file::CAPACITY + 1];
  File log_file;
  uint32_t log_bytes = 0, next_log_number = 1;
  uint8_t log_pending[1152];
  size_t log_pending_size = 0;
  uint32_t last_log_flush = 0;
  bool export_crash();
  void close_log();

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

  bool copy_entry_name(const char *source, char *destination)
  {
    const char *name = std::strrchr(source, '/');
    name = name ? name + 1 : source;
    const int written = std::snprintf(destination, TF_FILE_NAME_MAX, "%s", name);
    return written >= 0 && static_cast<size_t>(written) < TF_FILE_NAME_MAX;
  }

  bool initialize_directories()
  {
    return ensure_directory(TF_RECORDINGS_ROOT) && ensure_directory(TF_CONFIG_ROOT) &&
           ensure_directory(TF_LOG_ROOT);
  }

  // Called only with card ownership (at boot or from the storage worker).
  bool mount_card()
  {
    close_log();
    SD_MMC.end();
    mounted = false;
    storage_info.mounted = false;
    if (!SD_MMC.begin(TF_MOUNT_POINT, TF_1BIT_MODE, false, TF_FREQ))
      return false;
    if (!initialize_directories())
    {
      SD_MMC.end();
      return false;
    }
    refresh_storage_info();
    config_needs_check = true;
    config_synced_generation = 0;
    config_attempted_generation = 0;
    mounted = true;
    storage_info.mounted = true;
    return true;
  }

  bool recording_busy()
  {
    return recording_info.state == tf::RecordingState::STARTING ||
           recording_info.state == tf::RecordingState::RECORDING ||
           recording_info.state == tf::RecordingState::STOPPING;
  }

  void publish_recording(tf::RecordingState state, tf::RecordingError error = tf::RecordingError::NONE)
  {
    xSemaphoreTake(request_mutex, portMAX_DELAY);
    recording_info.state = state;
    recording_info.error = error;
    recording_info.bytes = recording_bytes;
    recording_info.revision++;
    xSemaphoreGive(request_mutex);
  }

  bool reserve_recording_path(char *path, size_t size)
  {
    File directory = SD_MMC.open(TF_RECORDINGS_ROOT);
    if (!directory || !directory.isDirectory())
      return false;
    // Scan actual media so switching cards/USB edits and reboots cannot overwrite
    // an existing file. The session counter also avoids reuse after deleting one.
    for (File file = directory.openNextFile(); file; file = directory.openNextFile())
    {
      const char *name = file.name();
      const char *base = std::strrchr(name, '/');
      name = base ? base + 1 : name;
      if (!file.isDirectory() && std::strncmp(name, "MIC", 3) == 0 &&
          name[3] >= '0' && name[3] <= '9')
      {
        char *end;
        const unsigned long number = std::strtoul(name + 3, &end, 10);
        if (std::strcmp(end, ".wav") == 0 && number < 100000 && number >= next_recording_number)
          next_recording_number = static_cast<uint32_t>(number) + 1;
      }
      file.close();
    }
    directory.close();
    while (next_recording_number <= 99999)
    {
      const int length = std::snprintf(path, size, TF_RECORDINGS_ROOT "/MIC%05lu.wav",
                                       static_cast<unsigned long>(next_recording_number++));
      if (length < 0 || static_cast<size_t>(length) >= size)
        return false;
      if (!SD_MMC.exists(path))
      {
        recording_file = SD_MMC.open(path, FILE_WRITE);
        recording_open = static_cast<bool>(recording_file);
        return static_cast<bool>(recording_file);
      }
    }
    return false;
  }

  bool sync_recording(bool final)
  {
    if (final && (recording_bytes & 1U))
    {
      const uint8_t padding = 0;
      if (!recording_file.seek(recording_header.size + recording_bytes) ||
          recording_file.write(&padding, 1) != 1)
        return false;
    }
    audio::wav::finish(recording_header, recording_bytes);
    if (!recording_file.seek(0) ||
        recording_file.write(recording_header.data, recording_header.size) != recording_header.size)
      return false;
    recording_file.flush();
    return recording_file.seek(recording_header.size + recording_bytes);
  }

  void finish_recording(tf::RecordingError error)
  {
    if (recording_open)
    {
      if (!sync_recording(true))
        error = tf::RecordingError::IO;
      recording_file.close();
      recording_open = false;
      if (error != tf::RecordingError::IO)
      {
        tf::RecordingInfo info;
        tf::get_recording_info(info);
        char path[80];
        std::snprintf(path, sizeof(path), TF_RECORDINGS_ROOT "/%s", info.filename);
        File saved = SD_MMC.open(path, FILE_READ);
        const uint32_t expected_size = recording_header.size + recording_bytes + (recording_bytes & 1U);
        if (!saved || saved.size() != expected_size ||
            saved.read(recording_data, recording_header.size) != recording_header.size ||
            std::memcmp(recording_data, recording_header.data, recording_header.size) != 0)
          error = tf::RecordingError::IO;
        saved.close();
      }
      if (error == tf::RecordingError::IO)
      {
        mounted = false;
        storage_info.mounted = false;
      }
      else
        refresh_storage_info();
    }
    LOGGER_INFO("TF recording ended: bytes=%lu error=%u stack_free=%lu",
                static_cast<unsigned long>(recording_bytes), static_cast<unsigned>(error),
                static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    publish_recording(error == tf::RecordingError::NONE ? tf::RecordingState::SAVED : tf::RecordingState::ERROR, error);
  }

  void service_recording()
  {
    tf::RecordingInfo info;
    tf::get_recording_info(info);
    if (info.state != tf::RecordingState::STARTING && info.state != tf::RecordingState::RECORDING &&
        info.state != tf::RecordingState::STOPPING)
      return;
    CardLock lock;
    if (!lock.acquired())
      return;
    if (info.state == tf::RecordingState::STARTING)
    {
      recording_bytes = 0;
      if (usb_storage_active)
      {
        publish_recording(tf::RecordingState::ERROR, tf::RecordingError::USB_BUSY);
        return;
      }
      if (!mounted && !mount_card())
      {
        publish_recording(tf::RecordingState::ERROR, tf::RecordingError::NO_CARD);
        return;
      }
      if (crash_export_pending)
        crash_export_pending = !export_crash();
      char path[80];
      if (!audio::wav::make(recording_header, recording_rate, recording_bit, recording_channels) ||
          !reserve_recording_path(path, sizeof(path)))
      {
        publish_recording(tf::RecordingState::ERROR, next_recording_number > 99999 ?
                          tf::RecordingError::LIMIT : tf::RecordingError::IO);
        return;
      }
      xSemaphoreTake(request_mutex, portMAX_DELAY);
      std::snprintf(recording_info.filename, sizeof(recording_info.filename), "%s", std::strrchr(path, '/') + 1);
      xSemaphoreGive(request_mutex);
      if (recording_file.write(recording_header.data, recording_header.size) != recording_header.size)
      {
        finish_recording(tf::RecordingError::IO);
        return;
      }
      audio::buffer::resetRecordingReader(recording_reader);
      recording_sync_time = millis();
      // Stop may have been requested while opening the file. Preserve it.
      xSemaphoreTake(request_mutex, portMAX_DELAY);
      if (recording_info.state == tf::RecordingState::STARTING)
        recording_info.state = tf::RecordingState::RECORDING;
      recording_info.revision++;
      xSemaphoreGive(request_mutex);
      LOGGER_INFO("TF recording started: %s %luHz/%ubit/%uch", path,
                  static_cast<unsigned long>(recording_rate), static_cast<unsigned>(recording_bit),
                  static_cast<unsigned>(recording_channels));
      return;
    }
    if (!recording_file)
    {
      if (recording_open)
        finish_recording(tf::RecordingError::IO);
      else
        publish_recording(tf::RecordingState::IDLE);
      return;
    }
    if (config::config.audio.rate != recording_rate || config::config.audio.bit != recording_bit ||
        config::config.audio.channel != recording_channels)
    {
      finish_recording(tf::RecordingError::FORMAT_CHANGED);
      return;
    }
    const bool stopping = info.state == tf::RecordingState::STOPPING;
    if (stopping && !recording_reader.stopping)
      audio::buffer::stopRecordingReader(recording_reader);
    const uint32_t frame_size = static_cast<uint32_t>(recording_rate) *
                                static_cast<uint32_t>(recording_bit) *
                                static_cast<uint32_t>(recording_channels) / 8 *
                                AUDIO_DECODER_POLLING_CYCLE / 1000;
    uint32_t count = 0;
    audio::buffer::RecordingRead read = audio::buffer::RecordingRead::WAIT;
    tf::RecordingError error = tf::RecordingError::NONE;
    while (count + frame_size <= sizeof(recording_data))
    {
      read = audio::buffer::readRecordingFrame(recording_reader, recording_data + count, frame_size, stopping);
      if (read != audio::buffer::RecordingRead::FRAME)
        break;
      count += frame_size;
    }
    if (count)
    {
      if (recording_bytes > UINT32_MAX - recording_header.size - count - 2)
        error = tf::RecordingError::LIMIT;
      else
      {
        const size_t written = recording_file.write(recording_data, count);
        // Only declare complete sample frames if the medium returns a short write.
        const uint32_t alignment = static_cast<uint32_t>(recording_bit) / 8 * recording_channels;
        recording_bytes += static_cast<uint32_t>(written) / alignment * alignment;
        if (written != count)
          error = tf::RecordingError::IO;
      }
    }
    if (read == audio::buffer::RecordingRead::RESET)
      error = tf::RecordingError::FORMAT_CHANGED;
    else if (read == audio::buffer::RecordingRead::OVERRUN)
      error = tf::RecordingError::OVERRUN;
    if (error != tf::RecordingError::NONE || (stopping && read == audio::buffer::RecordingRead::WAIT))
      finish_recording(error);
    else if (millis() - recording_sync_time >= 1000)
    {
      recording_sync_time = millis();
      if (!sync_recording(false))
        finish_recording(tf::RecordingError::IO);
    }
  }

  // Export the complete checksummed flash image, retaining it on any media
  // failure. Reading the saved file back is required before erasing flash.
  bool export_crash()
  {
    size_t address = 0, size = 0;
    const esp_err_t found = esp_core_dump_image_get(&address, &size);
    if (found == ESP_ERR_NOT_FOUND)
      return true;
    if (found != ESP_OK || esp_core_dump_image_check() != ESP_OK)
      return false;
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                               ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (!partition || address < partition->address || size > partition->size ||
        address - partition->address > partition->size - size)
      return false;
    char path[80], text_path[80];
    for (uint32_t n = 1; n <= 99999; ++n)
    {
      std::snprintf(path, sizeof(path), TF_LOG_ROOT "/CRASH%05lu.bin", static_cast<unsigned long>(n));
      std::snprintf(text_path, sizeof(text_path), TF_LOG_ROOT "/CRASH%05lu.txt", static_cast<unsigned long>(n));
      if (!SD_MMC.exists(path) && !SD_MMC.exists(text_path))
        break;
      if (n == 99999)
        return false;
    }
    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file)
      return false;
    // Reuse worker-owned PSRAM scratch, leaving the worker stack for filesystem
    // calls. No recording runs while this startup/remount export is in progress.
    bool success = true;
    for (size_t offset = 0; offset < size && success; offset += 1024)
    {
      const size_t count = size - offset < 1024 ? size - offset : 1024;
      success = esp_partition_read(partition, address - partition->address + offset, recording_data, count) == ESP_OK &&
                file.write(recording_data, count) == count;
    }
    file.flush();
    file.close();
    file = SD_MMC.open(path, FILE_READ);
    success = success && file && file.size() == size;
    for (size_t offset = 0; offset < size && success; offset += 1024)
    {
      const size_t count = size - offset < 1024 ? size - offset : 1024;
      success = esp_partition_read(partition, address - partition->address + offset, recording_data, count) == ESP_OK &&
                file.read(recording_data + 1024, count) == count &&
                std::memcmp(recording_data, recording_data + 1024, count) == 0;
    }
    file.close();
    if (success)
    {
      esp_core_dump_summary_t summary = {};
      char reason[160] = {};
      const bool has_summary = esp_core_dump_get_summary(&summary) == ESP_OK;
      esp_core_dump_get_panic_reason(reason, sizeof(reason));
      char text[1280];
      int length = std::snprintf(text, sizeof(text),
          "Project: %s\nAuthors: %s\nDevice: Receiver\nBoot reset reason: %u\n"
          "Image: %s\nImage bytes: %lu\nPanic: %s\nTask: %.16s\nPC: 0x%08lx\n"
          "Exception cause: %lu\nFault address: 0x%08lx\n"
          "Crashed ELF SHA256: %.64s\nKeep the matching firmware.elf for decoding this raw core dump.\n",
          MIC_PROJECT_NAME, AUTHOR, static_cast<unsigned>(esp_reset_reason()), path,
          static_cast<unsigned long>(size), reason, has_summary ? summary.exc_task : "unknown",
          static_cast<unsigned long>(summary.exc_pc), static_cast<unsigned long>(summary.ex_info.exc_cause),
          static_cast<unsigned long>(summary.ex_info.exc_vaddr),
          has_summary ? reinterpret_cast<const char *>(summary.app_elf_sha256) : "unknown");
      if (has_summary && length > 0 && static_cast<size_t>(length) < sizeof(text))
      {
        int added = std::snprintf(text + length, sizeof(text) - length, "Backtrace (corrupted=%u):\n",
                                  summary.exc_bt_info.corrupted ? 1U : 0U);
        length = added > 0 ? length + added : -1;
        const size_t maximum = sizeof(summary.exc_bt_info.bt) / sizeof(summary.exc_bt_info.bt[0]);
        for (size_t i = 0; i < summary.exc_bt_info.depth && i < maximum &&
                           length > 0 && static_cast<size_t>(length) < sizeof(text); ++i)
        {
          added = std::snprintf(text + length, sizeof(text) - length, "0x%08lx\n",
                                static_cast<unsigned long>(summary.exc_bt_info.bt[i]));
          length = added > 0 ? length + added : -1;
        }
      }
      file = SD_MMC.open(text_path, FILE_WRITE);
      success = length > 0 && static_cast<size_t>(length) < sizeof(text) && file &&
                file.write(reinterpret_cast<const uint8_t *>(text), static_cast<size_t>(length)) == static_cast<size_t>(length);
      file.flush();
      file.close();
      file = SD_MMC.open(text_path, FILE_READ);
      success = success && file && file.size() == static_cast<size_t>(length) &&
                file.read(recording_data, static_cast<size_t>(length)) == static_cast<size_t>(length) &&
                std::memcmp(recording_data, text, static_cast<size_t>(length)) == 0;
      file.close();
    }
    if (!success)
    {
      SD_MMC.remove(path);
      SD_MMC.remove(text_path);
      LOGGER_WARN("TF crash export failed; flash image retained.");
      return false;
    }
    const esp_err_t erased = esp_core_dump_image_erase();
    LOGGER_INFO("TF crash exported: %s erase=%s stack_free=%lu", path, esp_err_to_name(erased),
                static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    return erased == ESP_OK;
  }

  void close_log()
  {
    if (log_file) log_file.flush();
    log_file.close();
    log_bytes = 0;
  }

  bool write_config(const config::ConfigValue &value)
  {
    const size_t length = config::file::encode(value, config_text, sizeof(config_text));
    if (!length) return false;
    const char *path = TF_CONFIG_ROOT "/device.ini";
    const char *temp = TF_CONFIG_ROOT "/device.tmp";
    const char *backup = TF_CONFIG_ROOT "/device.bak";
    File file = SD_MMC.open(temp, FILE_WRITE);
    bool success = file && file.write(reinterpret_cast<const uint8_t *>(config_text), length) == length;
    file.flush(); file.close();
    file = SD_MMC.open(temp, FILE_READ);
    success = success && file && file.size() == length;
    uint8_t check[128];
    for (size_t offset = 0; offset < length && success; offset += sizeof(check))
    {
      const size_t count = length - offset < sizeof(check) ? length - offset : sizeof(check);
      success = file.read(check, count) == count && !std::memcmp(check, config_text + offset, count);
    }
    file.close();
    if (!success) { SD_MMC.remove(temp); return false; }
    // FAT rename cannot replace a target. Retain the last verified file until
    // the new file has been installed, and recover it if power fails between.
    if (SD_MMC.exists(backup) && !SD_MMC.remove(backup)) return false;
    const bool existing = SD_MMC.exists(path);
    if (existing && !SD_MMC.rename(path, backup)) return false;
    if (!SD_MMC.rename(temp, path))
    {
      if (existing) SD_MMC.rename(backup, path);
      return false;
    }
    if (existing) SD_MMC.remove(backup);
    return true;
  }

  void service_config()
  {
    config::ConfigValue candidate;
    uint32_t revision = 0;
    bool import = false;
    {
      CardLock lock;
      if (!lock.acquired() || !mounted || usb_storage_active || recording_open || !config::snapshot(candidate, revision)) return;
      tf::RecordingInfo recording;
      tf::get_recording_info(recording);
      if (recording.state == tf::RecordingState::STARTING || recording.state == tf::RecordingState::STOPPING) return;
      if (!config_needs_check && revision == config_synced_generation) return;
      config_attempted_generation = revision;
      const char *path = TF_CONFIG_ROOT "/device.ini";
      if (!SD_MMC.exists(path) && SD_MMC.exists(TF_CONFIG_ROOT "/device.bak"))
        if (!SD_MMC.rename(TF_CONFIG_ROOT "/device.bak", path)) return;
      File file = SD_MMC.open(path, FILE_READ);
      bool write = !SD_MMC.exists(path);
      if (!file && !write)
      {
        LOGGER_WARN("TF config open failed; original file retained.");
        config_needs_check = false;
        config_synced_generation = revision;
        return;
      }
      if (file)
      {
        const size_t length = file.size();
        bool readable = length > 0 && length <= config::file::CAPACITY &&
          file.read(reinterpret_cast<uint8_t *>(config_text), length) == length;
        file.close();
        if (readable) { config_text[length] = 0; readable = std::strlen(config_text) == length; }
        uint32_t raw_hash = 2166136261U;
        if (readable) for (size_t i = 0; i < length; ++i) raw_hash = (raw_hash ^ uint8_t(config_text[i])) * 16777619U;
        uint32_t baseline = 0;
        config::ConfigValue parsed;
        // Upgrade only the exact placeholder previously generated by firmware.
        static const char legacy[] = "; Wireless Microphone storage settings\n"
          "; UTF-8 text file, reserved for future runtime configuration\n"
          "[recording]\nformat=wav\nfilename=MIC00001.wav\n"
          "[storage]\nrecordings_dir=/recordings\nlogs_dir=/logs\n";
        if (readable && !std::strcmp(config_text, legacy)) write = true;
        else if (readable && config::file::decode(config_text, parsed, baseline))
        {
          const uint32_t hash = config::file::fingerprint(parsed);
          import = hash != baseline && hash != config::file::fingerprint(candidate);
          if (import) candidate = parsed;
          else write = hash != config::file::fingerprint(candidate) || hash != baseline;
        }
        else
        {
          // Preserve malformed user files for correction, including too-large
          // and partial writes. Never apply a subset of settings.
          if (last_invalid_config != raw_hash)
            LOGGER_WARN("TF config invalid/unreadable; original file and Preferences retained.");
          last_invalid_config = raw_hash;
          config_needs_check = false;
          config_synced_generation = revision;
          return;
        }
      }
      if (!import)
      {
        if (write && !write_config(candidate))
        {
          if (config_write_failed_generation != revision) LOGGER_WARN("TF config write failed; retry pending.");
          config_write_failed_generation = revision;
          return;
        }
        config_needs_check = false;
        config_synced_generation = revision;
        last_invalid_config = 0;
        config_write_failed_generation = 0;
      }
    }
    // Config application may request USB reader mode. It must happen outside
    // media ownership, and rejects a stale snapshot if the UI saved meanwhile.
    if (import && config::import_from_tf(candidate, revision))
    {
      CardLock lock;
      if (lock.acquired() && mounted && !usb_storage_active &&
          config::snapshot(candidate, revision) && write_config(candidate))
      {
        config_needs_check = false;
        config_synced_generation = revision;
        last_invalid_config = 0;
        config_attempted_generation = revision;
      }
    }
  }

  bool open_log()
  {
    File directory = SD_MMC.open(TF_LOG_ROOT);
    if (!directory || !directory.isDirectory()) return false;
    for (File file = directory.openNextFile(); file; file = directory.openNextFile())
    {
      const char *name = file.name(); const char *base = std::strrchr(name, '/'); if (base) name = base + 1;
      if (!file.isDirectory() && !std::strncmp(name,"LOG",3) && name[3]>='0' && name[3]<='9')
      {
        char *end; const unsigned long n = std::strtoul(name+3,&end,10);
        if (!std::strcmp(end,".txt") && n < 100000 && n >= next_log_number) next_log_number = n+1;
      }
      file.close();
    }
    directory.close();
    char path[80];
    while (next_log_number <= 99999)
    {
      std::snprintf(path,sizeof(path),TF_LOG_ROOT "/LOG%05lu.txt",static_cast<unsigned long>(next_log_number++));
      if (SD_MMC.exists(path)) continue;
      log_file = SD_MMC.open(path,FILE_WRITE);
      log_bytes = 0;
      return static_cast<bool>(log_file);
    }
    return false;
  }

  void service_logs()
  {
    if (millis() - last_log_flush < 250) return;
    last_log_flush = millis();
    CardLock lock;
    if (!lock.acquired() || !mounted || usb_storage_active) return;
    if (!log_pending_size)
    {
      uint32_t dropped = 0;
      // Leave space for a loss marker before the captured records.
      const size_t count = logger::take_logs(log_pending + 96, sizeof(log_pending) - 96, dropped);
      size_t prefix = 0;
      if (dropped)
        prefix = std::snprintf(reinterpret_cast<char *>(log_pending),96,"[TF log buffer full: %lu records dropped]\n",static_cast<unsigned long>(dropped));
      std::memmove(log_pending + prefix, log_pending + 96, count);
      log_pending_size = prefix + count;
    }
    if (!log_pending_size) return;
    if (log_file && log_bytes + log_pending_size > 1024 * 1024) close_log();
    if (!log_file && !open_log()) return;
    const size_t written = log_file.write(log_pending, log_pending_size);
    log_bytes += written;
    if (written)
    {
      std::memmove(log_pending,log_pending+written,log_pending_size-written);
      log_pending_size -= written;
    }
    log_file.flush();
    if (log_pending_size)
    {
      close_log();
      mounted = storage_info.mounted = false;
    }
  }

  void service_media()
  {
    CardLock lock;
    if (!lock.acquired() || usb_storage_active || recording_open) return;
    if (mounted)
    {
      uint8_t sector[512];
      if (!SD_MMC.readRAW(sector, 0))
      {
        close_log();
        mounted = storage_info.mounted = false;
        SD_MMC.end();
      }
      else config_needs_check = true;
    }
    else mount_card();
  }

  void tf_worker(void *)
  {
    uint32_t last_crash_attempt = millis() - 5000;
    uint32_t last_media_attempt = millis();
#ifdef BUILD_DEBUG
    uint32_t last_stack_report = millis();
#endif
    for (;;)
    {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
      if (millis() - last_media_attempt >= 2000)
      {
        last_media_attempt = millis();
        service_media();
      }
      if (millis() - last_config_attempt >= 250)
      {
        last_config_attempt = millis();
        service_config();
      }
      if (crash_export_pending && mounted && !usb_storage_active && !recording_file &&
          millis() - last_crash_attempt >= 5000)
      {
        last_crash_attempt = millis();
        CardLock lock;
        if (lock.acquired())
          crash_export_pending = !export_crash();
      }
      service_recording();
      service_logs();
#ifdef BUILD_DEBUG
      if (millis() - last_stack_report >= 10000)
      {
        last_stack_report = millis();
        LOGGER_INFO("TF task minimum free stack: %lu bytes", static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
      }
#endif
      RequestKind kind;
      uint32_t current_request_id;
      char path[TF_PATH_MAX];
      xSemaphoreTake(request_mutex, portMAX_DELAY);
      kind = request_kind;
      request_kind = RequestKind::NONE;
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

    const bool directories_ready = initialize_directories();
    if (!directories_ready)
    {
      LOGGER_WARN("TF storage directory initialization failed.");
    }
    else
    {
      config_needs_check = true;
      mounted = true;
      storage_info.mounted = true;
    }
  }
  else
  {
    LOGGER_WARN("TF card mount failed.");
  }

  // Import before USB's task can hand media to a host on this boot.
  service_config();
  // Remain available when booted without a card; Record retries mounting.
  if (xTaskCreatePinnedToCore(tf_worker, "tf_worker", TASK_TF_STACK, nullptr, TASK_TF_PRIORITY, &worker_handle, TASK_TF_CORE) != pdPASS)
  {
    worker_handle = nullptr;
    mounted = false;
    storage_info.mounted = false;
    LOGGER_WARN("TF worker creation failed.");
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

  // Match the worker's card -> state lock order; never keep the UI's short
  // state mutex held while waiting on filesystem I/O.
  CardLock lock;
  if (!lock.acquired())
    return false;
  // Attempt the latest UI export before host handoff. A failed media write must
  // still allow reader mode, so the user can repair/free space on the card.
  config::ConfigValue saved;
  uint32_t saved_generation = 0;
  if (active && config::snapshot(saved, saved_generation) &&
      config_attempted_generation != saved_generation) return false;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  // Completed replies live in RAM and do not own media. A paused file browser
  // may consume them later; they must not prevent entering reader mode.
  if (active && (!mounted || recording_busy() || request_status == RequestStatus::BUSY ||
                 storage_info.sector_size != 512 || storage_info.sector_count == 0))
  {
    xSemaphoreGive(request_mutex);
    return false;
  }

  const bool was_active = usb_storage_active;
  if (active)
  {
    close_log();
    usb_storage_active = true;
    storage_info.usb_active = true;
    xSemaphoreGive(request_mutex);
    return true;
  }
  // Keep ownership reserved while remounting, but release the short state lock
  // so LVGL status polling cannot block on slow media initialization.
  xSemaphoreGive(request_mutex);
  if (was_active && !mount_card())
  {
    storage_info.type = CardType::NONE;
    LOGGER_WARN("TF card remount after USB storage failed.");
  }
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  usb_storage_active = false;
  storage_info.usb_active = false;
  xSemaphoreGive(request_mutex);
  // Ownership is released even if the medium disappeared. The new USB mode can
  // start; TF UI reports unmounted and Record can retry after reinsertion.
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
  RecordingInfo info;
  get_recording_info(info);
  if ((info.state == RecordingState::STARTING || info.state == RecordingState::RECORDING ||
       info.state == RecordingState::STOPPING) && path && path_has_prefix(path, TF_RECORDINGS_ROOT))
    return false;
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

bool tf::start_recording()
{
  if (!request_mutex || !worker_handle)
    return false;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  if (recording_busy())
  {
    xSemaphoreGive(request_mutex);
    return false;
  }
  recording_info = {RecordingState::STARTING, RecordingError::NONE, recording_info.revision + 1, 0, {}};
  recording_rate = config::config.audio.rate;
  recording_bit = config::config.audio.bit;
  recording_channels = config::config.audio.channel;
  if (usb_storage_active)
  {
    recording_info.state = RecordingState::ERROR;
    recording_info.error = RecordingError::USB_BUSY;
  }
  xSemaphoreGive(request_mutex);
  xTaskNotifyGive(worker_handle);
  return true;
}

void tf::stop_recording()
{
  if (!request_mutex || !worker_handle)
    return;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  if (recording_info.state == RecordingState::RECORDING || recording_info.state == RecordingState::STARTING)
  {
    recording_info.state = RecordingState::STOPPING;
    recording_info.revision++;
  }
  xSemaphoreGive(request_mutex);
  xTaskNotifyGive(worker_handle);
}

void tf::get_recording_info(RecordingInfo &info)
{
  info = {};
  if (!request_mutex)
    return;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  info = recording_info;
  xSemaphoreGive(request_mutex);
}
