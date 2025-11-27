#include "ui/ui.h"
static lv_obj_t *menu;

static void back_cb(lv_event_t *e);      // 设置页面back按钮回调
static void relink_bt_cb(lv_event_t *e); // 重新链接蓝牙回调

static lv_obj_t *create_text(lv_obj_t *parent, const void *icon, const char *txt,
                             lv_menu_builder_variant_t builder_variant);
static lv_obj_t *create_slider(lv_obj_t *parent, const char *icon, const char *txt, int32_t min, int32_t max,
                               int32_t val);
static lv_obj_t *create_switch(lv_obj_t *parent, const char *icon, const char *txt, bool chk);

static lv_obj_t *create_sub_page(lv_obj_t *parent); // 新建子页面

void ui_setting_init(lv_event_t *e)
{
    lv_obj_t *btn;
    lv_obj_t *main_widget = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_add_flag(main_widget, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *setting_widget = add_win();
    setting_back_data *bd = lv_malloc(sizeof(setting_back_data));
    LV_ASSERT_MALLOC(bd);
    bd->main_widget = main_widget;
    bd->setting_widget = setting_widget;

    menu = lv_menu_create(setting_widget);
    lv_color_t bg_color = lv_obj_get_style_bg_color(menu, 0);
    if (lv_color_brightness(bg_color) > 127)
    {
        lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, 0), 10), 0);
    }
    else
    {
        lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, 0), 50), 0);
    }
    lv_menu_set_mode_root_back_button(menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);

    lv_obj_add_event_cb(menu, back_cb, LV_EVENT_CLICKED, bd);
    lv_obj_set_size(menu, lv_display_get_horizontal_resolution(NULL) - 5, lv_display_get_vertical_resolution(NULL) - 5);
    lv_obj_center(menu);

    lv_obj_t *cont;
    lv_obj_t *section;

    /*Create sub pages*/
    lv_obj_t *sub_about_page = create_sub_page(menu);
    lv_obj_t *sub_software_info_page = create_sub_page(menu);
    lv_obj_t *sub_legal_info_page = create_sub_page(menu);
    lv_obj_t *sub_audio_page = create_sub_page(menu);
    lv_obj_t *sub_bt_page = create_sub_page(menu);
    lv_obj_t *sub_wifi_page = create_sub_page(menu);

    lv_obj_t *label = lv_label_create(sub_software_info_page);
    lv_label_set_text(label, "dududu");

    // 关于
    section = lv_menu_section_create(sub_about_page);
    cont = create_text(section, NULL, "Software information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(menu, cont, sub_software_info_page);
    cont = create_text(section, NULL, "Legal information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(menu, cont, sub_legal_info_page);

    // 蓝牙
    section = lv_menu_section_create(sub_bt_page);
    cont = create_text(section, NULL, "返回连接蓝牙界面", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, relink_bt_cb, LV_EVENT_CLICKED, setting_widget);
    // lv_obj_t *sub_mechanics_page = lv_menu_page_create(menu, NULL);
    // lv_obj_set_style_pad_hor(sub_mechanics_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    // lv_menu_separator_create(sub_mechanics_page);
    // section = lv_menu_section_create(sub_mechanics_page);
    // create_slider(section, LV_SYMBOL_SETTINGS, "Velocity", 0, 150, 120);
    // create_slider(section, LV_SYMBOL_SETTINGS, "Acceleration", 0, 150, 50);
    // create_slider(section, LV_SYMBOL_SETTINGS, "Weight limit", 0, 150, 80);
    /*Create a root page*/
    lv_obj_t *root_page = lv_menu_page_create(menu, "设置");
    lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    section = lv_menu_section_create(root_page);
    // cont = create_text(section, NULL, "Mechanics", LV_MENU_ITEM_BUILDER_VARIANT_1);
    // lv_group_add_obj(lv_group_get_default(), cont);
    // lv_menu_set_load_page_event(menu, cont, sub_mechanics_page);

    // create_text(root_page, NULL, "     ", LV_MENU_ITEM_BUILDER_VARIANT_1);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, LV_SYMBOL_AUDIO, "音频", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_audio_page);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, LV_SYMBOL_BLUETOOTH, "蓝牙", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_bt_page);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, LV_SYMBOL_WIFI, "WIFI", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_wifi_page);

    section = lv_menu_section_create(root_page);
    cont = create_text(section, &about, "关于", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);

    // lv_menu_set_sidebar_page(menu, root_page);

    // lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
    //                   NULL);
    lv_menu_set_page(menu, root_page);
}

// 设置页面back按钮回调
static void back_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (lv_menu_back_button_is_root(menu, obj))
    {
        setting_back_data *bd = (setting_back_data *)lv_event_get_user_data(e);
        // lv_obj_set_flag(bd->main_widget,LV_OBJ_FLAG_HIDDEN, false);
        lv_obj_clear_flag(bd->main_widget, LV_OBJ_FLAG_HIDDEN);
        lv_obj_del(bd->setting_widget);
        lv_free(bd);
    }
}
// 重新连接蓝牙点击
static void relink_bt_cb(lv_event_t *e)
{
    ui_bt_init(e);
}

static lv_obj_t *create_text(lv_obj_t *parent, const void *icon, const char *txt,
                             lv_menu_builder_variant_t builder_variant)
{
    lv_obj_t *obj = lv_menu_cont_create(parent);
    lv_obj_t *img = NULL;
    lv_obj_t *label = NULL;

    if (icon)
    {
        img = lv_image_create(obj);
        lv_image_set_src(img, icon);
    }

    if (txt)
    {
        label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
    }

    if (builder_variant == LV_MENU_ITEM_BUILDER_VARIANT_2 && icon && txt)
    {
        lv_obj_add_flag(img, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_swap(img, label);
    }
    // lv_obj_set_style_text_font(obj,&lv_font_montserrat_10,0);
    lv_group_add_obj(lv_group_get_default(), obj);
    return obj;
}
static lv_obj_t *create_slider(lv_obj_t *parent, const char *icon, const char *txt, int32_t min, int32_t max,
                               int32_t val)
{
    lv_obj_t *obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *slider = lv_slider_create(obj);
    lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);

    if (icon == NULL)
    {
        lv_obj_add_flag(slider, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }

    return obj;
}
static lv_obj_t *create_switch(lv_obj_t *parent, const char *icon, const char *txt, bool chk)
{
    lv_obj_t *obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1);

    lv_obj_t *sw = lv_switch_create(obj);
    lv_obj_add_state(sw, chk ? LV_STATE_CHECKED : 0);

    return obj;
}
static lv_obj_t *create_sub_page(lv_obj_t *parent)
{
    lv_obj_t *sub_page = lv_menu_page_create(parent, NULL);
    lv_obj_set_style_pad_hor(sub_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(parent), 0), 0);
    lv_menu_separator_create(sub_page);
    return sub_page;
}
