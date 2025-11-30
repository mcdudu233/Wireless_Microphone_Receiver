#include "ui/ui.h"
#include "ui/ui_loading.h"

static lv_obj_t *loading_widget;
static lv_obj_t *bar;
static lv_obj_t *pct;       // 标签
static uint8_t percent = 0; // 加载百分比
static const char *loading_part = "";

void ui_loading_set_percent(uint8_t p)
{
    percent = p;
}

// 设置当前加载部分的名字
void ui_loading_set_part(const char *part)
{
    loading_part = part;
}

// 加载timer的周期回调函数
static void loding_timer_cb(lv_timer_t *timer)
{
    uint32_t val = lv_bar_get_value(bar);
    lv_label_set_text_fmt(pct, "正在加载:%s", loading_part);
    if (percent > val)
    {
        val++;
        lv_bar_set_value(bar, val, LV_ANIM_ON);
        // lv_label_set_text_fmt(pct, "%d%%", val);
    }
    if (val >= lv_bar_get_max_value(bar))
    {

        lv_obj_delete(loading_widget);
        ui_bt_init();
        LV_LOG_INFO("加载完成");
        lv_timer_delete(timer);
    }
}

// ui初始化函数
void ui_loading_init()
{
    // 设置屏幕背景色为白色
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    loading_widget = lv_obj_create(lv_screen_active());
    lv_obj_set_size(loading_widget, 160, 80);
    lv_obj_center(loading_widget);
    lv_obj_set_style_pad_all(loading_widget, 8, 0);
    lv_obj_set_style_radius(loading_widget, 6, 0);
    lv_obj_set_style_bg_color(loading_widget, lv_color_hex(0xF0F0F0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(loading_widget, LV_OPA_90, LV_PART_MAIN);

    lv_obj_t *img = lv_img_create(loading_widget);
    lv_img_set_src(img, &microphone);
    lv_img_set_zoom(img, 64);
    lv_obj_set_size(img, 32, 32);
    lv_obj_align(img, LV_ALIGN_TOP_MID, -30, -5);

    lv_obj_t *label = lv_label_create(loading_widget);
    lv_label_set_recolor(label, true);
    lv_label_set_text(label, "#626367 作者:awa#");
    lv_obj_set_style_text_font(label, &lv_font_harmonyos_16, 0);
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 6, 10);

    bar = lv_bar_create(loading_widget);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 20);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 20);

    pct = lv_label_create(bar);
    lv_label_set_text(pct, "正在加载:screen");
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, 0);

    lv_timer_create(loding_timer_cb, 1, NULL); // 设置加载速率
    LV_LOG_INFO("timer已创建");
}
