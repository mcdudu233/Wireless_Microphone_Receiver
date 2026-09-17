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
extern const lv_image_dsc_t ui_img_system_info;
extern const lv_image_dsc_t ui_img_screen;
extern const lv_image_dsc_t ui_img_about;
extern const lv_image_dsc_t ui_img_microphone;
extern const lv_image_dsc_t ui_img_audio;
extern const lv_image_dsc_t ui_img_usb;
extern const lv_image_dsc_t ui_img_wifi;
extern const lv_image_dsc_t ui_img_bt;
extern const lv_image_dsc_t ui_img_tips;
extern const lv_image_dsc_t ui_img_file;
extern const lv_image_dsc_t lv_img_bluetooth;

// 主界面状态栏彩色图标(采样率/USB模式/传输模式,语义化颜色编码状态)
extern const lv_image_dsc_t ui_img_rate48;
extern const lv_image_dsc_t ui_img_rate96;
extern const lv_image_dsc_t ui_img_rate192;
extern const lv_image_dsc_t ui_img_usb_none;
extern const lv_image_dsc_t ui_img_usb_audio;
extern const lv_image_dsc_t ui_img_usb_sd;
extern const lv_image_dsc_t ui_img_usb_jtag;
extern const lv_image_dsc_t ui_img_rf_ble;
extern const lv_image_dsc_t ui_img_rf_wifi;

// 字体资源
extern const lv_font_t lv_font_harmonyos_12;
extern const lv_font_t lv_font_harmonyos_14;
extern const lv_font_t lv_font_harmonyos_16;

// Compact ASCII annotations on the main dashboard only.
extern const lv_font_t lv_font_harmonyos_status_10;
