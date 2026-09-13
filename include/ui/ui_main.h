#pragma once
#include "ui/ui.h"
#include <vector>

#define UPDATE_TIMER_PERIOD 500 // 主界面数据(实时音量条)刷新周期

typedef struct
{
    lv_obj_t *left_voice_bar;
    lv_obj_t *right_voice_bar;
    lv_obj_t *power_label;
    lv_obj_t *signal_label;
    lv_obj_t *tab;
    std::string device_mac;

} device_card_data;

// api
device_card_data *ui_info_get_obj(const std::string &mac);
void ui_info_set_bar_pct(lv_obj_t *bar, int8_t pct);

const std::string ui_info_get_transmit_speed();
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
