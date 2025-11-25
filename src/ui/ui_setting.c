#include "ui/ui.h"

// 设置页面back按钮回调
void back_to_main_cb(lv_event_t *e)
{
    setting_back_data *bd = (setting_back_data *)lv_event_get_user_data(e);
    // lv_obj_set_flag(bd->main_widget,LV_OBJ_FLAG_HIDDEN, false);
    lv_obj_clear_flag(bd->main_widget, LV_OBJ_FLAG_HIDDEN);
    lv_obj_del(bd->setting_widget);
    lv_free(bd);
}

static lv_obj_t *create_text(lv_obj_t *parent, const char *icon, const char *txt,
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
    // btn = add_button(setting_widget,"text",100,60,&lv_font_montserrat_12);
    // lv_obj_align(btn,LV_ALIGN_CENTER,0,0);
    // lv_obj_add_event_cb(btn,back_to_main_cb,LV_EVENT_CLICKED,bd);
    lv_obj_t *menu = lv_menu_create(setting_widget);

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
    lv_obj_add_event_cb(menu, back_to_main_cb, LV_EVENT_CLICKED, bd);
    lv_obj_set_size(menu, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
    lv_obj_center(menu);

    lv_obj_t *cont;
    lv_obj_t *section;

    /*Create sub pages*/
    lv_obj_t *sub_mechanics_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_mechanics_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_mechanics_page);
    section = lv_menu_section_create(sub_mechanics_page);
    create_slider(section, LV_SYMBOL_SETTINGS, "Velocity", 0, 150, 120);
    create_slider(section, LV_SYMBOL_SETTINGS, "Acceleration", 0, 150, 50);
    create_slider(section, LV_SYMBOL_SETTINGS, "Weight limit", 0, 150, 80);

    lv_obj_t *sub_sound_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_sound_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_sound_page);
    section = lv_menu_section_create(sub_sound_page);
    create_switch(section, LV_SYMBOL_AUDIO, "Sound", false);

    lv_obj_t *sub_display_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_display_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_display_page);
    section = lv_menu_section_create(sub_display_page);
    create_slider(section, LV_SYMBOL_SETTINGS, "Brightness", 0, 150, 100);

    lv_obj_t *sub_software_info_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_software_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    section = lv_menu_section_create(sub_software_info_page);
    create_text(section, NULL, "Version 1.0", LV_MENU_ITEM_BUILDER_VARIANT_1);

    lv_obj_t *sub_legal_info_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_legal_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    section = lv_menu_section_create(sub_legal_info_page);
    for (uint32_t i = 0; i < 15; i++)
    {
        create_text(section, NULL,
                    "This is a long long long long long long long long long text, if it is long enough it may scroll.",
                    LV_MENU_ITEM_BUILDER_VARIANT_1);
    }

    lv_obj_t *sub_about_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_about_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_about_page);
    section = lv_menu_section_create(sub_about_page);
    cont = create_text(section, NULL, "Software information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(menu, cont, sub_software_info_page);
    cont = create_text(section, NULL, "Legal information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(menu, cont, sub_legal_info_page);

    lv_obj_t *sub_menu_mode_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_menu_mode_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_menu_mode_page);
    section = lv_menu_section_create(sub_menu_mode_page);
    cont = create_switch(section, LV_SYMBOL_AUDIO, "Sidebar enable", true);
    // lv_obj_add_event_cb(lv_obj_get_child(cont, 2), switch_handler, LV_EVENT_VALUE_CHANGED, menu);

    /*Create a root page*/
    lv_obj_t *root_page = lv_menu_page_create(menu, LV_SYMBOL_SETTINGS);
    // lv_obj_set_style_text_font(root_page,&lv_font_montserrat_10,0);
    lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, NULL, "Mechanics", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(),cont);
    lv_menu_set_load_page_event(menu, cont, sub_mechanics_page);
    cont = create_text(section, NULL, "Sound", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(),cont);
    lv_menu_set_load_page_event(menu, cont, sub_sound_page);
    cont = create_text(section, NULL, "Display", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(),cont);
    lv_menu_set_load_page_event(menu, cont, sub_display_page);

    create_text(root_page, NULL, "Others", LV_MENU_ITEM_BUILDER_VARIANT_1);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, NULL, "About", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(),cont);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);
    cont = create_text(section, NULL, "Menu mode", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(),cont);
    lv_menu_set_load_page_event(menu, cont, sub_menu_mode_page);

    lv_menu_set_sidebar_page(menu, root_page);

    lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
                      NULL);


}
