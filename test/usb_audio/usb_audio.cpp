#include "../../src/module/usb/usb.cpp"
#include "module/audio/buffer.h"
#include "device/dcd.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace config { ConfigValue config; void save() {} }
static int64_t clock_us;
static bool jtag_pad, attached, controller_live, storage_owned;
static bool fail_phy, fail_controller, fail_storage_release;
static unsigned fifo_words;
static std::array<unsigned, 256> allocated;
struct Transfer { bool pending; uint8_t *buffer; uint16_t size; std::vector<uint8_t> bytes; };
static std::array<Transfer, 256> transfers;
static bool stalled;
static std::function<void()> during_detach;
static constexpr uint32_t card_sectors = 100000;
static std::map<uint32_t, std::array<uint8_t,512>> disk_writes;
static bool fail_sector_io;
int64_t esp_timer_get_time() { return clock_us; }
void vTaskDelay(TickType_t ticks) {
  if (ticks == 250) {
    assert(!attached && !jtag_pad && !controller_live);
    if (during_detach) { auto cb = during_detach; during_detach = {}; cb(); }
  }
  clock_us += ticks * 1000;
}
void usb_serial_jtag_ll_phy_enable_pad(bool enabled) { jtag_pad = enabled; }
int usb_new_phy(const usb_phy_config_t *conf, usb_phy_handle_t *phy) {
  assert(*phy == nullptr && !attached && !controller_live);
  // ESP-IDF usb_new_phy aborts when a Serial/JTAG handle is given OTG mode
  // settings. The fake must enforce the real API's controller restriction.
  assert(conf->otg_mode == USB_PHY_MODE_DEFAULT || conf->controller == USB_PHY_CTRL_OTG);
  if (fail_phy) { fail_phy = false; return -1; }
  *phy = new int(conf->controller);
  return ESP_OK;
}
int usb_del_phy(usb_phy_handle_t phy) {
  assert(!attached && !controller_live && !jtag_pad);
  delete phy;
  return ESP_OK;
}
namespace tf {
  bool is_mounted() { return true; }
  void get_recording_info(RecordingInfo &info) { info={}; }
  void get_info(StorageInfo &info) { info={true,storage_owned,CardType::SDHC,card_sectors*512ULL,98304*512ULL,0,card_sectors,512}; }
  bool is_usb_storage_active() { return storage_owned; }
  bool read_sector(uint32_t sector,uint8_t *buffer) {
    if(!storage_owned || sector>=card_sectors || fail_sector_io)return false;
    if(disk_writes.count(sector))memcpy(buffer,disk_writes.at(sector).data(),512);
    else for(unsigned i=0;i<512;i++)buffer[i]=static_cast<uint8_t>(sector*7+i*37+19);
    return true;
  }
  bool write_sector(uint32_t sector,const uint8_t *buffer) {
    if(!storage_owned || sector>=card_sectors || fail_sector_io)return false;
    memcpy(disk_writes[sector].data(),buffer,512);return true;
  }
  bool set_usb_storage_active(bool active) {
    assert(!attached && !controller_live);
    if (!active && fail_storage_release) { fail_storage_release = false; return false; }
    storage_owned = active; return true;
  }
}
extern "C" {
uint32_t tusb_time_millis_api() { return clock_us / 1000; }
bool dcd_init(uint8_t, const tusb_rhport_init_t *) {
  assert(usb_phy && *usb_phy == USB_PHY_CTRL_OTG);
  controller_live = true; allocated.fill(0); fifo_words = 62 + 16;
  if (fail_controller) { fail_controller = false; return false; }
  attached = true; return true;
}
bool dcd_deinit(uint8_t) {
  assert(!attached); controller_live = false;
  for (auto &transfer : transfers) transfer = {};
  return true;
}
void dcd_int_enable(uint8_t) {}
void dcd_int_disable(uint8_t) {}
void dcd_int_handler(uint8_t) {}
void dcd_set_address(uint8_t, uint8_t) {}
void dcd_remote_wakeup(uint8_t) {}
void dcd_connect(uint8_t) { attached = true; }
void dcd_disconnect(uint8_t) { attached = false; }
void dcd_sof_enable(uint8_t, bool) {}
bool dcd_edpt_open(uint8_t, const tusb_desc_endpoint_t *ep) {
  const uint8_t address = ep->bEndpointAddress;
  if (address & 0x80) {
    const unsigned words = (tu_edpt_packet_size(ep) + 3) / 4;
    if (!allocated[address]) { allocated[address] = words; fifo_words += words; }
    assert(fifo_words <= 256);
  } else assert(tu_edpt_packet_size(ep) <= 64);
  return true;
}
bool dcd_edpt_iso_alloc(uint8_t, uint8_t address, uint16_t size) {
  assert(!allocated[address]); allocated[address] = (size + 3) / 4;
  fifo_words += allocated[address]; assert(fifo_words <= 256); return true;
}
bool dcd_edpt_iso_activate(uint8_t port, const tusb_desc_endpoint_t *ep) { return dcd_edpt_open(port, ep); }
void dcd_edpt_close_all(uint8_t) {
  allocated.fill(0); fifo_words = 62 + 16;
  for (auto &transfer : transfers) transfer = {};
}
bool dcd_edpt_xfer(uint8_t, uint8_t address, uint8_t *buffer, uint16_t size, bool) {
  assert(controller_live);
  if(usb::active_mode()==USB_MODE_SD && (address&0x7f))assert((size+63U)/64U<=127);
  auto &transfer = transfers[address];
  transfer = {true, buffer, size, {}};
  if ((address & 0x80) && size) transfer.bytes.assign(buffer, buffer + size);
  return true;
}
void dcd_edpt_stall(uint8_t, uint8_t) { stalled = true; }
void dcd_edpt_clear_stall(uint8_t, uint8_t) {}
}

static void pump() {
  service_device();
}
static std::vector<uint8_t> control(uint8_t type, uint8_t request, uint16_t value, uint16_t index,
                                    uint16_t size, const std::vector<uint8_t> &payload = {}) {
  tusb_control_request_t setup{};
  setup.bmRequestType = type; setup.bRequest = request; setup.wValue = value; setup.wIndex = index; setup.wLength = size;
  stalled = false; transfers[0] = {}; transfers[0x80] = {};
  dcd_event_setup_received(0, reinterpret_cast<uint8_t *>(&setup), false);
  tud_task_ext(0, false);
  std::vector<uint8_t> result;
  for (unsigned step = 0; step < 20; ++step) {
    const uint8_t address = transfers[0x80].pending ? 0x80 : 0;
    auto transfer = transfers[address];
    if (!transfer.pending) return result;
    transfers[address].pending = false;
    if (address) result.insert(result.end(), transfer.bytes.begin(), transfer.bytes.end());
    else if (transfer.size) { assert(payload.size() == transfer.size); memcpy(transfer.buffer, payload.data(), payload.size()); }
    dcd_event_xfer_complete(0, address, transfer.size, XFER_RESULT_SUCCESS, false);
    tud_task_ext(0, false);
  }
  assert(false); return result;
}
static void enumerate() {
  allocated.fill(0); fifo_words = 62 + 16;
  dcd_event_bus_reset(0, TUSB_SPEED_FULL, false); tud_task_ext(0, false);
  const auto device = control(0x80, TUSB_REQ_GET_DESCRIPTOR, TUSB_DESC_DEVICE << 8, 0, 18);
  assert(!stalled && device.size() == 18);
  const auto descriptor = control(0x80, TUSB_REQ_GET_DESCRIPTOR, TUSB_DESC_CONFIGURATION << 8, 0, 512);
  assert(!stalled && descriptor.size() >= 9 && tu_unaligned_read16(&descriptor[2]) == descriptor.size());
  control(0, TUSB_REQ_SET_CONFIGURATION, 1, 0, 0);
  assert(!stalled && tud_mounted());
}
struct BulkResult { uint8_t status; uint32_t residue; std::vector<uint8_t> data; unsigned chunks; };
static BulkResult scsi(const std::array<uint8_t,16> &command,unsigned length=0,bool in=true,
                       const std::vector<uint8_t> &payload={}) {
  static uint32_t tag;
  msc_cbw_t cbw{};cbw.signature=MSC_CBW_SIGNATURE;cbw.tag=++tag;
  cbw.total_bytes=length;cbw.dir=in?0x80:0;cbw.cmd_len=command[0]==SCSI_CMD_REQUEST_SENSE?6:10;
  memcpy(cbw.command,command.data(),command.size());
  assert(transfers[1].pending && transfers[1].size==sizeof(cbw));
  memcpy(transfers[1].buffer,&cbw,sizeof(cbw));transfers[1].pending=false;
  stalled=false;dcd_event_xfer_complete(0,1,sizeof(cbw),XFER_RESULT_SUCCESS,false);tud_task_ext(0,false);
  BulkResult result{};unsigned sent=0;
  for(unsigned step=0;step<1024;step++) {
    if(stalled) {
      control(2,TUSB_REQ_CLEAR_FEATURE,TUSB_REQ_FEATURE_EDPT_HALT,0x81,0);
      control(2,TUSB_REQ_CLEAR_FEATURE,TUSB_REQ_FEATURE_EDPT_HALT,1,0);
    }
    if(transfers[0x81].pending) {
      auto transfer=transfers[0x81];transfers[0x81].pending=false;
      if(transfer.size==sizeof(msc_csw_t) && tu_unaligned_read32(transfer.bytes.data())==MSC_CSW_SIGNATURE) {
        assert(tu_unaligned_read32(transfer.bytes.data()+4)==tag);
        result.residue=tu_unaligned_read32(transfer.bytes.data()+8);result.status=transfer.bytes[12];
        dcd_event_xfer_complete(0,0x81,transfer.size,XFER_RESULT_SUCCESS,false);tud_task_ext(0,false);
        return result;
      }
      result.data.insert(result.data.end(),transfer.bytes.begin(),transfer.bytes.end());++result.chunks;
      dcd_event_xfer_complete(0,0x81,transfer.size,XFER_RESULT_SUCCESS,false);tud_task_ext(0,false);
    } else if(transfers[1].pending) {
      auto transfer=transfers[1];assert(!in && sent+transfer.size<=payload.size());
      memcpy(transfer.buffer,payload.data()+sent,transfer.size);sent+=transfer.size;transfers[1].pending=false;++result.chunks;
      dcd_event_xfer_complete(0,1,transfer.size,XFER_RESULT_SUCCESS,false);tud_task_ext(0,false);
    } else assert(false);
  }
  assert(false);return result;
}
static std::array<uint8_t,16> rw_command(uint8_t command,uint32_t lba,uint16_t blocks) {
  std::array<uint8_t,16> c{};c[0]=command;tu_unaligned_write32(c.data()+2,tu_htonl(lba));
  tu_unaligned_write16(c.data()+7,tu_htons(blocks));return c;
}
static std::string serial() {
  const auto str = tud_descriptor_string_cb(3, 0);
  std::string result;
  for (unsigned i = 1; i < (str[0] & 255) / 2; ++i) result += static_cast<char>(str[i]);
  return result;
}
static void check_descriptor(const usb::AudioFormat &format) {
  const auto *desc = tud_descriptor_configuration_cb(0);
  const unsigned total = tu_unaligned_read16(desc + 2);
  unsigned formats = 0, channels = 0, feature_length = 0, clock_controls = 0;
  unsigned ac_start = 0, ac_size = 0, ac_end = 0;
  unsigned interface_number = 0, interfaces = 0, endpoints = 0;
  assert(desc[4] == 2 && ITF_NUM_TOTAL == 2 && CFG_TUD_CDC == 0);
  for (unsigned offset = 9; offset < total; offset += desc[offset]) {
    const auto *item = desc + offset; assert(item[0] > 0 && offset + item[0] <= total);
    if (item[1] == TUSB_DESC_INTERFACE) {
      interface_number = item[2]; assert(interface_number < 2 && item[5] == TUSB_CLASS_AUDIO); ++interfaces;
    }
    if (item[1] == TUSB_DESC_CS_INTERFACE) {
      if (item[0] == 9 && item[2] == AUDIO20_CS_AC_INTERFACE_HEADER) { ac_start = offset; ac_size = tu_unaligned_read16(item + 6); }
      if (item[0] == 8 && item[2] == AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE) clock_controls = item[5];
      if (item[0] == 17 && item[2] == AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL) {
        assert(item[8] == format.channels); ++channels;
      }
      if (item[2] == AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT) { feature_length = item[0]; ac_end = offset + item[0]; }
      if (item[0] == 16 && item[2] == AUDIO20_CS_AS_INTERFACE_AS_GENERAL) { assert(item[10] == format.channels); ++channels; }
      if (item[0] == 6 && item[2] == AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE) {
        assert(item[4] == format.bit / 8 && item[5] == format.bit); ++formats;
      }
    }
    if (item[1] == TUSB_DESC_ENDPOINT) {
      assert(item[2] == 0x81 && (item[3] & 3) == TUSB_XFER_ISOCHRONOUS);
      assert(tu_unaligned_read16(item + 4) == format.packet_bytes()); ++endpoints;
    }
  }
  assert(formats == 1 && channels == 2 && feature_length == 6U + (format.channels + 1U) * 4U);
  assert(clock_controls == 5 && ac_start + ac_size == ac_end);
  assert(interfaces == 3 && endpoints == 1 && tud_descriptor_string_cb(5, 0) == nullptr);
  assert(serial().find("-U") != std::string::npos && serial().find("-A") == std::string::npos);
  assert(tud_descriptor_configuration_cb(1) == nullptr);
}
static void feed(const usb::AudioFormat &format, bool complete = true) {
  audio::buffer::restart();
  for (unsigned number = 0; number < 60; ++number) {
    const unsigned bytes = format.frame_bytes();
    for (unsigned offset = 0, part = 0; offset < bytes; offset += PACKET_WIFI_AUDIO_DATA_MAX_SIZE, ++part) {
      WiFiAudioPacket packet{};
      packet.number = number; packet.part = part;
      packet.size = static_cast<uint16_t>(std::min<unsigned>(PACKET_WIFI_AUDIO_DATA_MAX_SIZE, bytes - offset));
      for (unsigned i = 0; i < packet.size; ++i) packet.data[i] = static_cast<uint8_t>((offset + i) * 37 + 19);
      if (complete || part == 0) audio::buffer::writeWiFiPacket(&packet);
    }
  }
}
int main() {
  audio::buffer::setup();
  // Booting in debug mode must keep the ROM-created Serial/JTAG personality
  // alive so an already-open monitor receives runtime logs without reconnecting.
  config::config.usb.mode = USB_MODE_DEBUG;
  jtag_pad = true;
  const int64_t boot_clock = clock_us;
  usb::setup();
  assert(usb::active_mode() == USB_MODE_DEBUG && jtag_pad && usb_phy == nullptr);
  assert(clock_us == boot_clock);
  usb::off(); pump();
  unsigned supported = 0, rejected = 0;
  std::vector<std::string> identities;
  for (auto rate : {AUDIO_RATE_48000, AUDIO_RATE_96000, AUDIO_RATE_192000})
    for (auto bit : {AUDIO_BIT_16, AUDIO_BIT_24, AUDIO_BIT_32})
      for (auto channels : {AUDIO_CHANNEL_SINGLE, AUDIO_CHANNEL_STEREO}) {
        const usb::AudioFormat format{rate, bit, channels};
        config::config.audio.rate = rate; config::config.audio.bit = bit; config::config.audio.channel = channels;
        usb::off(); pump();
        if (!format.supported()) {
          assert(!ui_setting_usb_page_scb(USB_MODE_AUDIO)); usb::on(USB_MODE_AUDIO); pump();
          assert(usb::active_mode() == USB_MODE_NONE && !attached); ++rejected; continue;
        }
        usb::on(USB_MODE_AUDIO); pump(); enumerate(); check_descriptor(format);
        assert(fifo_words <= 256); ++supported;
        for (const auto &identity : identities) assert(identity != serial());
        identities.push_back(serial());
        auto count = control(0xA1, AUDIO20_CS_REQ_RANGE, AUDIO20_CS_CTRL_SAM_FREQ << 8, UAC2_ENTITY_CLOCK << 8, 2);
        assert(!stalled && count.size() == 2 && tu_unaligned_read16(count.data()) == 1);
        auto range = control(0xA1, AUDIO20_CS_REQ_RANGE, AUDIO20_CS_CTRL_SAM_FREQ << 8, UAC2_ENTITY_CLOCK << 8, 14);
        assert(!stalled && range.size() == 14 && tu_unaligned_read16(range.data()) == 1);
        assert(tu_unaligned_read32(range.data() + 2) == rate && tu_unaligned_read32(range.data() + 6) == rate);
        auto current = control(0xA1, AUDIO20_CS_REQ_CUR, AUDIO20_CS_CTRL_SAM_FREQ << 8, UAC2_ENTITY_CLOCK << 8, 4);
        assert(!stalled && current.size() == 4 && tu_unaligned_read32(current.data()) == rate);
        control(0x21, AUDIO20_CS_REQ_CUR, AUDIO20_CS_CTRL_SAM_FREQ << 8, UAC2_ENTITY_CLOCK << 8, 4, current); assert(!stalled);
        std::vector<uint8_t> wrong{0x44, 0xAC, 0, 0};
        control(0x21, AUDIO20_CS_REQ_CUR, AUDIO20_CS_CTRL_SAM_FREQ << 8, UAC2_ENTITY_CLOCK << 8, 4, wrong); assert(stalled);
        control(1, TUSB_REQ_SET_INTERFACE, 1, ITF_NUM_AUDIO_STREAMING, 0); assert(!stalled && usb::uac::connected());
        feed(format); usb::uac::_loop();
        auto *fifo = tud_audio_n_get_ep_in_ff(0); assert(fifo && tu_fifo_count(fifo) == format.frame_bytes());
        std::vector<uint8_t> samples(format.frame_bytes());
        assert(tu_fifo_peek_n(fifo, samples.data(), samples.size()) == samples.size());
        for (unsigned i = 0; i < samples.size(); ++i) assert(samples[i] == static_cast<uint8_t>(i * 37 + 19));
        // Actual TinyUSB IN transfer sizes must stay within descriptor capacity.
        dcd_event_xfer_complete(0, 0x81, 0, XFER_RESULT_SUCCESS, false); tud_task_ext(0, false);
        assert(transfers[0x81].size > 0 && transfers[0x81].size <= format.packet_bytes());
        assert(transfers[0x81].size % format.sample_bytes() == 0);
        if(format.frame_bytes()>PACKET_WIFI_AUDIO_DATA_MAX_SIZE) {
          feed(format,false); tud_audio_n_clear_ep_in_ff(0); usb::uac::_loop();
          assert(tu_fifo_count(fifo)==format.frame_bytes());
          tu_fifo_peek_n(fifo,samples.data(),samples.size());
          for(auto sample:samples) assert(sample==0);
          feed(format);
        }
        control(0x21, AUDIO20_CS_REQ_CUR, AUDIO20_FU_CTRL_MUTE << 8, UAC2_ENTITY_FEATURE_UNIT << 8, 1, {1}); assert(!stalled);
        tud_audio_n_clear_ep_in_ff(0); usb::uac::_loop();
        tu_fifo_peek_n(fifo, samples.data(), samples.size());
        for (auto sample : samples) assert(sample == 0);
        control(1, TUSB_REQ_SET_INTERFACE, 0, ITF_NUM_AUDIO_STREAMING, 0); assert(!stalled && !usb::uac::connected());
        control(1, TUSB_REQ_SET_INTERFACE, 1, ITF_NUM_AUDIO_STREAMING, 0); assert(usb::uac::connected());
        audio::buffer::restart(); usb::uac::_loop();
        tu_fifo_peek_n(fifo, samples.data(), samples.size()); for (auto sample : samples) assert(sample == 0);
        for (unsigned i = 0; i < 100; ++i) usb::uac::_loop();
        const unsigned full = tu_fifo_count(fifo); usb::uac::_loop(); assert(tu_fifo_count(fifo) == full);
        dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, false); tud_task_ext(0, false);
        usb::uac::_loop(); assert(tu_fifo_count(fifo) == full);
        dcd_event_bus_signal(0, DCD_EVENT_RESUME, false); tud_task_ext(0, false); assert(usb::uac::connected());
        dcd_event_bus_reset(0, TUSB_SPEED_FULL, false); tud_task_ext(0, false); assert(!usb::uac::connected());
      }
  assert(supported == 13 && rejected == 5);
  config::config.audio = {}; // restore default 48 kHz/16 bit/mono
  usb::on(USB_MODE_AUDIO); pump(); enumerate();
  // Old CDC interface requests cannot select a mode or disrupt audio.
  for (unsigned interface_number : {2U, 3U}) {
    control(0x21, 0x22, 3, interface_number, 0); assert(stalled);
    assert(usb::active_mode() == USB_MODE_AUDIO && tud_mounted());
  }
  usb::on(USB_MODE_SD);pump();enumerate();
  std::array<uint8_t,16> command{};command[0]=SCSI_CMD_READ_CAPACITY_10;
  auto capacity=scsi(command,8);
  assert(capacity.status==0 && capacity.data.size()==8 && capacity.residue==0);
  assert(tu_ntohl(tu_unaligned_read32(capacity.data.data()))==card_sectors-1);
  assert(tu_ntohl(tu_unaligned_read32(capacity.data.data()+4))==512);
  // Windows may read a large command ending at the actual last sector, well
  // beyond the FAT data-space count returned by Arduino numSectors().
  const uint32_t first=card_sectors-256;
  constexpr unsigned large_transfer_chunks=(128*1024+CFG_TUD_MSC_EP_BUFSIZE-1)/CFG_TUD_MSC_EP_BUFSIZE;
  auto tail=scsi(rw_command(SCSI_CMD_READ_10,first,256),128*1024);
  assert(tail.status==0 && tail.residue==0 && tail.data.size()==128*1024 && tail.chunks==large_transfer_chunks);
  for(unsigned i=0;i<tail.data.size();i++)assert(tail.data[i]==static_cast<uint8_t>((first+i/512)*7+(i%512)*37+19));
  std::vector<uint8_t> written(128*1024);
  for(unsigned i=0;i<written.size();i++)written[i]=static_cast<uint8_t>(i*13+97);
  auto write=scsi(rw_command(SCSI_CMD_WRITE_10,1000,256),written.size(),false,written);
  assert(write.status==0 && write.residue==0 && write.chunks==large_transfer_chunks);
  auto read=scsi(rw_command(SCSI_CMD_READ_10,1000,256),written.size());
  assert(read.status==0 && read.data==written && read.chunks==large_transfer_chunks);
  command={};command[0]=0x35;assert(scsi(command).status==0); // host flush
  // Partial, cross-sector writes must preserve surrounding data.
  std::vector<uint8_t> partial(700,0xA5),sector_before(1024),sector_after(1024);
  assert(tud_msc_read10_cb(0,333,0,sector_before.data(),sector_before.size())==1024);
  assert(tud_msc_write10_cb(0,333,17,partial.data(),partial.size())==700);
  assert(tud_msc_read10_cb(0,333,0,sector_after.data(),sector_after.size())==1024);
  for(unsigned i=0;i<sector_after.size();i++)assert(sector_after[i]==(i>=17 && i<717?0xA5:sector_before[i]));
  assert(scsi(rw_command(SCSI_CMD_READ_10,card_sectors,1),512).status==MSC_CSW_STATUS_FAILED);
  command={};command[0]=SCSI_CMD_REQUEST_SENSE;command[4]=18;
  assert(scsi(command,18).status==0);
  fail_sector_io=true;assert(scsi(rw_command(SCSI_CMD_READ_10,10,1),512).status==MSC_CSW_STATUS_FAILED);
  fail_sector_io=false;assert(scsi(command,18).status==0);
  assert(scsi(rw_command(SCSI_CMD_READ_10,card_sectors-1,1),512).status==0);
  command={};command[0]=SCSI_CMD_START_STOP_UNIT;command[4]=2;assert(scsi(command).status==0);
  command={};command[0]=SCSI_CMD_TEST_UNIT_READY;assert(scsi(command).status==MSC_CSW_STATUS_FAILED);
  command={};command[0]=SCSI_CMD_REQUEST_SENSE;command[4]=18;assert(scsi(command,18).status==0);
  command={};command[0]=SCSI_CMD_START_STOP_UNIT;command[4]=3;assert(scsi(command).status==0);
  command={};command[0]=SCSI_CMD_TEST_UNIT_READY;assert(scsi(command).status==0);
  for (unsigned repeat = 0; repeat < 25; ++repeat)
    for (auto from : {USB_MODE_NONE, USB_MODE_AUDIO, USB_MODE_SD, USB_MODE_DEBUG})
      for (auto to : {USB_MODE_NONE, USB_MODE_AUDIO, USB_MODE_SD, USB_MODE_DEBUG}) {
        usb::on(from); pump();
        if (from == USB_MODE_AUDIO || from == USB_MODE_SD) enumerate();
        usb::on(to); pump();
        assert(usb::active_mode() == to && storage_owned == (to == USB_MODE_SD));
        assert(jtag_pad == (to == USB_MODE_DEBUG)); assert(attached == (to == USB_MODE_SD || to == USB_MODE_AUDIO));
        assert(tud_inited() == (to == USB_MODE_SD || to == USB_MODE_AUDIO));
        assert(!usb::uac::connected());
      }
  usb::on(USB_MODE_AUDIO); pump(); enumerate(); const std::string old_serial = serial();
  config::config.audio.bit = AUDIO_BIT_24; config::config.audio.channel = AUDIO_CHANNEL_STEREO;
  usb::audio_format_changed(); pump(); enumerate(); assert(serial() != old_serial);
  const auto before = serial(); usb::audio_format_changed(); pump(); assert(serial() == before && tud_mounted());
  during_detach = [] { usb::on(USB_MODE_DEBUG); };
  usb::on(USB_MODE_SD); pump(); assert(usb::active_mode() == USB_MODE_DEBUG && !storage_owned);
  usb::off(); pump(); fail_phy = true; usb::on(USB_MODE_AUDIO); pump();
  assert(usb::active_mode() == USB_MODE_NONE && !controller_live); clock_us += 1000000; pump(); assert(usb::active_mode() == USB_MODE_AUDIO);
  usb::off(); pump(); fail_controller = true; usb::on(USB_MODE_AUDIO); pump();
  assert(usb::active_mode() == USB_MODE_NONE && !controller_live && !attached); clock_us += 1000000; pump(); enumerate();
  usb::on(USB_MODE_SD); pump(); enumerate(); fail_storage_release = true; usb::off(); pump();
  assert(storage_owned && !attached && !controller_live); clock_us += 1000000; pump(); assert(!storage_owned && usb::active_mode() == USB_MODE_NONE);
  puts("PASS: real MSC capacity/tail/128KB read-write/flush/partial writes/eject/error recovery, bounded S3 bulk packets; audio-only enumeration/clock/PCM/mute/stop/reset/suspend, 18 formats, 400 mode transitions, rapid changes, failure recovery");
}
