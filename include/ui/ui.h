#pragma once
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

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

typedef void (*callback)();

void ui_bt_init(lv_event_t * e);
void ui_main_init(lv_event_t * e);
void ui_setting_init(lv_event_t * e);

void obj_set_pos(lv_obj_t * obj, int32_t x, int32_t y);
void obj_set_size(lv_obj_t * obj, int32_t w, int32_t h);
void grid_dsc_array(int32_t* dsc_array, int32_t* val, int32_t len);

lv_obj_t* add_win();
lv_obj_t* add_button(lv_obj_t * parent, char * title, int32_t w, int32_t h,const lv_font_t * font);
lv_obj_t* add_list_obj(lv_obj_t * list, char * content, lv_event_cb_t cb,const lv_font_t * font, lv_palette_t bg_color);


static inline void set_bar_val(void * bar, int32_t val)
{
    lv_bar_set_value((lv_obj_t *)bar, val, LV_ANIM_ON);
}

extern const lv_image_dsc_t bt_o;
extern const lv_image_dsc_t usb;
extern const lv_image_dsc_t img_star;

extern const lv_font_t lv_font_harmonyos_12;
extern const lv_font_t lv_font_harmonyos_14;
extern const lv_font_t lv_font_harmonyos_16;


typedef struct
{
    lv_obj_t* load_widget;
    lv_obj_t* bar;
    lv_obj_t* label;
    callback next_cb;
} load_data;
typedef struct
{
    lv_obj_t* main_widget;
    lv_obj_t* setting_widget;
} setting_back_data;
typedef enum {
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
} lv_menu_builder_variant_t;
enum bt_section_color{
    selected = LV_PALETTE_YELLOW,
    linked = LV_PALETTE_GREEN,
    none = -1
} ;
