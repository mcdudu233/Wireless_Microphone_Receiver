#include "ui/ui.h"

// 设置按钮回调 -> 进入设置界面
void setting_widget_cb(lv_event_t * e)
{
    ui_setting_init(e);
}


void ui_main_init(lv_event_t * e)
{
    static lv_style_t style_indic_h;  // 横向bar样式

    lv_style_init(&style_indic_h);
    lv_style_set_bg_opa(&style_indic_h, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic_h, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style_indic_h, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_dir(&style_indic_h, LV_GRAD_DIR_HOR);

    static lv_style_t style_indic_v; // 纵向bar样式

    lv_style_init(&style_indic_v);
    lv_style_set_bg_opa(&style_indic_v, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic_v, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_color(&style_indic_v, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_bg_grad_dir(&style_indic_v, LV_GRAD_DIR_VER);

    lv_obj_t * btn;
    lv_obj_t * label;
    lv_obj_t * bt_widget = (lv_obj_t*) lv_event_get_user_data(e);
    lv_obj_del(bt_widget); // 删除蓝牙窗口组件

    lv_obj_t * main_widget = add_win();

    lv_obj_t * info_widget = lv_obj_create(main_widget);
    lv_obj_set_style_bg_color(info_widget,lv_color_hex(0xfef7ff),0);
    lv_obj_set_style_pad_all(info_widget, 0, 0); // 去除内边距
    lv_obj_set_size(info_widget, 160*0.6,70);
    //obj_set_pos(info_widget,0,0);
    lv_obj_align_to(info_widget,main_widget,LV_ALIGN_TOP_LEFT,5,5);

    // 第一个
    lv_obj_t* f_card = lv_obj_create(info_widget);
    lv_obj_set_style_pad_all(f_card, 0, 0);
    lv_obj_set_size(f_card, 160*0.6-15,(70-15)/2);
    lv_obj_align(f_card,LV_ALIGN_TOP_LEFT,5,5);

    lv_obj_t * bt = lv_label_create(f_card);
    lv_label_set_text(bt, LV_SYMBOL_BLUETOOTH);  // 使用内置的蓝牙符号
    lv_obj_set_style_text_font(bt, &lv_font_montserrat_12, 0);  // 设置合适的字体大小
    lv_obj_align(bt,LV_ALIGN_LEFT_MID, 2, 0);

    lv_obj_t* left_bar_1 = lv_bar_create(f_card);
    lv_obj_add_style(left_bar_1, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_size(left_bar_1, 160*0.6-15-10-12-5-10, 5);
    lv_obj_align_to(left_bar_1,bt,LV_ALIGN_OUT_RIGHT_TOP,2,0);
    lv_bar_set_range(left_bar_1, 0, 100);

    lv_obj_t* right_bar_1 = lv_bar_create(f_card);
    lv_obj_add_style(right_bar_1, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_size(right_bar_1, 160*0.6-15-10-12-5-10, 5);
    lv_obj_align_to(right_bar_1,bt,LV_ALIGN_OUT_RIGHT_BOTTOM,2,0);
    lv_bar_set_range(right_bar_1, 0, 100);

    lv_obj_t* power_bar_1 = lv_bar_create(f_card);
    lv_obj_add_style(power_bar_1, &style_indic_v, LV_PART_INDICATOR);
    lv_obj_set_size(power_bar_1, 5, 15);
    lv_obj_align_to(power_bar_1,left_bar_1,LV_ALIGN_OUT_RIGHT_TOP,3,0);
    lv_bar_set_range(power_bar_1, 0, 100);

    lv_obj_t* signal_bar_1 = lv_bar_create(f_card);
    lv_obj_add_style(signal_bar_1, &style_indic_v, LV_PART_INDICATOR);
    lv_obj_set_size(signal_bar_1, 5, 15);
    lv_obj_align_to(signal_bar_1,power_bar_1,LV_ALIGN_OUT_RIGHT_TOP,3,0);
    lv_bar_set_range(signal_bar_1, 0, 100);

    // 颜色测试
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_bar_val);
    lv_anim_set_duration(&a, 3000);
    lv_anim_set_reverse_duration(&a, 3000);
    lv_anim_set_var(&a, left_bar_1);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_exec_cb(&b, set_bar_val);
    lv_anim_set_duration(&b, 2000);
    //lv_anim_set_reverse_duration(&a, 3000);
    lv_anim_set_var(&b, right_bar_1);
    lv_anim_set_values(&b, 0, 100);
    lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&b);

    lv_anim_t c;
    lv_anim_init(&c);
    lv_anim_set_exec_cb(&c, set_bar_val);
    lv_anim_set_duration(&c, 2000);
    lv_anim_set_reverse_duration(&c, 2000);
    lv_anim_set_var(&c, power_bar_1);
    lv_anim_set_values(&c, 0, 100);
    lv_anim_set_repeat_count(&c, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&c);

    lv_anim_t d;
    lv_anim_init(&d);
    lv_anim_set_exec_cb(&d, set_bar_val);
    lv_anim_set_duration(&d, 2000);
    lv_anim_set_reverse_duration(&d, 2000);
    lv_anim_set_var(&d, signal_bar_1);
    lv_anim_set_values(&d, 0, 100);
    lv_anim_set_repeat_count(&d, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&d);

    // 第二个
    lv_obj_t* s_card = lv_obj_create(info_widget);
    lv_obj_set_style_pad_all(s_card, 0, 0);
    lv_obj_set_size(s_card, 160*0.6-15,(70-15)/2);
    lv_obj_align_to(s_card,f_card,LV_ALIGN_OUT_BOTTOM_LEFT,0,5);

    lv_obj_t * bt_2 = lv_label_create(s_card);
    lv_label_set_text(bt_2, LV_SYMBOL_BLUETOOTH);  // 使用内置的蓝牙符号
    lv_obj_set_style_text_font(bt_2, &lv_font_montserrat_12, 0);  // 设置合适的字体大小
    lv_obj_align(bt_2,LV_ALIGN_LEFT_MID, 2, 0);

    lv_obj_t* left_bar_2 = lv_bar_create(s_card);
    lv_obj_add_style(left_bar_2, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_size(left_bar_2, 160*0.6-15-10-12-5-10, 5);
    lv_obj_align_to(left_bar_2,bt_2,LV_ALIGN_OUT_RIGHT_TOP,2,0);
    lv_bar_set_range(left_bar_2, 0, 100);

    lv_obj_t* right_bar_2 = lv_bar_create(s_card);
    lv_obj_add_style(right_bar_2, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_size(right_bar_2, 160*0.6-15-10-12-5-10, 5);
    lv_obj_align_to(right_bar_2,bt_2,LV_ALIGN_OUT_RIGHT_BOTTOM,2,0);
    lv_bar_set_range(right_bar_2, 0, 100);

    lv_obj_t* power_bar_2 = lv_bar_create(s_card);
    lv_obj_add_style(power_bar_2, &style_indic_v, LV_PART_INDICATOR);
    lv_obj_set_size(power_bar_2, 5, 15);
    lv_obj_align_to(power_bar_2,left_bar_2,LV_ALIGN_OUT_RIGHT_TOP,3,0);
    lv_bar_set_range(power_bar_2, 0, 100);

    lv_obj_t* signal_bar_2 = lv_bar_create(s_card);
    lv_obj_add_style(signal_bar_2, &style_indic_v, LV_PART_INDICATOR);
    lv_obj_set_size(signal_bar_2, 5, 15);
    lv_obj_align_to(signal_bar_2,power_bar_2,LV_ALIGN_OUT_RIGHT_TOP,3,0);
    lv_bar_set_range(signal_bar_2, 0, 100);


    lv_obj_t *label_widget = lv_obj_create(main_widget);
    lv_obj_set_style_pad_all(label_widget, 0, 0); // 去除内边距
    lv_obj_set_size(label_widget,160*0.3,25);
    lv_obj_align_to(label_widget,info_widget,LV_ALIGN_OUT_RIGHT_MID,5,-5);
    label = lv_label_create(label_widget);
    lv_label_set_text(label,"up:");
    lv_obj_set_style_text_font(label,&lv_font_montserrat_8,0);
    lv_obj_align(label,LV_ALIGN_TOP_LEFT,0,0);
    // lv_obj_align_to(label,info_widget,LV_ALIGN_OUT_RIGHT_MID,10,-10);
    lv_obj_t * last = label;
    label = lv_label_create(label_widget);
    lv_label_set_text(label,"down:");
    lv_obj_set_style_text_font(label,&lv_font_montserrat_8,0);
    lv_obj_align_to(label,last,LV_ALIGN_OUT_BOTTOM_LEFT,0,0);

    btn = add_button(main_widget,"setting",160*0.3,20,&lv_font_montserrat_10);
    lv_obj_align_to(btn,info_widget,LV_ALIGN_OUT_RIGHT_MID,5,20);
    lv_obj_add_event_cb(btn,setting_widget_cb,LV_EVENT_CLICKED,main_widget); // 切换窗体并隐藏

    lv_obj_t* img;
    img = lv_img_create(main_widget);
    lv_obj_set_size(img,16,16);
    lv_img_set_src(img,&usb);
    lv_obj_align_to(img,label_widget,LV_ALIGN_OUT_TOP_LEFT,1,-5);
    last = img;
    img = lv_img_create(main_widget);
    lv_obj_set_size(img,16,16);
    lv_img_set_src(img,&bt_o);
    lv_obj_align_to(img,last,LV_ALIGN_OUT_RIGHT_MID,2,0);
    last = img;
    // char* bt_svg = "<svg t=\"1763872980562\" class=\"icon\" viewBox=\"0 0 1024 1024\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" p-id=\"5937\" width=\"16\" height=\"16\"><path d=\"M512 593.066667L337.066667 768l-29.866667-29.866667 204.8-204.8v-4.266666l-213.333333-213.333334 29.866666-29.866666L512 469.333333V128l200.533333 200.533333 29.866667 29.866667-170.666667 170.666667 140.8 140.8 29.866667 29.866666-230.4 234.666667v-341.333333z m170.666667-234.666667l-128-128v260.266667l128-132.266667z m0 341.333333l-128-128v260.266667l128-132.266667z\" fill=\"#444444\" p-id=\"5938\"></path></svg>";
    // static lv_image_dsc_t svg_dsc;
    // svg_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    // svg_dsc.header.w = 16;
    // svg_dsc.header.h = 16;
    // svg_dsc.data_size = sizeof(bt_svg) - 1;
    // svg_dsc.data = (const uint8_t *) bt_svg;

    // lv_obj_t * svg = lv_image_create(main_widget);
    // lv_image_set_src(svg, &svg_dsc);
    // lv_obj_align_to(svg,last,LV_ALIGN_OUT_RIGHT_MID,5,0);
    // 使用 LV_SYMBOL 替代 SVG,或者使用文本标签
    lv_obj_t * bt_label = lv_label_create(main_widget);
    lv_label_set_text(bt_label, LV_SYMBOL_BLUETOOTH);  // 使用内置的蓝牙符号
    lv_obj_set_style_text_font(bt_label, &lv_font_montserrat_12, 0);  // 设置合适的字体大小
    lv_obj_align_to(bt_label, last, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
}
