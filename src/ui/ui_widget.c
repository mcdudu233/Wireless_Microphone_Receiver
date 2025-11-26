#include "ui/ui_widget.h"
//#include <stdio.h>

//160 * 80

// 加载timer的周期回调函数
void loding_timer_cb(lv_timer_t * timer)
{
    // 强转数据
    load_data* ld = (load_data*)lv_timer_get_user_data(timer);
    uint32_t val = lv_bar_get_value(ld->bar) + 1;
    lv_bar_set_value(ld->bar,val,LV_ANIM_ON);
    lv_label_set_text_fmt(ld->label,"%d%%",val);
    if(val >= lv_bar_get_max_value(ld->bar)){
        lv_obj_delete(ld->load_widget);
        ld->next_cb();
        lv_timer_delete(timer);

        lv_free(ld);

        LV_LOG_INFO("加载完成");
    }
}

// 加载完成后回调函数 -> 进入蓝牙连接界面
void finish_loading_cb(lv_event_t * e)
{
    ui_bt_init(NULL);
}

// ui初始化函数
void ui_init(void)
{
    // 设置屏幕背景色为白色

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* start_widget = lv_obj_create(lv_screen_active());
    //
    /* start_widget layout & style */
    obj_set_size(start_widget, 160, 80);
    lv_obj_center(start_widget);
    lv_obj_set_style_pad_all(start_widget, 8, 0);
    lv_obj_set_style_radius(start_widget, 6, 0);
    lv_obj_set_style_bg_color(start_widget, lv_color_hex(0xF0F0F0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(start_widget, LV_OPA_90, LV_PART_MAIN);

    /* Spinner (indeterminate loading indicator) */
    // lv_obj_t * spinner = lv_spinner_create(start_widget);
    // obj_set_size(spinner,20,20);
    // lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 6);

    /* "Loading" label */
    //lv_obj_t * lbl = lv_label_create(start_widget);
    //lv_label_set_text(lbl, "Loading...");
    //lv_obj_align_to(lbl, spinner, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    lv_obj_t* star = lv_img_create(start_widget);
    lv_img_set_src(star,&img_star);
    lv_obj_align(star,LV_ALIGN_TOP_MID,0,3);
    /* Progress bar (static initial value; update via timer/animation elsewhere if needed) */
    lv_obj_t * bar = lv_bar_create(start_widget);
    lv_obj_set_width(bar, lv_pct(65));
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);

    lv_obj_align_to(bar, star, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    /* Percentage label under the bar */
    lv_obj_t * pct = lv_label_create(start_widget);
    lv_label_set_text(pct, "0%");
    lv_obj_align_to(pct, star, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    //lv_obj_align_to(pct, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // 创建定时器
    load_data* ld = (load_data*)lv_malloc(sizeof(load_data));
    LV_ASSERT_MALLOC(ld);

    ld->load_widget = start_widget;
    ld->bar = bar;
    ld->label = pct;
    ld->next_cb = finish_loading_cb;

    lv_timer_create(loding_timer_cb,1,ld); // 设置加载速率
    LV_LOG_INFO("timer已创建");


}
