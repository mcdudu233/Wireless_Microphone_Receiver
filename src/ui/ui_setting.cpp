#include "ui/ui.h"
#include "ui/ui_setting.h"
#include <string>
#include <optional>

static lv_obj_t *menu;
static lv_obj_t *setting_widget;
static lv_obj_t *last_enter_btn = NULL; // 记录进入子页面所用的条目

static void back_cb(lv_event_t *e);          // 设置页面back按钮回调
static void relink_bt_cb(lv_event_t *e);     // 重新链接蓝牙回调
static void enter_subpage_cb(lv_event_t *e); // 记录进入子页的来源条目
static void focus_async_cb(void *obj_p);     // 异步将焦点移回来源条目

static lv_obj_t *create_text(lv_obj_t *parent, const void *icon, const char *txt,
                             lv_menu_builder_variant_t builder_variant, std::optional<lv_obj_t **> label_o = std::nullopt);
static lv_obj_t *create_slider(lv_obj_t *parent, const char *icon, const char *txt, int32_t min, int32_t max,
                               int32_t val);
static lv_obj_t *create_switch(lv_obj_t *parent, const char *icon, const char *txt, bool chk);
static lv_obj_t *create_dropdown(lv_obj_t *parent, const void *icon, const char *txt, const char *options, std::optional<lv_obj_t **> dd_o = std::nullopt);
static lv_obj_t *create_sub_page(lv_obj_t *parent); // 新建子页面
void ui_setting_init()
{
    lv_obj_t *btn;

    set_hidden_main_widget(true);
    setting_widget = add_win();
    menu = lv_menu_create(setting_widget);
    lv_color_t bg_color = lv_obj_get_style_bg_color(menu, LV_PART_MAIN);
    if (lv_color_brightness(bg_color) > 127)
    {
        lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, LV_PART_MAIN), 10), 0);
    }
    else
    {
        lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, LV_PART_MAIN), 50), 0);
    }
    lv_menu_set_mode_root_back_button(menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);

    lv_obj_add_event_cb(menu, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(menu, lv_display_get_horizontal_resolution(NULL) - 5, lv_display_get_vertical_resolution(NULL) - 5);
    lv_obj_center(menu);

    lv_obj_t *cont;
    lv_obj_t *section;
    lv_obj_t *dd;

    /*Create sub pages*/
    lv_obj_t *sub_about_page = create_sub_page(menu);
    lv_obj_t *sub_usb_page = create_sub_page(menu);
    lv_obj_t *sub_system_page = create_sub_page(menu);
    lv_obj_t *sub_audio_page = create_sub_page(menu);
    lv_obj_t *sub_wireless_page = create_sub_page(menu);

    // 关于
    section = lv_menu_section_create(sub_about_page);
    create_text(section, NULL, "无线麦克风", LV_MENU_ITEM_BUILDER_VARIANT_1);
    create_text(section, NULL, ("固件版本:" + std::string(VERSION)).c_str(), LV_MENU_ITEM_BUILDER_VARIANT_1);
    section = lv_menu_section_create(sub_about_page);
    create_text(section, NULL, "作者:awa", LV_MENU_ITEM_BUILDER_VARIANT_1);
    create_text(section, NULL, ("外部链接:\n" + std::string(EXTERNAL_LINK)).c_str(), LV_MENU_ITEM_BUILDER_VARIANT_1);

    section = lv_menu_section_create(sub_system_page); // CPU核1、2占用 运行内存（IRAM PSRAM） tf储存
    create_text(section, NULL, "CPU 1", LV_MENU_ITEM_BUILDER_VARIANT_1);
    create_text(section, NULL, "CPU 2", LV_MENU_ITEM_BUILDER_VARIANT_1);
    create_text(section, NULL, "IRAM", LV_MENU_ITEM_BUILDER_VARIANT_1);
    create_text(section, NULL, "PSRAM", LV_MENU_ITEM_BUILDER_VARIANT_1);
    create_text(section, NULL, "外置存储", LV_MENU_ITEM_BUILDER_VARIANT_1);

    section = lv_menu_section_create(sub_usb_page);
    create_dropdown(section, NULL, "传输模式", "音频传输\n"
                                               "读卡器",
                    &dd);

    // 音频 频率 比特 通道 下拉菜单
    // 48000 96000 192000 Hz
    // 16 24 32 bit
    // 单声道 立体
    section = lv_menu_section_create(sub_audio_page);
    create_dropdown(section, NULL, "采样率", "48000Hz\n"
                                             "96000Hz\n"
                                             "192000Hz",
                    &dd);
    section = lv_menu_section_create(sub_audio_page);
    create_dropdown(section, NULL, "位深度", "16bit\n"
                                             "24bit\n"
                                             "32bit");
    section = lv_menu_section_create(sub_audio_page);
    create_dropdown(section, NULL, "通道数", "单通道\n"
                                             "立体声",
                    &dd);

    section = lv_menu_section_create(sub_wireless_page);
    create_dropdown(section, NULL, "传输协议", "BLE\n"
                                               "UDP\n"
                                               "TCP",
                    &dd);

    // lv_obj_align(dd, LV_ALIGN_TOP_MID, 0, 20);
    // lv_obj_add_event_cb(dd, event_handler, LV_EVENT_ALL, NULL);

    // lv_obj_t *sub_mechanics_page = lv_menu_page_create(menu, NULL);
    // lv_obj_set_style_pad_hor(sub_mechanics_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    // lv_menu_separator_create(sub_mechanics_page);
    // section = lv_menu_section_create(sub_mechanics_page);
    // create_slider(section, LV_SYMBOL_SETTINGS, "Velocity", 0, 150, 120);
    // create_slider(section, LV_SYMBOL_SETTINGS, "Acceleration", 0, 150, 50);
    // create_slider(section, LV_SYMBOL_SETTINGS, "Weight limit", 0, 150, 80);
    /*Create a root page*/
    lv_obj_t *root_page = lv_menu_page_create(menu, "设置");
    lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), LV_PART_MAIN), 0);
    section = lv_menu_section_create(root_page);
    // cont = create_text(section, NULL, "Mechanics", LV_MENU_ITEM_BUILDER_VARIANT_1);
    // lv_group_add_obj(lv_group_get_default(), cont);
    // lv_menu_set_load_page_event(menu, cont, sub_mechanics_page);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, LV_SYMBOL_AUDIO, "音频设置", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_audio_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, LV_SYMBOL_BLUETOOTH, "设备连接", LV_MENU_ITEM_BUILDER_VARIANT_1); // finish
    lv_group_add_obj(lv_group_get_default(), cont);
    // lv_menu_set_load_page_event(menu, cont, sub_bt_page);
    lv_obj_add_event_cb(cont, relink_bt_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, LV_SYMBOL_WIFI, "无线传输设置", LV_MENU_ITEM_BUILDER_VARIANT_1); // TODO: 传输协议（BLE udp tcp）
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_wireless_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, LV_SYMBOL_USB, "USB传输设置", LV_MENU_ITEM_BUILDER_VARIANT_1); // TODO: 模式（音频传输、读卡器）
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_usb_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = create_text(section, &system_info, "系统信息", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_system_page); // TODO: AWA  CPU核1、2占用 运行内存（IRAM PSRAM） tf储存
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);

    section = lv_menu_section_create(root_page);
    cont = create_text(section, &about, "关于", LV_MENU_ITEM_BUILDER_VARIANT_1); // TODO: 大标题 固件版本 作者 链接
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);

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
        set_hidden_main_widget(false);
        lv_obj_del(setting_widget);
    }
    else
    {
        // 返回到上一级（主页面），将焦点移回进入该子页的条目
        if (last_enter_btn && lv_obj_is_valid(last_enter_btn))
        {
            // 异步执行，确保菜单已完成页面切换
            lv_async_call(focus_async_cb, last_enter_btn);
        }
    }
}
// 重新连接蓝牙点击
static void relink_bt_cb(lv_event_t *e)
{
    lv_obj_del(setting_widget);
    free_main_widget();
    ui_bt_init();
}

static lv_obj_t *create_text(lv_obj_t *parent, const void *icon, const char *txt,
                             lv_menu_builder_variant_t builder_variant, std::optional<lv_obj_t **> label_o)
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
        // lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
    }

    if (builder_variant == LV_MENU_ITEM_BUILDER_VARIANT_2 && icon && txt)
    {
        lv_obj_add_flag(img, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_swap(img, label);
    }
    // lv_obj_set_style_text_font(obj,&lv_font_montserrat_10,0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_group_add_obj(lv_group_get_default(), obj);

    // lv_obj_add_event_cb(obj, [](lv_event_t *e)
    //                     {
    //     lv_obj_t* target = lv_event_get_target_obj(e);
    //     lv_obj_scroll_to_view(target, LV_ANIM_ON);
    // }, LV_EVENT_FOCUSED, NULL);
    if (label_o.has_value())
    {
        *label_o.value() = label;
    }
    return obj;
}
static lv_obj_t *create_slider(lv_obj_t *parent, const void *icon, const char *txt, int32_t min, int32_t max,
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
static lv_obj_t *create_switch(lv_obj_t *parent, const void *icon, const char *txt, bool chk)
{
    lv_obj_t *obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1);

    lv_obj_t *sw = lv_switch_create(obj);
    lv_obj_add_state(sw, chk ? LV_STATE_CHECKED : LV_STATE_DEFAULT);

    return obj;
}
// 创建下拉列表， options: apple\nbanana...
static lv_obj_t *create_dropdown(lv_obj_t *parent, const void *icon, const char *txt, const char *options, std::optional<lv_obj_t **> dd_o)
{
    lv_obj_t *label;
    lv_obj_t *obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1, &label);

    // 从焦点组中移除标签容器，使其无法被编码器选中
    lv_group_remove_obj(obj);

    lv_obj_t *dd = lv_dropdown_create(obj);
    lv_dropdown_set_options(dd, options);

    lv_obj_set_size(dd, 100, 20);
    lv_obj_add_flag(dd, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    // 给下拉菜单添加滚动到视图的标志
    lv_obj_add_flag(dd, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    // 将下拉菜单添加到焦点组，使其可以被编码器选中
    lv_group_add_obj(lv_group_get_default(), dd);

    // 添加焦点事件处理，当下拉菜单获得焦点时，确保标签容器也滚动到可见区域
    lv_obj_add_event_cb(dd, [](lv_event_t *e)
                        {
        lv_obj_t *dd = lv_event_get_target_obj(e);
        lv_obj_t *container = lv_obj_get_parent(dd); // 获取标签容器
        if (container && lv_obj_is_valid(container)) {
            lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
        } }, LV_EVENT_FOCUSED, NULL);

    if (dd_o.has_value())
    {
        *dd_o.value() = dd;
    }
    return obj;
}
static lv_obj_t *create_sub_page(lv_obj_t *parent)
{
    lv_obj_t *sub_page = lv_menu_page_create(parent, NULL);
    lv_obj_set_style_pad_hor(sub_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(parent), LV_PART_MAIN), 0);
    lv_menu_separator_create(sub_page);
    return sub_page;
}

// 记录进入子页的来源条目
static void enter_subpage_cb(lv_event_t *e)
{
    last_enter_btn = lv_event_get_target_obj(e);
}

// 异步把焦点放回来源条目（并滚动可见）
static void focus_async_cb(void *obj_p)
{
    lv_obj_t *obj = (lv_obj_t *)obj_p;
    if (obj && lv_obj_is_valid(obj))
    {
        lv_group_focus_obj(obj);
        lv_obj_scroll_to_view(obj, LV_ANIM_ON);
        lv_obj_add_state(obj, LV_STATE_FOCUS_KEY); // 使返回焦点正常显示
    }
}
