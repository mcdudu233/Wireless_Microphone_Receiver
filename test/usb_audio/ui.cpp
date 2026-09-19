#include "../../src/ui/ui_setting.cpp"
#include "ui/ui_bt.h"
#include "ui/ui_loading.h"
#include <cassert>
#include <fstream>
#include <vector>
#include "../main_dashboard/glyphs.h"

namespace config { ConfigValue config; }
static bool card_present = true;
static bool card_owned;
static bool recording_busy;
static unsigned list_requests;
static bool list_ready;
static uint32_t list_id;
static bool full_listing;
static std::string requested_path;
static bool settings_notice;
static std::vector<std::string> linked;
void ui_setting_audio_input_page_rcb(AudioBit &bit, AudioChannel &channels, AudioRate &rate, AudioGain &gain, AudioMode &mode) {
  const auto &a = config::config.audio; bit=a.bit; channels=a.channel; rate=a.rate; gain=a.gain; mode=a.mode;
}
bool ui_setting_audio_input_page_scb(AudioBit bit, AudioChannel channels, AudioRate rate, AudioGain gain, AudioMode mode) {
  if(config::config.usb.mode==USB_MODE_AUDIO && !usb::AudioFormat{rate,bit,channels}.supported()) return false;
  auto &a=config::config.audio; a.bit=bit; a.channel=channels; a.rate=rate; a.gain=gain; a.mode=mode; return true;
}
void ui_setting_audio_output_page_rcb(bool &enabled, AudioOutputMode &mode) { enabled=config::config.audio_output.enabled; mode=config::config.audio_output.mode; }
void ui_setting_audio_output_page_scb(bool, AudioOutputMode) {}
void ui_setting_screen_page_rcb(uint8_t &brightness, ScreenTimeout &timeout) { brightness=config::config.screen.brightness; timeout=config::config.screen.timeout; }
void ui_setting_screen_page_scb(uint8_t, ScreenTimeout) {}
void ui_setting_rf_page_rcb(RFMode &mode) { mode=config::config.rf.mode; }
void ui_setting_rf_page_scb(RFMode mode) { config::config.rf.mode=mode; }
void ui_setting_usb_page_rcb(USBMode &mode) { mode=config::config.usb.mode; }
bool ui_setting_usb_page_scb(USBMode mode) {
  const auto &a=config::config.audio;
  if(mode==USB_MODE_AUDIO && !usb::AudioFormat{a.rate,a.bit,a.channel}.supported()) return false;
  if(mode==USB_MODE_SD && (!card_present || recording_busy)) return false;
  config::config.usb.mode=mode;card_owned=mode==USB_MODE_SD;return true;
}
void ui_setting_system_page_rcb(float &a,float &b,size_t &c,size_t &d,size_t &e,size_t &f) { a=b=100; c=e=320*1024; d=f=8*1024*1024; }
namespace rf { AudioGain getConnectedDeviceGain() { return 63; } }
namespace tf {
  void get_recording_info(RecordingInfo &info) { info={};if(recording_busy)info.state=RecordingState::RECORDING; }
  bool start_recording() { return false; }
  void stop_recording() {}
  bool is_mounted() { return card_present; }
  void get_info(StorageInfo &info) { info={card_present,card_owned,CardType::SDHC,128ULL*1024*1024*1024,0,0,0,512}; }
  bool request_list(const char *path,uint32_t id) {
    assert(std::strcmp(path,"/")==0 || std::strcmp(path,"/recordings")==0);
    assert(!card_owned);++list_requests;list_id=id;requested_path=path;return true;
  }
  bool take_list_result(FileEntry *entries,size_t capacity,size_t &count,bool &truncated,bool &success,uint32_t &id) {
    if(!list_ready) return false;
    list_ready=false;count=0;truncated=false;success=true;id=list_id;
    if(full_listing) {
      if(requested_path=="/") {
        assert(capacity>=4);
        for(const char *name:{"config","errors","logs","recordings"}) {
          entries[count]={};std::strcpy(entries[count].name,name);
          entries[count].directory=true;entries[count].name_complete=true;++count;
        }
      } else {
        assert(capacity==TF_FILE_LIST_MAX);
        for(size_t n=0;n<capacity;++n) {
          entries[n]={};std::snprintf(entries[n].name,sizeof(entries[n].name),"MIC%05u.wav",static_cast<unsigned>(n+1));
          entries[n].size=UINT64_MAX;entries[n].name_complete=true;
        }
        count=capacity;truncated=true;
      }
    }
    return true;
  }
  bool request_remove_file(const char *,uint32_t) { return false; }
  bool take_remove_result(bool &,uint32_t &) { return false; }
  bool is_deletable_path(const char *path) {
    assert(!std::strstr(path,"//"));return std::strncmp(path,"/recordings/",12)==0;
  }
}
std::vector<std::string> ui_bt_get_linked() { return linked; }
uint8_t ui_info_get_number(const std::string &) { return 255; }
int8_t ui_info_get_power(const std::string &) { return 100; }
int8_t ui_info_get_signal(const std::string &) { return -35; }
uint8_t ui_info_get_loss(const std::string &) { return 0; }
int8_t ui_info_get_left_voice(const std::string &) { return 100; }
int8_t ui_info_get_right_voice(const std::string &) { return 100; }
bool ui_bt_unlink(const std::string &) { return true; }
bool ui_bt_can_link(const std::string &) { return true; }
bool ui_bt_link(const std::string &) { return true; }
void ui_bt_search() {}
void ui_bt_pause_search() {}
void ui_bt_seed_devices() {}
bool ui_bt_wifi_ready() { return true; }
uint8_t ui_bt_linked_count() { return linked.size(); }
void ui_bt_reset_autoconnect() {}
void ui_bt_finish() {}
static void settle(unsigned ticks=750) {
  for(unsigned i=0;i<ticks;i+=50) { lv_tick_inc(50); lv_timer_handler(); }
  lv_obj_update_layout(lv_screen_active());
}
static lv_obj_t *find_label(lv_obj_t *parent,const char *text) {
  if(lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return nullptr;
  if(lv_obj_check_type(parent,&lv_label_class) && std::strcmp(lv_label_get_text(parent),text)==0) return parent;
  for(unsigned i=0;i<lv_obj_get_child_count(parent);++i)
    if(auto *obj=find_label(lv_obj_get_child(parent,i),text)) return obj;
  return nullptr;
}
static lv_obj_t *top_visible() {
  for(int i=static_cast<int>(lv_obj_get_child_count(lv_screen_active()))-1;i>=0;--i) {
    auto *obj=lv_obj_get_child(lv_screen_active(),i);
    if(!lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return obj;
  }
  return nullptr;
}
static void capture(const std::string &name) {
  settle();assert_visible_glyphs(lv_screen_active());
  auto *buf=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888); assert(buf);
  std::ofstream file(name+".ppm",std::ios::binary); file<<"P6\n160 80\n255\n";
  for(int y=0;y<80;++y) for(int x=0;x<160;++x) {
    auto *b=buf->data+y*buf->header.stride+x*3;
    const char rgb[]={static_cast<char>(b[2]),static_cast<char>(b[1]),static_cast<char>(b[0])}; file.write(rgb,3);
  }
  lv_draw_buf_destroy(buf);
}
static void enter(const char *label) {
  if(lv_menu_get_cur_main_page(menu)!=root_page_ref) lv_obj_send_event(main_back_btn,LV_EVENT_CLICKED,nullptr);
  settle();
  assert(top_visible()==setting_widget);
  for(unsigned i=0;i<lv_obj_get_child_count(root_page_ref);++i) {
    auto *row=lv_obj_get_child(root_page_ref,i);
    for(unsigned j=0;j<lv_obj_get_child_count(row);++j) {
      auto *obj=lv_obj_get_child(row,j);
      if(lv_obj_check_type(obj,&lv_label_class) && std::strcmp(lv_label_get_text(obj),label)==0) {
        lv_group_focus_obj(row); lv_obj_send_event(row,LV_EVENT_CLICKED,nullptr); settle(); return;
      }
    }
  }
  assert(false);
}
static void change(lv_obj_t *dropdown,unsigned value) {
  lv_group_focus_obj(dropdown); lv_dropdown_set_selected(dropdown,value);
  lv_obj_send_event(dropdown,LV_EVENT_VALUE_CHANGED,nullptr); settle();
}
static void label_fits(lv_obj_t *obj) {
  lv_point_t size;
  lv_text_get_size(&size,lv_label_get_text(obj),lv_obj_get_style_text_font(obj,LV_PART_MAIN),0,0,1000,LV_TEXT_FLAG_NONE);
  if(size.x>lv_obj_get_width(obj) || size.y>lv_obj_get_height(obj))
    std::fprintf(stderr,"Label does not fit: %s text=%ldx%ld box=%ldx%ld\n",lv_label_get_text(obj),long(size.x),long(size.y),long(lv_obj_get_width(obj)),long(lv_obj_get_height(obj)));
  assert(size.x<=lv_obj_get_width(obj) && size.y<=lv_obj_get_height(obj));
}
static void inside(lv_obj_t *obj,lv_obj_t *parent,int margin=0) {
  lv_area_t a,b; lv_obj_get_coords(obj,&a); lv_obj_get_coords(parent,&b);
  if(!(a.x1-margin>=b.x1 && a.y1-margin>=b.y1 && a.x2+margin<=b.x2 && a.y2+margin<=b.y2))
    fprintf(stderr,"Clipped object=(%ld,%ld,%ld,%ld) parent=(%ld,%ld,%ld,%ld) margin=%d\n",
      static_cast<long>(a.x1),static_cast<long>(a.y1),static_cast<long>(a.x2),static_cast<long>(a.y2),
      static_cast<long>(b.x1),static_cast<long>(b.y1),static_cast<long>(b.x2),static_cast<long>(b.y2),margin);
  assert(a.x1-margin>=b.x1 && a.y1-margin>=b.y1 && a.x2+margin<=b.x2 && a.y2+margin<=b.y2);
}
static void usb_details_fit() {
  auto *page=lv_menu_get_cur_main_page(menu);
  for(auto *label:{usb_detail_primary,usb_detail_secondary}) {
    label_fits(label); inside(label,lv_obj_get_parent(label)); inside(label,page); inside(label,setting_widget);
  }
  inside(dd_usb_mode,page,3); inside(dd_usb_mode,setting_widget,3);
}
static void check_notice(lv_obj_t *restore,const char *text="USB带宽不足\n请降低音频格式") {
  auto *popup=top_visible(); auto *win=lv_obj_get_child(popup,0);
  bool body=false;
  for(unsigned i=0;i<lv_obj_get_child_count(win);++i) {
    auto *obj=lv_obj_get_child(win,i);
    if(lv_obj_check_type(obj,&lv_label_class) && std::strcmp(lv_label_get_text(obj),text)==0) {
      lv_point_t size;
      lv_text_get_size(&size,lv_label_get_text(obj),UI_FONT_BODY,0,0,lv_obj_get_width(obj),LV_TEXT_FLAG_NONE);
      assert(size.y<=lv_obj_get_height(obj)); body=true;
    }
  }
  assert(body); capture(std::strcmp(text,"USB带宽不足\n请降低音频格式")==0 ? "usb_bandwidth_dialog" :
                        std::strcmp(text,"正在录音\n请先停止录音")==0 ? "reader_recording_dialog" : "usb_no_card");
  lv_obj_send_event(popup,LV_EVENT_CLICKED,nullptr); settle();
  assert(lv_group_get_focused(setting_group)==restore); settings_notice=true;
}
int main() {
  config::config.screen.brightness=100;
  lv_init(); auto *display=lv_display_create(160,80); static uint8_t pixels[160*80*4];
  lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display,[](lv_display_t *d,const lv_area_t *,uint8_t *) {lv_display_flush_ready(d);});
  ui_loading_init(); ui_loading_set_part("音频解码器"); ui_loading_set_percent(50); capture("loading");
  ui_loading_set_percent(100); settle(30000);
  assert(find_label(lv_screen_active(),"麦克风设备连接")); capture("bluetooth_empty");
  for(unsigned i=0;i<4;++i) ui_bt_update(std::to_string(i),i==0?BT_LINK_STATE_LINKED:BT_LINK_STATE_UNLINKED);
  capture("bluetooth_max_devices");
  auto *finish=find_label(lv_screen_active(),"完成"); assert(finish);
  lv_obj_send_event(lv_obj_get_parent(finish),LV_EVENT_CLICKED,nullptr); capture("main_empty");
  auto *settings=find_label(lv_screen_active(),"设置"); assert(settings);
  lv_obj_send_event(lv_obj_get_parent(settings),LV_EVENT_CLICKED,nullptr); settle();
  capture("settings_root");
  assert(top_visible()==setting_widget);
  enter("USB传输设置");
  assert(std::strcmp(lv_dropdown_get_options(dd_usb_mode),"关闭\n音频\n读卡器\n调试模式")==0);
  for(auto mode:{0U,1U,2U,3U}) { change(dd_usb_mode,mode); usb_details_fit(); capture("usb_mode_"+std::to_string(mode)); }
  assert(config::config.usb.mode==USB_MODE_DEBUG);
  assert(std::strcmp(lv_label_get_text(usb_detail_primary),"程序下载与调试")==0);
  assert(std::strcmp(lv_label_get_text(usb_detail_secondary),"USB串口输出日志")==0);
  lv_dropdown_open(dd_usb_mode); capture("usb_modes_popup"); lv_dropdown_close(dd_usb_mode);
  card_present=false; change(dd_usb_mode,2); capture("usb_no_card");
  assert(lv_dropdown_get_selected(dd_usb_mode)==3);check_notice(dd_usb_mode,"未检测到TF卡\n请插入TF卡");card_present=true;
  config::config.audio.rate=AUDIO_RATE_192000; config::config.audio.bit=AUDIO_BIT_32; config::config.audio.channel=AUDIO_CHANNEL_STEREO;
  change(dd_usb_mode,1); assert(lv_dropdown_get_selected(dd_usb_mode)==3); check_notice(dd_usb_mode);
  config::config.audio.rate=AUDIO_RATE_96000; config::config.audio.bit=AUDIO_BIT_24;
  change(dd_usb_mode,1); assert(config::config.usb.mode==USB_MODE_AUDIO); usb_details_fit(); capture("usb_96k_24bit_stereo");
  enter("音频输入设置"); change(dd_audio_bit,2); assert(lv_dropdown_get_selected(dd_audio_bit)==1); check_notice(dd_audio_bit);
  change(dd_audio_rate,0); change(dd_audio_bit,2); assert(config::config.audio.bit==AUDIO_BIT_32); capture("audio_48k_32bit");
  lv_dropdown_open(dd_audio_channel); capture("channel_popup"); lv_dropdown_close(dd_audio_channel);
  change(dd_audio_channel,0); change(dd_audio_rate,2); assert(lv_dropdown_get_selected(dd_audio_rate)==0); check_notice(dd_audio_rate);
  change(dd_audio_bit,1); change(dd_audio_rate,2); assert(config::config.audio.rate==AUDIO_RATE_192000); capture("audio_192k_24bit_mono");
  enter("USB传输设置"); usb_details_fit(); capture("usb_192k_24bit_mono");
  unsigned index=0;
  for(const char *page:{"音频输出设置","屏幕设置","系统信息","关于","文件管理","无线传输设置"}) { enter(page); capture("subpage_"+std::to_string(index++)); }
  enter("USB传输设置");recording_busy=true;
  const auto previous_mode=config::config.usb.mode;
  change(dd_usb_mode,2);assert(config::config.usb.mode==previous_mode);
  check_notice(dd_usb_mode,"正在录音\n请先停止录音");recording_busy=false;
  card_present=false;change(dd_usb_mode,2);assert(config::config.usb.mode==previous_mode);
  check_notice(dd_usb_mode,"未检测到TF卡\n请插入TF卡");card_present=true;
  change(dd_usb_mode,2);usb_details_fit();capture("reader_exclusive_details");
  const auto requests_before=list_requests;
  enter("文件管理");
  assert(list_requests==requests_before && lv_group_get_focused(setting_group)==main_back_btn);
  for(const char *text:{"TF卡被USB占用","请退出读卡器模式"}) {
    auto *label=find_label(setting_widget,text);assert(label);label_fits(label);inside(label,lv_menu_get_cur_main_page(menu));
  }
  capture("files_usb_exclusive");
  // Actual owner release/remount, independent of selected-mode UI timing.
  card_owned=false;settle();assert(list_requests==requests_before+1);
  list_ready=true;settle();assert(find_label(setting_widget,"目录为空"));capture("files_restored");
  // A stale list completion must not replace the USB ownership notice.
  request_file_page();card_owned=true;settle();list_ready=true;settle();
  assert(find_label(setting_widget,"TF卡被USB占用"));capture("files_stale_result_blocked");
  card_owned=false;card_present=false;settle();assert(find_label(setting_widget,"TF卡未挂载"));capture("files_restore_no_card");
  card_present=true;settle();list_ready=true;settle();assert(find_label(setting_widget,"目录为空"));
  full_listing=true;request_file_page();list_ready=true;settle();
  assert(lv_obj_get_child_count(file_list)==4 && requested_path=="/");
  auto *recordings=find_label(setting_widget,"recordings/");assert(recordings);
  auto *recordings_row=lv_obj_get_parent(recordings);lv_group_focus_obj(recordings_row);settle();inside(recordings_row,file_list);
  lv_obj_send_event(recordings_row,LV_EVENT_CLICKED,nullptr);settle();
  assert(requested_path=="/recordings" && std::strcmp(current_file_path,"/recordings")==0);
  list_ready=true;settle();assert(lv_obj_get_child_count(file_list)==26);
  for(unsigned n:{1U,12U,24U}) {
    auto *row=lv_obj_get_child(file_list,n);lv_group_focus_obj(row);settle();
    assert(lv_group_get_focused(setting_group)==row);inside(row,file_list);
    auto *label=lv_obj_get_child(row,1);assert(lv_obj_get_width(label)==115);
    assert(lv_obj_get_height(label)==lv_font_get_line_height(UI_FONT_BODY));
    assert(lv_label_get_long_mode(label)==LV_LABEL_LONG_DOT);
    capture("files_max_focus_"+std::to_string(n));
  }
  auto *parent_row=lv_obj_get_child(file_list,0);lv_group_focus_obj(parent_row);settle();inside(parent_row,file_list);
  lv_obj_send_event(parent_row,LV_EVENT_CLICKED,nullptr);settle();
  assert(requested_path=="/" && std::strcmp(current_file_path,"/")==0);
  list_ready=true;settle();capture("files_root_directories");
  // TF imports refresh the exact visible controls without replaying navigation,
  // retaining focus or advancing it when new settings disable that control.
  enter("USB传输设置");auto *usb_focus=lv_group_get_focused(setting_group);
  config::config.usb.mode=USB_MODE_DEBUG;ui_setting_refresh_config();settle();
  assert(lv_dropdown_get_selected(dd_usb_mode)==3 && lv_group_get_focused(setting_group)==usb_focus);
  usb_details_fit();capture("tf_import_usb_refresh");
  enter("屏幕设置");lv_group_focus_obj(dd_screen_timeout);settle();
  config::config.screen.brightness=88;config::config.screen.timeout=SCREEN_TIMEOUT_30_SECONDS;
  ui_setting_refresh_config();settle();
  assert(lv_slider_get_value(slider_screen_brightness)==88 && lv_dropdown_get_selected(dd_screen_timeout)==1);
  assert(std::strcmp(lv_label_get_text(label_screen_brightness),"88%") == 0);
  assert(lv_group_get_focused(setting_group)==dd_screen_timeout);capture("tf_import_screen_refresh");
  config::config.audio.mode=AUDIO_MODE_MANUAL;config::config.audio.rate=AUDIO_RATE_48000;
  config::config.audio.bit=AUDIO_BIT_16;config::config.audio.channel=AUDIO_CHANNEL_SINGLE;
  config::config.audio.gain=-64;
  enter("音频输入设置");lv_group_focus_obj(slider_audio_gain);settle();
  assert(lv_slider_get_value(slider_audio_gain)==-64 && std::strcmp(lv_label_get_text(label_audio_gain),"-64dB")==0);
  label_fits(label_audio_gain);inside(label_audio_gain,lv_obj_get_parent(label_audio_gain));capture("tf_import_negative_gain");
  auto *import_popup=ui_popwin_msgbox("正在录音\n请先停止录音",nullptr,slider_audio_gain);
  config::config.rf.mode=RF_MODE_BLE;config::config.audio.mode=AUDIO_MODE_AUTO;
  ui_setting_refresh_config();settle();
  lv_obj_send_event(import_popup,LV_EVENT_CLICKED,nullptr);settle();
  assert(lv_obj_has_state(slider_audio_gain,LV_STATE_DISABLED));
  assert(lv_group_get_focused(setting_group)!=slider_audio_gain &&
         !lv_obj_has_state(lv_group_get_focused(setting_group),LV_STATE_DISABLED));
  capture("tf_import_ble_refresh");
  enter("音频输出设置");auto *output_focus=lv_group_get_focused(setting_group);
  config::config.audio_output.enabled=false;config::config.audio_output.mode=AUDIO_OUTPUT_ALWAYS_ON;
  ui_setting_refresh_config();settle();
  assert(lv_dropdown_get_selected(dd_audio_output_enabled)==0 && lv_dropdown_get_selected(dd_audio_output_mode)==1);
  assert(lv_group_get_focused(setting_group)==output_focus);capture("tf_import_output_refresh");
  assert(settings_notice); puts("PASS: actual LVGL USB/input controls, unsupported rollback, two-line dialog and restored focus, format details, loading/Bluetooth/main/settings/subpages");
}
