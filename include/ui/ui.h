#pragma once

#include "ui/ui_res.h"

#include <optional>
#include <string>


#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif


#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

// 实际水平
#define WIDGET_H 160
// 实际垂直
#define WIDGET_V 80

// 主窗口数据获取周期 单位：ms
#define DEVICE_DATA_REFLUSH_TIME 500
// 最大蓝牙连接数
#define MAX_DEVICE_COUNT 4

void ui_bt_init();
void ui_main_init();
void ui_setting_init();

void bind_group_to_all_encoders(lv_group_t *g);
void info_msgbox(std::string title, std::string content);

lv_obj_t *add_win();
lv_obj_t *add_button(lv_obj_t *parent, std::string title, int32_t w, int32_t h, const lv_font_t *font);
lv_obj_t *add_list_obj(lv_obj_t *list, std::string content, lv_event_cb_t cb, const lv_font_t *font, std::optional<lv_color_t> bg_color);


void free_main_widget();
void set_hidden_main_widget(bool state);

static inline void set_bar_val(void *bar, int32_t val)
{
    lv_bar_set_value((lv_obj_t *)bar, val, LV_ANIM_ON);
}

// 数据结构
typedef struct
{
    lv_obj_t *left_voice_bar;
    lv_obj_t *right_voice_bar;
    lv_obj_t *power_bar;
    lv_obj_t *signal_bar;
    char *device_name;

} device_card_data;

typedef enum
{
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
} lv_menu_builder_variant_t;
