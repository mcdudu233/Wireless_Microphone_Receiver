#pragma once
#include "lvgl.h"

// 显示设备
#define WIN 0
#if WIN
// 在win上的缩放倍率
#define SCALE 5
#else
#define SCALE 1
#endif

// 显示屏水平
#define TARGET_WIDGET_H 160
// 显示屏垂直
#define TARGET_WIDGET_V 80

// 实际水平
#define WIDGET_H TARGET_WIDGET_H*SCALE
// 实际垂直
#define WIDGET_V TARGET_WIDGET_V*SCALE



void obj_set_pos(lv_obj_t * obj, int32_t x, int32_t y);
void obj_set_size(lv_obj_t * obj, int32_t w, int32_t h);
void grid_dsc_array(int32_t* dsc_array, int32_t* val, int32_t len);

extern const lv_image_dsc_t bt_o;
extern const lv_image_dsc_t usb;
