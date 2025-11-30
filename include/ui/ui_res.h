#pragma once

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

// 图片资源
extern const lv_image_dsc_t system_info;
extern const lv_image_dsc_t about;
extern const lv_image_dsc_t microphone;

// 字体资源
extern const lv_font_t lv_font_harmonyos_12;
extern const lv_font_t lv_font_harmonyos_14;
extern const lv_font_t lv_font_harmonyos_16;
