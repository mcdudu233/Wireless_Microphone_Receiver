#include "../../src/ui/ui_main.cpp"
#include <cassert>
#include <fstream>

namespace config { ConfigValue config; }
static unsigned device_count = 4;
static int8_t power_value = 100, signal_value = -35;
static uint8_t loss_value = 0;
static bool settings_opened;
static unsigned number_offset;
std::vector<std::string> ui_bt_get_linked() {
    std::vector<std::string> result;
    for(unsigned i=0;i<device_count;i++) result.push_back(std::to_string(i+1));
    return result;
}
uint8_t ui_info_get_number(const std::string &mac) { return std::stoi(mac)+number_offset; }
int8_t ui_info_get_power(const std::string &) { return power_value; }
int8_t ui_info_get_signal(const std::string &) { return signal_value; }
uint8_t ui_info_get_loss(const std::string &) { return loss_value; }
int8_t ui_info_get_left_voice(const std::string &) { return 72; }
int8_t ui_info_get_right_voice(const std::string &) { return 46; }
void ui_setting_init(lv_obj_t *) { settings_opened = true; }
void ui_bt_init() {}
static void settle() {
    for(int i=0;i<15;i++) { lv_tick_inc(50); lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active());
}
static void inside(lv_obj_t *obj, lv_obj_t *parent) {
    lv_area_t a,b;lv_obj_get_coords(obj,&a);lv_obj_get_coords(parent,&b);
    assert(a.x1>=b.x1 && a.x2<=b.x2 && a.y1>=b.y1 && a.y2<=b.y2);
}
static void label_fits(lv_obj_t *label) {
    lv_point_t size;
    lv_text_get_size(&size,lv_label_get_text(label),UI_FONT_BODY,0,0,1000,LV_TEXT_FLAG_NONE);
    assert(size.x<=lv_obj_get_width(label));
    assert(size.y<=lv_obj_get_height(label));
}
static void capture(const char *name) {
    settle();
    lv_draw_buf_t *buf=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888);
    assert(buf);
    std::ofstream file(std::string(name)+".ppm",std::ios::binary);
    file<<"P6\n160 80\n255\n";
    for(int y=0;y<80;y++) for(int x=0;x<160;x++) {
        const uint8_t *bgr=buf->data+y*buf->header.stride+x*3;
        char rgb[3]={(char)bgr[2],(char)bgr[1],(char)bgr[0]};file.write(rgb,3);
    }
    lv_draw_buf_destroy(buf);
}
int main() {
    lv_init();
    auto display=lv_display_create(160,80);
    static uint8_t pixels[160*80*4];
    lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,[](lv_display_t *d,const lv_area_t *,uint8_t *) {lv_display_flush_ready(d);});
    ui_main_init();settle();
    assert(cards.size()==4);
    assert(lv_group_get_obj_count(main_group)==5);
    auto bar=lv_tabview_get_tab_bar(tabview);
    for(int i=0;i<4;i++) {
        auto button=lv_obj_get_child(bar,i);
        lv_group_focus_obj(button);
        lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);settle();
        assert(lv_tabview_get_tab_active(tabview)==(unsigned)i);
        auto card=lv_obj_get_child(cards[i]->tab,0);
        inside(card,info_widget);
        inside(cards[i]->left_voice_bar,card);inside(cards[i]->right_voice_bar,card);
        inside(cards[i]->battery_label,card);inside(cards[i]->loss_label,card);
        inside(button,info_widget);
        label_fits(cards[i]->battery_label);label_fits(cards[i]->loss_label);
        capture(("focus_"+std::to_string(i)).c_str());
    }
    capture("four_devices");
    const AudioRate rates[]={AUDIO_RATE_48000,AUDIO_RATE_96000,AUDIO_RATE_192000};
    const char *texts[]={"48k","96k","192k"};
    const lv_image_dsc_t *usb_icons[]={&ui_img_usb_none,&ui_img_usb_jtag,&ui_img_usb_audio,&ui_img_usb_sd};
    const lv_image_dsc_t *rf_icons[]={&ui_img_rf_ble,&ui_img_rf_wifi};
    for(int r=0;r<3;r++) for(int u=0;u<4;u++) for(int f=1;f<=2;f++) {
        config::config.audio.rate=rates[r];config::config.usb.mode=(USBMode)u;config::config.rf.mode=(RFMode)f;
        settle();
        assert(std::strcmp(lv_label_get_text(status_rate_badge),texts[r])==0);
        assert(lv_image_get_src(status_img_usb)==usb_icons[u]);
        assert(lv_image_get_src(status_img_rf)==rf_icons[f-1]);
        inside(status_rate_badge,main_widget);inside(status_img_usb,main_widget);inside(status_img_rf,main_widget);
        label_fits(status_rate_badge);
        capture(("mode_"+std::to_string(r)+std::to_string(u)+std::to_string(f)).c_str());
    }
    power_value=0;signal_value=-95;loss_value=100;settle();
    auto data=cards[3];
    assert(std::strcmp(lv_label_get_text(data->loss_label),"丢包100%")==0);
    label_fits(data->loss_label);capture("weak_signal_max_loss");
    signal_value=0;power_value=100;settle();capture("unknown_signal");
    ui_main_set_reconnect("4",true);settle();inside(reconnect_chip,info_widget);
    label_fits(lv_obj_get_child(reconnect_chip,0));capture("reconnect");
    ui_main_set_reconnect("3",true);capture("multiple_reconnect");
    ui_main_set_reconnect("4",false);ui_main_set_reconnect("3",false);
    lv_group_focus_obj(lv_obj_get_child(bar,3));lv_group_focus_next(main_group);
    auto settings=lv_group_get_focused(main_group);
    assert(lv_obj_get_parent(settings)==main_widget);
    capture("settings_focus");lv_obj_send_event(settings,LV_EVENT_CLICKED,nullptr);assert(settings_opened);
    // Removing an earlier tab must retain device 4 and remove its stale button.
    ui_info_del_card("2");settle();
    assert(cards.size()==3 && lv_obj_get_child_count(bar)==3);
    assert(lv_group_get_obj_count(main_group)==4);
    assert(lv_tabview_get_tab_active(tabview)==2);
    assert(cards[2]->device_mac=="4");
    lv_obj_send_event(lv_obj_get_child(bar,2),LV_EVENT_CLICKED,nullptr);settle();
    ui_info_del_card("4");settle();
    assert(lv_tabview_get_tab_active(tabview)==1);
    assert(cards[1]->device_mac=="3");capture("removed_devices");
    ui_free_main_widget();number_offset=251;device_count=4;ui_main_init();settle();
    bar=lv_tabview_get_tab_bar(tabview);
    for(int i=0;i<4;i++) label_fits(lv_obj_get_child(lv_obj_get_child(bar,i),0));
    ui_main_set_reconnect("4",true);settle();
    label_fits(lv_obj_get_child(reconnect_chip,0));capture("max_device_number");
    ui_main_set_reconnect("4",false);
    ui_main_show_link_lost_msgbox("设备255连接失败", "已返回选择设备");
    capture("link_lost_dialog");ui_close_popup();
    assert(lv_group_get_default()==main_group);
    ui_free_main_widget();number_offset=0;device_count=1;ui_main_init();capture("one_device");
    ui_free_main_widget();device_count=0;ui_main_init();
    config::config.audio.rate=AUDIO_RATE_48000;settle();
    assert(std::strcmp(lv_label_get_text(status_rate_badge),"48k")==0);
    assert(lv_group_get_obj_count(main_group)==1);capture("empty");
    ui_free_main_widget();
    puts("PASS: 24 modes, telemetry extremes, 0/1/4 devices, focus, reconnect, repeat entry");
}
