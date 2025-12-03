#include "ui/ui.h"
#include "ui/ui_setting.h"
#include <string>
#include <optional>

static lv_obj_t *menu;
static lv_obj_t *setting_widget;
static lv_obj_t *last_enter_btn = NULL; // 记录进入子页面所用的条目
static lv_timer_t *sys_info_timer;
static lv_style_t scroll_style;
static bool enter_bottom = false;

static lv_obj_t *cpu1;
static lv_obj_t *cpu2;
static lv_obj_t *iram;
static lv_obj_t *psram;
static lv_obj_t *sd;

static void back_cb(lv_event_t *e);          // 设置页面back按钮回调
static void relink_bt_cb(lv_event_t *e);     // 重新链接蓝牙回调
static void enter_subpage_cb(lv_event_t *e); // 记录进入子页的来源条目
static void focus_async_cb(void *obj_p);     // 异步将焦点移回来源条目
static void choose_cb(lv_event_t *e);        // 下拉菜单选中回调
static void sys_info_timer_cb(lv_timer_t *); // 系统信息更新timer

static lv_obj_t *ui_create_text(lv_obj_t *parent, const void *icon, const char *txt,
                                lv_menu_builder_variant_t builder_variant, std::optional<lv_obj_t **> label_o = std::nullopt, std::optional<bool> is_from_svg = std::nullopt);
static lv_obj_t *ui_create_slider(lv_obj_t *parent, const char *icon, const char *txt, int32_t min, int32_t max,
                                  int32_t val);
static lv_obj_t *ui_create_switch(lv_obj_t *parent, const char *icon, const char *txt, bool chk);
static lv_obj_t *ui_create_dropdown(lv_obj_t *parent, const void *icon, const char *txt, const char *options, std::optional<lv_obj_t **> dd_o = std::nullopt);
static lv_obj_t *ui_create_sub_page(lv_obj_t *parent, const char *title, bool display_scroll = true); // 新建子页面

namespace SystemInfo
{
    uint8_t cpu1 = 0;
    uint8_t cpu2 = 0;
    uint64_t IRAM = 0;
    uint64_t PSRAM = 0;
    uint64_t SD = 0;
}

void ui_setting_init()
{
    lv_obj_t *btn;
    lv_style_init(&scroll_style);
    lv_style_set_pad_right(&scroll_style, 0);

    ui_set_hidden_main_widget(true);
    setting_widget = ui_add_win();
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
    lv_obj_t *sub_about_page = ui_create_sub_page(menu, "关于", false);
    lv_obj_t *sub_usb_page = ui_create_sub_page(menu, "USB传输设置");
    lv_obj_t *sub_system_page = ui_create_sub_page(menu, "系统信息");
    lv_obj_t *sub_audio_page = ui_create_sub_page(menu, "音频设置");
    lv_obj_t *sub_wireless_page = ui_create_sub_page(menu, "无线连接设置");

    // 关于
    section = lv_menu_section_create(sub_about_page);
    // 放入一个可滚动容器，使其显示滚动条，并可被编码器聚焦控制
    lv_obj_t *scroller = lv_obj_create(section);
    lv_obj_set_width(scroller, lv_pct(100));
    lv_obj_set_height(scroller, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(scroller, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(scroller, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scroller, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_group_add_obj(lv_group_get_default(), scroller);
    lv_obj_set_style_bg_opa(scroller, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroller, 0, 0);
    lv_obj_set_style_pad_all(scroller, 0, 0);
    lv_obj_set_style_pad_all(sub_about_page, 0, 0);
    lv_obj_add_style(scroller, &scroll_style, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(scroller, [](lv_event_t *e)
                        {
        lv_obj_t *obj = lv_event_get_target_obj(e);
        lv_group_t *g = lv_obj_get_group(obj);
        if(g) lv_group_set_editing(g, true);
        lv_obj_scroll_to_view_recursive(obj, LV_ANIM_ON); }, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(scroller, [](lv_event_t *e)
                        {
        lv_obj_t *obj = lv_event_get_target_obj(e);
        lv_point_t end;
        lv_obj_get_scroll_end(obj, &end);
        int32_t bottom = lv_obj_get_scroll_bottom(obj);
        if(end.y >= bottom) {
            if(enter_bottom)
            {
                lv_obj_scroll_to_y(obj, 0, LV_ANIM_ON);
                lv_obj_t *header = lv_menu_get_main_header(menu);
                if(header) {
                    lv_obj_t *back_btn = lv_obj_get_child(header, 0);
                    if(back_btn) lv_group_focus_obj(back_btn);
                    lv_group_t *g = lv_obj_get_group(obj);
                    if(g) lv_group_set_editing(g, false);
                }
                enter_bottom = false;
            }else
                enter_bottom = true;
            
        } }, LV_EVENT_SCROLL, NULL);

    lv_obj_t *spans = lv_spangroup_create(scroller);
    // 让文本按容器宽度自动换行，高度由内容决定
    lv_obj_set_width(spans, lv_pct(100));
    lv_obj_set_height(spans, LV_SIZE_CONTENT);
    lv_spangroup_set_align(spans, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(scroller, 53);

    lv_span_t *span;

    span = lv_spangroup_add_span(spans);
    lv_span_set_text(span, ABOUT_INFO);
    lv_style_set_text_color(lv_span_get_style(span), lv_color_hex(0x626367));
    lv_style_set_text_font(lv_span_get_style(span), &lv_font_harmonyos_12);
    // span = lv_spangroup_add_span(spans);
    // lv_span_set_text(span, "作者: awa\n");
    // lv_style_set_text_color(lv_span_get_style(span), lv_color_hex(0x626367));
    // lv_style_set_text_font(lv_span_get_style(span), &lv_font_harmonyos_12);
    // span = lv_spangroup_add_span(spans);
    // lv_span_set_text_fmt(span, "外部链接:%s", EXTERNAL_LINK);
    // lv_style_set_text_color(lv_span_get_style(span), lv_color_hex(0x626367));
    // lv_style_set_text_font(lv_span_get_style(span), &lv_font_harmonyos_12);
    lv_spangroup_refresh(spans);

    // create_text(section, NULL, "无线麦克风", LV_MENU_ITEM_BUILDER_VARIANT_1);
    // create_text(section, NULL, ("固件版本:" + std::string(VERSION)).c_str(), LV_MENU_ITEM_BUILDER_VARIANT_1);
    // create_text(section, NULL, "作者:awa", LV_MENU_ITEM_BUILDER_VARIANT_1);
    // create_text(section, NULL, ("外部链接:\n" + std::string(EXTERNAL_LINK)).c_str(), LV_MENU_ITEM_BUILDER_VARIANT_1);

    section = lv_menu_section_create(sub_system_page); // CPU核1、2占用 运行内存（IRAM PSRAM） tf储存
    ui_create_text(section, NULL, "CPU 1", LV_MENU_ITEM_BUILDER_VARIANT_1, &cpu1);
    ui_create_text(section, NULL, "CPU 2", LV_MENU_ITEM_BUILDER_VARIANT_1, &cpu2);
    ui_create_text(section, NULL, "IRAM", LV_MENU_ITEM_BUILDER_VARIANT_1, &iram);
    ui_create_text(section, NULL, "PSRAM", LV_MENU_ITEM_BUILDER_VARIANT_1, &psram);
    ui_create_text(section, NULL, "外置存储", LV_MENU_ITEM_BUILDER_VARIANT_1, &sd);

    lv_label_set_recolor(cpu1, true);
    lv_label_set_recolor(cpu2, true);
    lv_label_set_recolor(iram, true);
    lv_label_set_recolor(psram, true);
    lv_label_set_recolor(sd, true);

    section = lv_menu_section_create(sub_usb_page);
    ui_create_dropdown(section, NULL, "传输模式", "音频传输\n"
                                                  "读卡器",
                       &dd);
    lv_obj_add_event_cb(dd, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)1);
    // 音频 频率 比特 通道 下拉菜单
    // 48000 96000 192000 Hz
    // 16 24 32 bit
    // 单声道 立体
    section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(section, NULL, "采样率", "48000Hz\n"
                                                "96000Hz\n"
                                                "192000Hz",
                       &dd);
    lv_obj_add_event_cb(dd, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)2);
    section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(section, NULL, "位深度", "16bit\n"
                                                "24bit\n"
                                                "32bit",
                       &dd);
    lv_obj_add_event_cb(dd, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)3);
    section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(section, NULL, "通道数", "单通道\n"
                                                "立体声",
                       &dd);
    lv_obj_add_event_cb(dd, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)4);

    section = lv_menu_section_create(sub_wireless_page);
    ui_create_dropdown(section, NULL, "传输协议", "BLE\n"
                                                  "UDP\n"
                                                  "TCP",
                       &dd);
    lv_obj_add_event_cb(dd, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)5);

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
    lv_obj_add_style(root_page, &scroll_style, LV_PART_SCROLLBAR);
    // lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), LV_PART_MAIN), 0);
    section = lv_menu_section_create(root_page);
    // cont = create_text(section, NULL, "Mechanics", LV_MENU_ITEM_BUILDER_VARIANT_1);
    // lv_group_add_obj(lv_group_get_default(), cont);
    // lv_menu_set_load_page_event(menu, cont, sub_mechanics_page);
    section = lv_menu_section_create(root_page);
    cont = ui_create_text(section, LV_SYMBOL_AUDIO, "音频设置", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_audio_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = ui_create_text(section, LV_SYMBOL_BLUETOOTH, "设备连接", LV_MENU_ITEM_BUILDER_VARIANT_1); // finish
    lv_group_add_obj(lv_group_get_default(), cont);
    // lv_menu_set_load_page_event(menu, cont, sub_bt_page);
    lv_obj_add_event_cb(cont, relink_bt_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = ui_create_text(section, LV_SYMBOL_WIFI, "无线传输设置", LV_MENU_ITEM_BUILDER_VARIANT_1); // TODO: 传输协议（BLE udp tcp）
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_wireless_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = ui_create_text(section, LV_SYMBOL_USB, "USB传输设置", LV_MENU_ITEM_BUILDER_VARIANT_1); // TODO: 模式（音频传输、读卡器）
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_usb_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);
    section = lv_menu_section_create(root_page);
    cont = ui_create_text(section, &system_info, "系统信息", LV_MENU_ITEM_BUILDER_VARIANT_1, std::nullopt, true);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_system_page); // TODO: AWA  CPU核1、2占用 运行内存（IRAM PSRAM） tf储存
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);

    section = lv_menu_section_create(root_page);
    cont = ui_create_text(section, &about, "关于", LV_MENU_ITEM_BUILDER_VARIANT_1, std::nullopt, true); // TODO: 大标题 固件版本 作者 链接
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, NULL);

    // lv_menu_set_sidebar_page(menu, root_page);

    // lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
    //                   NULL);
    lv_menu_set_page(menu, root_page);
    sys_info_timer = lv_timer_create(sys_info_timer_cb, SYSTEM_INFO_REFLUSH_TIME, NULL);
}

// 设置页面back按钮回调
static void back_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (lv_menu_back_button_is_root(menu, obj))
    {
        ui_set_hidden_main_widget(false);
        lv_timer_delete(sys_info_timer);
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
    ui_free_main_widget();
    ui_bt_init();
}

static lv_obj_t *ui_create_text(lv_obj_t *parent, const void *icon, const char *txt,
                                lv_menu_builder_variant_t builder_variant, std::optional<lv_obj_t **> label_o, std::optional<bool> is_from_svg)
{
    lv_obj_t *obj = lv_menu_cont_create(parent);
    lv_obj_t *img = NULL;
    lv_obj_t *label = NULL;

    if (icon)
    {
        img = lv_image_create(obj);
        lv_image_set_src(img, icon);
        if (is_from_svg.value_or(false))
        {
            lv_img_set_zoom(img, 32);
            lv_obj_set_size(img, 16, 16);
        }
    }

    if (txt)
    {
        label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        // lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        // lv_obj_set_flex_grow(label, 1);
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
    lv_obj_t *obj = ui_create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_2);

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
static lv_obj_t *ui_create_switch(lv_obj_t *parent, const void *icon, const char *txt, bool chk)
{
    lv_obj_t *obj = ui_create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1);

    lv_obj_t *sw = lv_switch_create(obj);
    lv_obj_add_state(sw, chk ? LV_STATE_CHECKED : LV_STATE_DEFAULT);

    return obj;
}
// 创建下拉列表， options: apple\nbanana...
static lv_obj_t *ui_create_dropdown(lv_obj_t *parent, const void *icon, const char *txt, const char *options, std::optional<lv_obj_t **> dd_o)
{
    lv_obj_t *label;
    lv_obj_t *obj = ui_create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1, &label);

    // 从焦点组中移除标签容器，使其无法被编码器选中
    lv_group_remove_obj(obj);

    lv_obj_t *dd = lv_dropdown_create(obj);
    lv_obj_add_style(dd, &scroll_style, LV_PART_SCROLLBAR);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_size(dd, 85, 20);
    lv_obj_add_flag(dd, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_align_to(dd, label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_group_add_obj(lv_group_get_default(), dd);
    lv_obj_add_event_cb(dd, [](lv_event_t *e)
                        {
        lv_obj_t *dd = lv_event_get_target_obj(e);
        lv_obj_t *container = lv_obj_get_parent(dd); 
        if (container && lv_obj_is_valid(container)) {
            lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
        } }, LV_EVENT_FOCUSED, NULL);

    if (dd_o.has_value())
    {
        *dd_o.value() = dd;
    }
    return obj;
}
static lv_obj_t *ui_create_sub_page(lv_obj_t *parent, const char *title, bool display_scroll)
{
    lv_obj_t *sub_page = lv_menu_page_create(parent, title);
    // lv_obj_set_style_pad_hor(sub_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(parent), LV_PART_MAIN), 0);
    // lv_menu_separator_create(sub_page);
    lv_obj_set_scroll_dir(sub_page, LV_DIR_VER);
    lv_obj_add_style(sub_page, &scroll_style, LV_PART_SCROLLBAR);
    if (display_scroll)
        lv_obj_set_scrollbar_mode(sub_page, LV_SCROLLBAR_MODE_AUTO);
    else
        lv_obj_set_scrollbar_mode(sub_page, LV_SCROLLBAR_MODE_OFF);
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
static void choose_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target_obj(e);
    void *ud = lv_event_get_user_data(e);
    int choice = lv_dropdown_get_selected(dd);
    int sec = (int)ud;
    switch (sec)
    {
    case 1:
        ui_set_transmit_mode(choice);
        break;
    case 2:
        ui_set_sample_freq(choice);
        break;
    case 3:
        ui_set_bit(choice);
        break;
    case 4:
        ui_set_channel(choice);
        break;
    case 5:
        ui_set_transmit_protocol(choice);
        break;
    default:
        break;
    }
}
static void sys_info_timer_cb(lv_timer_t *)
{
    lv_label_set_text_fmt(cpu1, "CPU1   #626367 %d%#", SystemInfo::cpu1);
    lv_label_set_text_fmt(cpu2, "CPU2   #626367 %d%#", SystemInfo::cpu2);
    lv_label_set_text_fmt(iram, "IRAM   #626367 %dbit#", SystemInfo::IRAM);
    lv_label_set_text_fmt(psram, "PSRAM   #626367 %dbit#", SystemInfo::PSRAM);
    lv_label_set_text_fmt(sd, "SD   #626367 %dbit#", SystemInfo::SD);
}

// 传输模式 0:音频转换 1:usb
void ui_set_transmit_mode(const uint32_t &choice)
{
}
// 采样率 0:48000 1:96000 2:192000
void ui_set_sample_freq(const uint32_t &choice)
{
}
// 位深度 0:16 1:24 2:32
void ui_set_bit(const uint32_t &choice)
{
}
// 通道数 0:单通道 1:立体声
void ui_set_channel(const uint32_t &choice)
{
}
// 传输协议 0:BLE 1:UDP 2:TCP
void ui_set_transmit_protocol(const uint32_t &choice)
{
}