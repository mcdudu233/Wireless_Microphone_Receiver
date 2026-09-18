#include "../../src/config.cpp"
#include "../../src/logger.cpp"
#include "../../src/module/audio/buffer.cpp"
#include "../../src/module/tf.cpp"
#include <cassert>

namespace fake {
inline unsigned lv_depth=0,apply_count=0,screen_count=0,usb_count=0,refresh_count=0;
inline bool rf_busy=false,apply_fail=false;
}
namespace screen {
void lv_lock_wait() { assert(!fake::lv_depth && (!card_mutex || !*card_mutex));++fake::lv_depth; }
void lv_unlock() { assert(fake::lv_depth==1);--fake::lv_depth; }
void apply_settings() { assert(fake::lv_depth);++fake::screen_count; }
}
namespace rf {
bool settings_busy() { return fake::rf_busy; }
bool apply_settings(const config::ConfigValue &) { assert(fake::lv_depth && !*saveMutex);++fake::apply_count;return !fake::apply_fail; }
}
namespace usb { void on() { assert(fake::lv_depth);++fake::usb_count; } }
void ui_setting_refresh_config() { assert(fake::lv_depth);++fake::refresh_count; }

static std::string contents(const char *path) {
 if(!fake::files.count(path))std::fprintf(stderr,"Missing expected file %s\n",path);
 assert(fake::files.count(path));
 const auto &v=fake::files.at(path).bytes;return {v.begin(),v.end()};
}
static void put(const char *path,const std::string &s) { fake::files[path]={false,{s.begin(),s.end()}}; }
static config::ConfigValue disk() {
 auto text=contents("/config/device.ini");config::ConfigValue v;uint32_t baseline;
 assert(config::file::decode(text.data(),v,baseline));assert(baseline==config::file::fingerprint(v));return v;
}
static void replace(std::string &s,const char *old,const char *value) {
 auto pos=s.find(old);assert(pos!=std::string::npos);s.replace(pos,std::strlen(old),value);
}
static void check_reject(const std::string &text) {
 const uint32_t before=config::file::fingerprint(config::config);put("/config/device.ini",text);
 config_needs_check=true;service_config();assert(contents("/config/device.ini")==text);
 assert(config::file::fingerprint(config::config)==before);
}
int main() {
 logger::setup();logger::setup();config::setup();fake::files["/"]={true,{}};audio::buffer::setup();tf::setup();
 assert(disk().audio.rate==AUDIO_RATE_48000 && disk().screen.brightness==50);
 for(auto rate:{AUDIO_RATE_48000,AUDIO_RATE_96000,AUDIO_RATE_192000})
   for(auto bit:{AUDIO_BIT_16,AUDIO_BIT_24,AUDIO_BIT_32})for(auto channel:{AUDIO_CHANNEL_SINGLE,AUDIO_CHANNEL_STEREO}) {
     config::ConfigValue value;value.usb.mode=USB_MODE_NONE;value.audio.rate=rate;value.audio.bit=bit;value.audio.channel=channel;
     char text[config::file::CAPACITY];assert(config::file::encode(value,text,sizeof(text)));
     config::ConfigValue decoded;uint32_t baseline;
     assert(config::file::decode(text,decoded,baseline) && config::file::fingerprint(decoded)==baseline);
   }
 // UI -> actual Preferences -> TF, and failed NVS writes never reach the card.
 config::config.screen.brightness=76;config::save();service_config();assert(disk().screen.brightness==76);
 fake::during_nvs_write=[] { config::config.screen.brightness=66; };
 config::save();fake::during_nvs_write={};service_config();assert(disk().screen.brightness==76);
 config::ConfigValue nvs_value;assert(prefs.getBytes(CONFIG_DATA_NAME,&nvs_value,sizeof(nvs_value))==sizeof(nvs_value));
 assert(nvs_value.screen.brightness==76);config::config.screen.brightness=76;
 const auto original=contents("/config/device.ini");
 fake::nvs_write_fail=true;config::config.screen.brightness=77;config::save();service_config();
 assert(contents("/config/device.ini")==original);fake::nvs_write_fail=false;config::config.screen.brightness=76;
 // USB host edit: no firmware file access while owned; import after remount.
 assert(tf::set_usb_storage_active(true));auto edit=original;replace(edit,"brightness=76","brightness=39");put("/config/device.ini",edit);
 service_config();assert(config::config.screen.brightness==76 && contents("/config/device.ini")==edit);
 assert(tf::set_usb_storage_active(false));service_config();assert(config::config.screen.brightness==39 && disk().screen.brightness==39);
 assert(fake::apply_count==1 && fake::screen_count==1 && fake::usb_count==1 && fake::refresh_count==1);
 // Reboot reconstructs baseline without any clock; unchanged TF cannot undo
 // Preferences changes saved with no card, even on another session.
 config::config.screen.brightness=82;config::save();config::setup();config_needs_check=true;service_config();assert(disk().screen.brightness==82);
 // Detect card removal, then editing and reinsertion without a restart.
 fake::inserted=false;service_media();assert(!tf::is_mounted());
 edit=contents("/config/device.ini");replace(edit,"brightness=82","brightness=24");put("/config/device.ini",edit);
 fake::inserted=true;service_media();service_config();assert(tf::is_mounted() && config::config.screen.brightness==24);
 const auto valid=contents("/config/device.ini");
 for(const char *bad:{"brightness=101","brightness=-1","brightness=0","brightness=9","brightness=256","brightness=garbage"}) {
   edit=valid;replace(edit,"brightness=24",bad);check_reject(edit);
 }
 edit=valid;replace(edit,"rate=48000","rate=44100");check_reject(edit);
 edit=valid;replace(edit,"[rf]\nmode=2","[rf]\nmode=1");replace(edit,"bit=16","bit=24");check_reject(edit);
 edit=valid;replace(edit,"rate=48000","rate=192000");replace(edit,"channel=1","channel=2");replace(edit,"bit=16","bit=32");check_reject(edit);
 edit=valid+"brightness=24\n";check_reject(edit);check_reject(valid.substr(0,200));check_reject(std::string(3000,'x'));
 check_reject(valid+std::string("\0bad",4));
 // BOM, CRLF, comments, and signed gain parse as a complete valid transaction.
 edit="\xef\xbb\xbf"+valid;replace(edit,"gain=0","gain=-64 ; dB");
 for(size_t p=0;(p=edit.find('\n',p))!=std::string::npos;p+=2)edit.insert(p,"\r");
 put("/config/device.ini",edit);config_needs_check=true;service_config();assert(config::config.audio.gain==-64 && disk().audio.gain==-64);
 // A busy switch or failed Preferences import retains the edit for retry.
 edit=contents("/config/device.ini");replace(edit,"brightness=24","brightness=17");put("/config/device.ini",edit);
 config_needs_check=true;fake::rf_busy=true;service_config();assert(config::config.screen.brightness==24);
 fake::rf_busy=false;fake::nvs_write_fail=true;service_config();assert(config::config.screen.brightness==24);
 fake::nvs_write_fail=false;service_config();assert(disk().screen.brightness==17);
 edit=contents("/config/device.ini");replace(edit,"brightness=17","brightness=16");put("/config/device.ini",edit);config_needs_check=true;
 fake::apply_fail=true;service_config();assert(config::config.screen.brightness==17);
 config::ConfigValue rolled_back;uint32_t rollback_generation;assert(config::snapshot(rolled_back,rollback_generation));
 assert(rolled_back.screen.brightness==17);fake::apply_fail=false;service_config();assert(disk().screen.brightness==16);
 config::ConfigValue stale;uint32_t revision;assert(config::snapshot(stale,revision));config::save();assert(!config::import_from_tf(stale,revision));
 // Readback failures cannot replace a verified file. Recover backup on boot.
 config::config.screen.brightness=18;config::save();const auto previous=contents("/config/device.ini");
 fake::write_limit=10;service_config();assert(contents("/config/device.ini")==previous);fake::write_limit=SIZE_MAX;
 fake::corrupt_reads=true;assert(!write_config(config::config));assert(contents("/config/device.ini")==previous);fake::corrupt_reads=false;
 service_config();assert(disk().screen.brightness==18);
 assert(SD_MMC.rename("/config/device.ini","/config/device.bak"));config_needs_check=true;service_config();assert(disk().screen.brightness==18);
 put("/config/device.ini","; Wireless Microphone storage settings\n; UTF-8 text file, reserved for future runtime configuration\n[recording]\nformat=wav\nfilename=MIC00001.wav\n[storage]\nrecordings_dir=/recordings\nlogs_dir=/logs\n");
 config_needs_check=true;service_config();assert(disk().screen.brightness==18);
 edit=contents("/config/device.ini");replace(edit,"[usb]\nmode=2","[usb]\nmode=3");put("/config/device.ini",edit);
 config_needs_check=true;service_config();assert(config::config.usb.mode==USB_MODE_SD);
 assert(tf::set_usb_storage_active(true)); // Imported reader mode is not stuck behind its old generation.
 config::config.usb.mode=USB_MODE_DEBUG;config::save();service_config();assert(contents("/config/device.ini").find("[usb]\nmode=3")!=std::string::npos);
 assert(tf::set_usb_storage_active(false));service_config();assert(disk().usb.mode==USB_MODE_DEBUG);
 // Capture exact console output, bounded overflow, serial forwarding, and
 // file sessions/rotation with strictly increasing sequence numbers.
 fake::emit("I (12) rf: value=%d\n",42);assert(fake::serial=="I (12) rf: value=42\n");fake::clock_ms+=250;service_logs();
 assert(contents("/logs/LOG00001.txt")==fake::serial);
 assert(tf::set_usb_storage_active(true));fake::emit("W (21) TF owned\n");fake::clock_ms+=250;service_logs();assert(!fake::files.count("/logs/LOG00002.txt"));
 assert(tf::set_usb_storage_active(false));fake::clock_ms+=250;service_logs();assert(contents("/logs/LOG00002.txt")=="W (21) TF owned\n");
 log_bytes=1024*1024;fake::emit("rotation\n");fake::clock_ms+=250;service_logs();assert(contents("/logs/LOG00003.txt")=="rotation\n");
 close_log();next_log_number=1;fake::emit("restart\n");fake::clock_ms+=250;service_logs();assert(contents("/logs/LOG00004.txt")=="restart\n");
 fake::emit("partial\n");fake::write_limit=3;fake::clock_ms+=250;service_logs();assert(!tf::is_mounted() && log_pending_size==5);
 fake::write_limit=SIZE_MAX;service_media();fake::clock_ms+=250;service_logs();assert(contents("/logs/LOG00005.txt")=="tial\n");
 for(int n=0;n<1000;++n)fake::emit("record %d: buffered while card unavailable\n",n);
 assert(log_buffer.dropped>0);fake::clock_ms+=250;service_logs();assert(contents("/logs/LOG00005.txt").find("records dropped")!=std::string::npos);
 logger::Buffer ring;std::string large(logger::Buffer::CAPACITY-2,'a');ring.append(large.data(),large.size());ring.append("xxx",3);
 uint8_t bytes[1024];uint32_t dropped;assert(ring.take(bytes,sizeof(bytes),dropped)==1024 && dropped==1);
 while(ring.count)ring.take(bytes,sizeof(bytes),dropped);
 ring.append("wrap",4);assert(ring.take(bytes,sizeof(bytes),dropped)==4 && !std::memcmp(bytes,"wrap",4));
 fake::emit("%s",std::string(900,'z').c_str());assert(fake::serial.size()>900);
 puts("PASS: Preferences/TF bidirectional sync, boot/USB/removal recovery, validation, NVS/readback failures, runtime hooks, captured serial/log sequence/rotation/overflow");
}
