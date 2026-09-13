#include "logger.h"
#include "module/tf.h"
#include "module/usb/usb_device_msc.h"

#include "tusb.h"

#include <algorithm>
#include <cstring>

namespace
{
  constexpr uint32_t MSC_SECTOR_SIZE = 512;
  bool media_ejected = true;
  uint8_t sector_buffer[MSC_SECTOR_SIZE] __attribute__((aligned(4)));

  bool transfer_valid(uint32_t lba, uint32_t offset, uint32_t size, const tf::StorageInfo &info)
  {
    if (!info.mounted || !info.usb_active || info.sector_size != MSC_SECTOR_SIZE)
      return false;
    const uint64_t first_byte = static_cast<uint64_t>(lba) * info.sector_size + offset;
    const uint64_t capacity = static_cast<uint64_t>(info.sector_count) * info.sector_size;
    return first_byte <= capacity && size <= capacity - first_byte;
  }
}

void usb::msc::_connect()
{
  media_ejected = false;
}

void usb::msc::_disconnect()
{
  media_ejected = true;
}

extern "C" void tud_msc_inquiry_cb(uint8_t, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  const char vendor[] = "dudu233";
  const char product[] = "WirelessMic TF";
  const char revision[] = "1.0";
  std::memset(vendor_id, ' ', 8);
  std::memset(product_id, ' ', 16);
  std::memset(product_rev, ' ', 4);
  std::memcpy(vendor_id, vendor, sizeof(vendor) - 1);
  std::memcpy(product_id, product, sizeof(product) - 1);
  std::memcpy(product_rev, revision, sizeof(revision) - 1);
}

extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  if (lun != 0)
    return false;
  if (media_ejected || !tf::is_usb_storage_active())
  {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
    return false;
  }
  return true;
}

extern "C" void tud_msc_capacity_cb(uint8_t, uint32_t *block_count, uint16_t *block_size)
{
  tf::StorageInfo info;
  tf::get_info(info);
  *block_count = info.mounted ? info.sector_count : 0;
  *block_size = info.mounted ? info.sector_size : MSC_SECTOR_SIZE;
}

extern "C" bool tud_msc_start_stop_cb(uint8_t lun, uint8_t, bool start, bool load_eject)
{
  if (lun != 0)
    return false;
  if (load_eject)
    media_ejected = !start;
  return true;
}

extern "C" bool tud_msc_is_writable_cb(uint8_t lun)
{
  return lun == 0 && !media_ejected && tf::is_usb_storage_active();
}

extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  tf::StorageInfo info;
  tf::get_info(info);
  if (lun != 0 || media_ejected || !buffer || !transfer_valid(lba, offset, bufsize, info))
    return -1;

  uint8_t *destination = static_cast<uint8_t *>(buffer);
  uint32_t sector = lba + offset / info.sector_size;
  uint32_t sector_offset = offset % info.sector_size;
  uint32_t remaining = bufsize;
  while (remaining)
  {
    const uint32_t chunk = std::min<uint32_t>(remaining, info.sector_size - sector_offset);
    if (!tf::read_sector(sector, sector_buffer))
      return -1;
    std::memcpy(destination, sector_buffer + sector_offset, chunk);
    destination += chunk;
    remaining -= chunk;
    ++sector;
    sector_offset = 0;
  }
  return static_cast<int32_t>(bufsize);
}

extern "C" int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  tf::StorageInfo info;
  tf::get_info(info);
  if (lun != 0 || !buffer || media_ejected || !transfer_valid(lba, offset, bufsize, info))
    return -1;

  const uint8_t *source = buffer;
  uint32_t sector = lba + offset / info.sector_size;
  uint32_t sector_offset = offset % info.sector_size;
  uint32_t remaining = bufsize;
  while (remaining)
  {
    const uint32_t chunk = std::min<uint32_t>(remaining, info.sector_size - sector_offset);
    if (sector_offset != 0 || chunk != info.sector_size)
    {
      if (!tf::read_sector(sector, sector_buffer))
        return -1;
    }
    std::memcpy(sector_buffer + sector_offset, source, chunk);
    if (!tf::write_sector(sector, sector_buffer))
      return -1;
    source += chunk;
    remaining -= chunk;
    ++sector;
    sector_offset = 0;
  }
  return static_cast<int32_t>(bufsize);
}

extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *, uint16_t)
{
  tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
  LOGGER_DEBUG("Unsupported MSC SCSI command: 0x%02x", scsi_cmd[0]);
  return -1;
}
