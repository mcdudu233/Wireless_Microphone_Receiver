#include "ui/ui_main.h"

static lv_style_t style_indic_h; // 横向bar样式
static lv_style_t style_indic_v;
static lv_timer_t *pull_data_timer;
static lv_obj_t *info_widget;
static lv_obj_t *main_widget;
static lv_obj_t *upload_label;
static lv_obj_t *download_label;
static lv_obj_t *tabview;
static lv_obj_t *tabs[MAX_DEVICE_COUNT];                                                                  // 最多4个                                                                              // 纵向bar样式
static lv_obj_t *ui_create_device_card(lv_obj_t *parent, char *device_name, char *icon, bool color_test); // 创建自定义card容器组件

static void setting_widget_cb(lv_event_t *e);      // 设置按钮回调
static void pull_data_timer_cb(lv_timer_t *timer); // 获取设备数据回调

void ui_main_init()
{
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);

    lv_style_init(&style_indic_h);
    lv_style_set_bg_opa(&style_indic_h, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic_h, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style_indic_h, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_dir(&style_indic_h, LV_GRAD_DIR_HOR);

    lv_style_init(&style_indic_v);
    lv_style_set_bg_opa(&style_indic_v, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic_v, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_color(&style_indic_v, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_bg_grad_dir(&style_indic_v, LV_GRAD_DIR_VER);

    lv_obj_t *btn;
    lv_obj_t *label;
    lv_obj_t *last;

    main_widget = ui_add_win();

    info_widget = lv_obj_create(main_widget);
    lv_obj_set_style_bg_opa(info_widget, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(info_widget, 0, 0); // 去除内边距
    lv_obj_set_style_border_width(info_widget, 1, 0);
    lv_obj_set_size(info_widget, 160 * 0.6, 70);
    lv_obj_align_to(info_widget, main_widget, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_border_width(info_widget, 0, 0); // 去除边框

    tabview = lv_tabview_create(info_widget);
    lv_tabview_set_tab_bar_size(tabview, 20);
    // lv_obj_set_size(tabview, 160 * 0.6, 70);
    lv_obj_align(tabview, LV_ALIGN_CENTER, 0, 0);

    char buf[13];
    int i = 0;
    lv_snprintf(buf, 13, "#0000FF %s#", LV_SYMBOL_BLUETOOTH);
    std::vector<std::string> linked_devices = ui_get_linked_bt();
    for (std::string dev : linked_devices)
    {
        LV_LOG_USER(dev.c_str());
        lv_obj_t *tab = lv_tabview_add_tab(tabview, std::to_string(i + 1).c_str());
        lv_obj_t *card = ui_create_device_card(tab, dev.data(), buf, true);
        lv_obj_set_style_pad_all(tab, 0, 0);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
        tabs[i] = tab;
        i++;
    }

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview); // 标题栏
    uint32_t cnt = lv_obj_get_child_count_by_type(tab_bar, &lv_button_class);
    for (int i = 0; i < cnt; i++)
    {
        btn = lv_obj_get_child_by_type(tab_bar, i, &lv_button_class);
        label = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_font(label, &lv_font_harmonyos_12, 0);
    }

    lv_obj_t *label_widget = lv_obj_create(main_widget);
    lv_obj_set_style_pad_all(label_widget, 0, 0);      // 去除内边距
    lv_obj_set_style_border_width(label_widget, 0, 0); // 去除边框
    lv_obj_set_size(label_widget, 160 * 0.3 + 5, 35);
    lv_obj_align_to(label_widget, info_widget, LV_ALIGN_OUT_RIGHT_MID, 2, -5);

    upload_label = lv_label_create(label_widget);
    lv_label_set_recolor(upload_label, true);
    lv_label_set_text_fmt(upload_label, "#00FF00 %s#%.1fb/s", LV_SYMBOL_UP, 0);
    lv_obj_set_style_text_font(upload_label, &lv_font_harmonyos_12, 0);
    lv_obj_align(upload_label, LV_ALIGN_TOP_LEFT, 0, 0);

    download_label = lv_label_create(label_widget);
    lv_label_set_recolor(download_label, true);
    lv_label_set_text_fmt(download_label, "#00FF00 %s#%.1fb/s", LV_SYMBOL_DOWN, 1.1);
    lv_obj_set_style_text_font(download_label, &lv_font_harmonyos_12, 0);
    lv_obj_align_to(download_label, upload_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    btn = ui_add_button(main_widget, "设置", 160 * 0.3, 25, NULL);
    lv_obj_align_to(btn, info_widget, LV_ALIGN_OUT_RIGHT_MID, 3, 20);
    lv_obj_add_event_cb(btn, setting_widget_cb, LV_EVENT_CLICKED, main_widget); // 切换窗体并隐藏

    // img 使用方式
    // lv_obj_t* img;
    // img = lv_img_create(main_widget);
    // lv_obj_set_size(img,16,16);
    // lv_img_set_src(img,&usb);
    // lv_obj_align_to(img,label_widget,LV_ALIGN_OUT_TOP_LEFT,1,0);
    // last = img;

    lv_obj_t *img;
    img = lv_label_create(main_widget);
    lv_label_set_text(img, LV_SYMBOL_USB);
    lv_obj_set_style_text_font(img, &lv_font_harmonyos_12, 0);
    // lv_obj_set_style_text_color(img, lv_palette_main(LV_PALETTE_NONE), 0); // 设置状态颜色
    lv_obj_align_to(img, label_widget, LV_ALIGN_OUT_TOP_LEFT, 3, 0);
    last = img;
    img = lv_label_create(main_widget);
    lv_label_set_text(img, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(img, &lv_font_harmonyos_12, 0);
    // lv_obj_set_style_text_color(img, lv_palette_main(LV_PALETTE_NONE), 0);
    lv_obj_align_to(img, last, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
    last = img;
    img = lv_label_create(main_widget);
    lv_label_set_text(img, LV_SYMBOL_BLUETOOTH);               // 使用内置的蓝牙符号
    lv_obj_set_style_text_font(img, &lv_font_harmonyos_12, 0); // 设置合适的字体大小
    lv_obj_set_style_text_color(img, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align_to(img, last, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    ui_bind_group_to_all_encoders(lv_group_get_default());

    // 注册timer
    pull_data_timer = lv_timer_create(pull_data_timer_cb, DEVICE_DATA_REFLUSH_TIME, NULL);
}

// 设置按钮回调 -> 进入设置界面
static void setting_widget_cb(lv_event_t *e)
{
    // lv_timer_delete(pull_data_timer);
    ui_setting_init();
}

// 创建设备卡片
static lv_obj_t *ui_create_device_card(lv_obj_t *parent, char *device_name, char *icon, bool color_test)
{
    lv_obj_t *card = lv_obj_create(parent);

    // lv_obj_set_style_border_width(card, 0, 0); // 去除边框
    lv_obj_set_style_pad_all(card, 0, 0);
    // 使用百分比自适应大小，而不是在创建时读取parent尺寸
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, lv_pct(100));

    // lv_obj_t *symbol = lv_label_create(card);
    // lv_label_set_recolor(symbol, true);                           // 允许颜色
    // lv_label_set_text(symbol, icon);                              // 使用内置的蓝牙符号
    // lv_obj_set_style_text_font(symbol, &lv_font_harmonyos_12, 0); // 设置合适的字体大小
    // lv_obj_align(symbol, LV_ALIGN_LEFT_MID, 4, 5);
    lv_obj_t *label;
    label = lv_label_create(card);
    lv_label_set_text(label, "L");
    lv_obj_align(label, LV_ALIGN_CENTER, -40, -5);
    label = lv_label_create(card);
    lv_label_set_text(label, "R");
    lv_obj_align(label, LV_ALIGN_CENTER, -40, 15);

    lv_obj_t *left_bar_1 = lv_bar_create(card);
    lv_obj_add_style(left_bar_1, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_size(left_bar_1, 160 * 0.6 - 15 - 10 - 12 - 10, 10);
    lv_obj_align(left_bar_1, LV_ALIGN_CENTER, -10, -5);
    lv_bar_set_range(left_bar_1, 0, 100);

    lv_obj_t *right_bar_1 = lv_bar_create(card);
    lv_obj_add_style(right_bar_1, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_size(right_bar_1, 160 * 0.6 - 15 - 10 - 12 - 10, 10);
    lv_obj_align(right_bar_1, LV_ALIGN_CENTER, -10, 15);
    lv_bar_set_range(right_bar_1, 0, 100);

    lv_obj_t *power_bar_1 = lv_bar_create(card);
    lv_obj_add_style(power_bar_1, &style_indic_v, LV_PART_INDICATOR);
    lv_obj_set_size(power_bar_1, 10, 30);
    lv_obj_align_to(power_bar_1, right_bar_1, LV_ALIGN_OUT_RIGHT_TOP, 15, -7);
    lv_bar_set_range(power_bar_1, 0, 100);

    lv_obj_t *signal_bar_1 = lv_bar_create(card);
    lv_obj_add_style(signal_bar_1, &style_indic_v, LV_PART_INDICATOR);
    lv_obj_set_size(signal_bar_1, 10, 30);
    lv_obj_align_to(signal_bar_1, power_bar_1, LV_ALIGN_OUT_RIGHT_TOP, 5, 0);
    lv_bar_set_range(signal_bar_1, 0, 100);

    if (color_test)
    {
        // 颜色测试
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_exec_cb(&a, ui_set_bar_val);
        lv_anim_set_duration(&a, 3000);
        lv_anim_set_reverse_duration(&a, 3000);
        lv_anim_set_var(&a, left_bar_1);
        lv_anim_set_values(&a, 0, 100);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);

        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_exec_cb(&b, ui_set_bar_val);
        lv_anim_set_duration(&b, 2000);
        // lv_anim_set_reverse_duration(&a, 3000);
        lv_anim_set_var(&b, right_bar_1);
        lv_anim_set_values(&b, 0, 100);
        lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&b);

        lv_anim_t c;
        lv_anim_init(&c);
        lv_anim_set_exec_cb(&c, ui_set_bar_val);
        lv_anim_set_duration(&c, 2000);
        lv_anim_set_reverse_duration(&c, 2000);
        lv_anim_set_var(&c, power_bar_1);
        lv_anim_set_values(&c, 0, 100);
        lv_anim_set_repeat_count(&c, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&c);

        lv_anim_t d;
        lv_anim_init(&d);
        lv_anim_set_exec_cb(&d, ui_set_bar_val);
        lv_anim_set_duration(&d, 2000);
        lv_anim_set_reverse_duration(&d, 2000);
        lv_anim_set_var(&d, signal_bar_1);
        lv_anim_set_values(&d, 0, 100);
        lv_anim_set_repeat_count(&d, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&d);
    }

    device_card_data *data = (device_card_data *)lv_malloc(sizeof(device_card_data));
    LV_ASSERT_MALLOC(data);
    data->left_voice_bar = left_bar_1; // 通过获取obj的userdata可以直接操作内部控件数值
    data->right_voice_bar = right_bar_1;
    data->power_bar = power_bar_1;
    data->signal_bar = signal_bar_1;
    data->device_name = device_name;

    lv_obj_set_user_data(card, data);

    return card;
}

static void pull_data_timer_cb(lv_timer_t *timer)
{
    // lv_label_set_text_fmt(upload_label, "#00FF00 %s#%s", LV_SYMBOL_UP, get_upload_speed());
    // lv_label_set_text_fmt(download_label, "#00FF00 %s#%s", LV_SYMBOL_DOWN, get_download_speed());

    lv_obj_t *cur_tab = tabs[lv_tabview_get_tab_active(tabview)];
    lv_obj_t *card = lv_obj_get_child(cur_tab, 0);
    device_card_data *card_data = (device_card_data *)lv_obj_get_user_data(card);
    char *name = card_data->device_name;

    // lv_bar_set_value(card_data->left_voice_bar, get_left_voice_per(name), LV_ANIM_ON);
    // lv_bar_set_value(card_data->right_voice_bar, get_right_voice_per(name), LV_ANIM_ON);
    // lv_bar_set_value(card_data->power_bar, get_power_per(name), LV_ANIM_ON);
    // lv_bar_set_value(card_data->signal_bar, get_signal_per(name), LV_ANIM_ON);

    LV_LOG_USER("pull timer触发");
}

void ui_free_main_widget()
{
    for (int i = 0; i < MAX_DEVICE_COUNT; i++)
    {
        if (tabs[i] == NULL)
            break;
        lv_obj_t *cur_tab = tabs[i];
        lv_obj_t *card = lv_obj_get_child(cur_tab, 0);
        if (card == NULL)
            break;
        device_card_data *card_data = (device_card_data *)lv_obj_get_user_data(card);
        lv_free(card_data);
    }
    lv_timer_delete(pull_data_timer);
    lv_obj_delete(main_widget);
}

void ui_set_hidden_main_widget(bool state)
{
    lv_obj_set_flag(main_widget, LV_OBJ_FLAG_HIDDEN, state);
}

// api
std::vector<std::string> ui_get_linked_bt()
{
    std::vector<std::string> dev = {"00:1A:2B:3C:4D:5E", "00:1A:2B:3C:4D:5F", "00:1A:2B:3C:4D:5D"};
    return dev;
}
