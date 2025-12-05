#pragma once
#include "ui/ui.h"
#include <vector>

typedef struct
{
    lv_obj_t *left_voice_bar;
    lv_obj_t *right_voice_bar;
    lv_obj_t *power_label;
    lv_obj_t *signal_label;
    lv_obj_t *tab;
    char *device_name;

} device_card_data;

// api
device_card_data *ui_info_get_obj(const std::string &mac);
void ui_info_set_bar_pct(lv_obj_t *&bar, int8_t pct);
void ui_info_set_upload(const std::string &speed);
void ui_info_set_download(const std::string &speed);
void ui_info_del_card(const std::string &mac);
void ui_info_set_power(const std::string &mac, int8_t pct);
void ui_info_set_power(device_card_data *card, int8_t pct);
void ui_info_set_signal(const std::string &mac, int8_t pct);
void ui_info_set_signal(device_card_data *card, int8_t pct);
void ui_info_del_card(device_card_data *&card);

// 定义
std::vector<std::string> ui_bt_get_linked();
