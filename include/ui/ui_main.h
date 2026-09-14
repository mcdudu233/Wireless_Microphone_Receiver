#pragma once
#include "ui/ui.h"
#include <vector>

#define UPDATE_TIMER_PERIOD 50 // 主界面数据刷新周期；音量包络由RF侧限频至25Hz
#define UPDATE_INFO_PERIOD 500 // 速率、电量、信号无需随音量条高频刷新

typedef struct
{
    lv_obj_t *left_voice_bar;
    lv_obj_t *right_voice_bar;
    lv_obj_t *battery_fill;   // 电池图标内的电量填充条(0-100)
    lv_obj_t *signal_bars[4]; // 信号强度格(从左到右递增,点亮数=信号强弱)
    lv_obj_t *loss_label;     // 丢包率标签(0%为绿色,非0%为红色)
    lv_obj_t *tab;
    std::string device_mac;

} device_card_data;

// api
device_card_data *ui_info_get_obj(const std::string &mac);
void ui_info_set_bar_pct(lv_obj_t *bar, int8_t pct);

uint8_t ui_info_get_loss(const std::string &mac);
int8_t ui_info_get_power(const std::string &mac);
int8_t ui_info_get_signal(const std::string &mac);
int8_t ui_info_get_left_voice(const std::string &mac);
int8_t ui_info_get_right_voice(const std::string &mac);

void ui_info_del_card(device_card_data *card);
void ui_info_del_card(const std::string &mac);

// 断线重连提示(由 rf 模块在其他任务调用,内部自带 LVGL 锁)
bool ui_main_has_device(const std::string &mac);        // 设备是否在主界面卡片中
void ui_main_set_reconnect(const std::string &mac, bool reconnecting); // 显示/隐藏"重连中"状态条
bool ui_main_notify_link_lost(const std::string &mac);  // 宽限期用尽:移除卡片并提示,返回卡片是否被移除

// 定义
std::vector<std::string> ui_bt_get_linked();
