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

lv_obj_t *ui_add_win();
lv_obj_t *ui_add_button(lv_obj_t *parent, std::string title, int32_t w, int32_t h, const lv_font_t *font);
lv_obj_t *ui_add_list_obj(lv_obj_t *list, std::string content, lv_event_cb_t cb, const lv_font_t *font, std::optional<lv_color_t> bg_color);

lv_obj_t **ui_popwin(bool has_bg = true, lv_group_t *g = nullptr, lv_obj_t *obj = nullptr);
lv_obj_t *ui_popwin_msgbox(const char *text, lv_group_t *g = nullptr, lv_obj_t *obj = nullptr);
lv_obj_t *ui_popwin_load(const char *text, const void *src, size_t src_size, int32_t time, int32_t w = 64, int32_t h = 64, lv_group_t *g = nullptr, lv_obj_t *obj = nullptr);
lv_obj_t *ui_popwin_finish(const char *text, const void *src, size_t src_size, int32_t time, int32_t w = 64, int32_t h = 64, lv_group_t *g = nullptr, lv_obj_t *obj = nullptr);
lv_obj_t *ui_lottie_create(lv_obj_t *parent, const void *src, size_t src_size, int32_t time, int32_t w = 64, int32_t h = 64);

void ui_free_main_widget();
void ui_set_hidden_main_widget(bool state);
void ui_set_bar_val(void *bar, int32_t val);
