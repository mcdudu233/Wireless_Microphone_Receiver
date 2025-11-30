#pragma once
#include "ui/ui.h"
#include <vector>

typedef struct
{
    lv_obj_t *left_voice_bar;
    lv_obj_t *right_voice_bar;
    lv_obj_t *power_bar;
    lv_obj_t *signal_bar;
    char *device_name;

} device_card_data;

// api
std::vector<std::string> ui_get_linked_bt();
