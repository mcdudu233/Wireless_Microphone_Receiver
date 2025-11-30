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

void ui_bind_group_to_all_encoders(lv_group_t *g);
void ui_info_msgbox(std::string title, std::string content);

lv_obj_t *ui_add_win();
lv_obj_t *ui_add_button(lv_obj_t *parent, std::string title, int32_t w, int32_t h, const lv_font_t *font);
lv_obj_t *ui_add_list_obj(lv_obj_t *list, std::string content, lv_event_cb_t cb, const lv_font_t *font, std::optional<lv_color_t> bg_color);

void ui_free_main_widget();
void ui_set_hidden_main_widget(bool state);

void ui_set_bar_val(void *bar, int32_t val)
{
    lv_bar_set_value((lv_obj_t *)bar, val, LV_ANIM_ON);
}
