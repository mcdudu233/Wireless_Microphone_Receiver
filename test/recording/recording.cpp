// Exercise production TF storage and audio reassembly, replacing hardware only.
#include "../../src/module/audio/buffer.cpp"
#include "../../src/module/tf.cpp"
#include <fstream>

namespace config { ConfigValue config; }
bool config::snapshot(ConfigValue &, uint32_t &) { return false; }
bool config::import_from_tf(const ConfigValue &, uint32_t) { assert(false);return false; }
size_t logger::take_logs(uint8_t *, size_t, uint32_t &dropped) { dropped=0;return 0; }
static tf::RecordingInfo info() { tf::RecordingInfo i;tf::get_recording_info(i);return i; }
static void frame(uint32_t number, const std::vector<uint8_t> &pcm, bool incomplete=false) {
  for(size_t offset=0;offset<pcm.size();offset+=PACKET_WIFI_AUDIO_DATA_MAX_SIZE) {
    if(incomplete && offset)break;
    WiFiAudioPacket packet={};packet.number=number;packet.part=offset/PACKET_WIFI_AUDIO_DATA_MAX_SIZE;
    packet.size=std::min<size_t>(pcm.size()-offset,PACKET_WIFI_AUDIO_DATA_MAX_SIZE);
    std::memcpy(packet.data,pcm.data()+offset,packet.size);audio::buffer::writeWiFiPacket(&packet);
  }
}
static void begin() { assert(tf::start_recording());service_recording();assert(info().state==tf::RecordingState::RECORDING); }
static void stop() { tf::stop_recording();for(int n=0;n<100 && info().state==tf::RecordingState::STOPPING;n++)service_recording(); }
int main() {
  fake::files["/"]={true,{}};audio::buffer::setup();tf::setup();
  assert(fake::files.count("/recordings") && fake::files.count("/config") && fake::files.count("/logs"));
  assert(!fake::files.count("/WirelessMic"));
  for(auto rate:{AUDIO_RATE_48000,AUDIO_RATE_96000,AUDIO_RATE_192000})
    for(auto bit:{AUDIO_BIT_16,AUDIO_BIT_24,AUDIO_BIT_32}) for(auto channels:{AUDIO_CHANNEL_SINGLE,AUDIO_CHANNEL_STEREO}) {
      config::config.audio.rate=rate;config::config.audio.bit=bit;config::config.audio.channel=channels;
      audio::buffer::restart();begin();assert(!tf::start_recording());assert(!tf::set_usb_storage_active(true));
      assert(!tf::is_deletable_path("/recordings/MIC00001.wav"));
      std::vector<uint8_t> pcm(audio::buffer::getFrameSize());for(size_t n=0;n<pcm.size();n++)pcm[n]=(n*37+11)&255;
      for(uint32_t n=1;n<=9;n++) { frame(n,pcm);fake::clock_ms+=4;service_recording(); }
      stop();assert(info().state==tf::RecordingState::SAVED);
      assert(!usbStarted && !decoderStarted); // Recording never advances other consumers.
      const auto &bytes=fake::files.at(std::string("/recordings/")+info().filename).bytes;
      assert(bytes.size()==recording_header.size+9*pcm.size());
      for(size_t n=recording_header.size;n<bytes.size();n++)assert(bytes[n]==pcm[(n-recording_header.size)%pcm.size()]);
      std::ofstream out(std::to_string(rate)+"_"+std::to_string(bit)+"_"+std::to_string(channels)+".wav",std::ios::binary);
      out.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
    }
  assert(std::string(info().filename)=="MIC00018.wav");
  assert(tf::start_recording());tf::stop_recording();service_recording();assert(info().state==tf::RecordingState::IDLE);
  // Existing media and a reset session counter still produce the next sequence.
  next_recording_number=1;audio::buffer::restart();begin();stop();assert(std::string(info().filename)=="MIC00019.wav");
  assert(tf::set_usb_storage_active(true));assert(tf::start_recording());service_recording();assert(info().error==tf::RecordingError::USB_BUSY);assert(tf::set_usb_storage_active(false));
  assert(tf::set_usb_storage_active(true));
  fake::during_mount=[] { assert(tf::is_usb_storage_active());info();assert(!tf::request_list("/",700)); };
  const auto mounts=fake::mounts;assert(tf::set_usb_storage_active(false));fake::during_mount={};
  assert(fake::mounts==mounts+1 && tf::is_mounted() && !tf::is_usb_storage_active());
  assert(tf::set_usb_storage_active(false));assert(fake::mounts==mounts+1);
  tf::FileEntry entries[4];size_t entry_count=0;bool truncated=false;
  assert(tf::list("/",entries,4,entry_count,truncated) && entry_count==3);
  assert(tf::set_usb_storage_active(true));fake::inserted=false;assert(tf::set_usb_storage_active(false));
  assert(!tf::is_mounted() && !tf::is_usb_storage_active());fake::inserted=true;assert(mount_card());
  request_status=tf::RequestStatus::BUSY;assert(!tf::set_usb_storage_active(true));
  request_status=tf::RequestStatus::LIST_READY;assert(tf::set_usb_storage_active(true));
  result_count=0;bool result_ok=false;uint32_t result_id=0;
  assert(tf::take_list_result(entries,4,entry_count,truncated,result_ok,result_id));
  assert(tf::set_usb_storage_active(false));
  mounted=false;fake::inserted=false;assert(tf::start_recording());service_recording();assert(info().error==tf::RecordingError::NO_CARD);
  fake::inserted=true;begin();stop();assert(info().state==tf::RecordingState::SAVED); // insertion after boot/retry
  // Recording skips pre-start audio, without touching the playback/USB readers.
  audio::buffer::restart();std::vector<uint8_t> pcm(audio::buffer::getFrameSize(),0x55);frame(10,pcm);begin();frame(11,pcm);service_recording();stop();assert(recording_bytes==pcm.size());
  // Missing sequence numbers and incomplete multipart frames keep the timeline.
  audio::buffer::restart();begin();frame(1,pcm);frame(3,pcm,true);frame(4,pcm);frame(5,pcm);stop();
  assert(info().state==tf::RecordingState::SAVED && recording_bytes==5*pcm.size());
  const auto gap=fake::files.at(std::string("/recordings/")+info().filename).bytes;
  for(size_t n=recording_header.size+pcm.size();n<recording_header.size+3*pcm.size();n++)assert(gap[n]==0);
  // Stop boundary does not include subsequently received frames.
  audio::buffer::RecordingReader reader={};audio::buffer::resetRecordingReader(reader);frame(6,pcm);audio::buffer::stopRecordingReader(reader);frame(7,pcm);
  assert(audio::buffer::readRecordingFrame(reader,recording_data,pcm.size(),true)==audio::buffer::RecordingRead::FRAME);
  assert(audio::buffer::readRecordingFrame(reader,recording_data,pcm.size(),true)==audio::buffer::RecordingRead::WAIT);
  audio::buffer::restart();begin();frame(UINT32_MAX-1,pcm);service_recording();frame(UINT32_MAX,pcm);frame(0,pcm);stop();
  assert(info().state==tf::RecordingState::SAVED && recording_bytes==3*pcm.size());
  audio::buffer::restart();begin();config::config.audio.bit=AUDIO_BIT_16;service_recording();assert(info().error==tf::RecordingError::FORMAT_CHANGED);
  audio::buffer::restart();begin();audio::buffer::restart();service_recording();assert(info().error==tf::RecordingError::FORMAT_CHANGED);
  audio::buffer::restart();begin();pcm.resize(audio::buffer::getFrameSize());frame(1,pcm);service_recording();for(uint32_t n=2;n<300;n++)frame(n,pcm);service_recording();assert(info().error==tf::RecordingError::OVERRUN);
  audio::buffer::restart();begin();fake::write_limit=10;frame(1,pcm);service_recording();assert(info().error==tf::RecordingError::IO);fake::write_limit=SIZE_MAX;
  audio::buffer::restart();begin();fake::inserted=false;frame(1,pcm);service_recording();assert(info().error==tf::RecordingError::IO);fake::inserted=true;
  audio::buffer::restart();begin();recording_bytes=UINT32_MAX-257;frame(1,pcm);service_recording();assert(info().state==tf::RecordingState::ERROR); // never wrap RIFF
  assert(mount_card());
  assert(!tf::is_deletable_path("/config/device.ini"));assert(!tf::is_deletable_path("//recordings/a.wav"));
  assert(tf::is_deletable_path("/recordings/MIC00001.wav"));assert(tf::is_deletable_path("/logs/CRASH00001.bin"));
  // No card or corrupt/short saved files must never destroy the flash dump.
  fake::crash.resize(4099);for(size_t n=0;n<fake::crash.size();n++)fake::crash[n]=n%251;
  const auto crash=fake::crash;
  fake::inserted=false;assert(!export_crash());assert(fake::erases==0 && fake::crash==crash);fake::inserted=true;
  fake::write_limit=5;assert(!export_crash());assert(fake::erases==0 && fake::crash==crash);fake::write_limit=SIZE_MAX;
  fake::corrupt_reads=true;assert(!export_crash());assert(fake::erases==0 && fake::crash==crash);fake::corrupt_reads=false;
  fake::crash_valid=false;assert(!export_crash());assert(fake::erases==0);fake::crash_valid=true;
  assert(export_crash());assert(fake::erases==1 && fake::crash.empty());assert(fake::files.at("/logs/CRASH00001.bin").bytes==crash);
  const auto text=fake::files.at("/logs/CRASH00001.txt").bytes;assert(std::string(text.begin(),text.end()).find("test panic")!=std::string::npos);
  const std::string summary(text.begin(),text.end());
  assert(summary.find("0x40381234")!=std::string::npos && summary.find("0x40385678")!=std::string::npos);
  assert(summary.find("0xdeadbeef")!=std::string::npos && summary.find("Exception cause: 28")!=std::string::npos);
  assert(export_crash());assert(fake::erases==1);
  puts("PASS: 18 PCM formats, exact data, independent reader, sequence/gap/stop boundaries, storage exclusion, insertion, format/reset/overrun/write/removal/RIFF errors, readback-verified crash export");
}
