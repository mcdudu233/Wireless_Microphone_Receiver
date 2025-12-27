#include "ui/ui.h"
#include "ui/ui_setting.h"

#include <string>

static lv_obj_t *menu;
static lv_obj_t *setting_widget;
static lv_obj_t *last_enter_btn = NULL; // 记录进入子页面所用的条目
static lv_timer_t *sys_info_timer;
static lv_style_t scroll_style;

static bool enter_bottom = false;
static lv_obj_t *root_page_ref = NULL;

static lv_obj_t *dd_audio_bit;
static lv_obj_t *dd_audio_channel;
static lv_obj_t *dd_audio_freq;
static lv_obj_t *dd_audio_volumn_mode;
static lv_obj_t *slider_audio_volumn;

static lv_obj_t *dd_wireless_mode;

static lv_obj_t *dd_usb_mode;

static lv_obj_t *cpu1;
static lv_obj_t *cpu2;
static lv_obj_t *iram;
static lv_obj_t *psram;
static lv_obj_t *sd;

static void get_config();
static void back_cb(lv_event_t *e);          // 设置页面back按钮回调
static void relink_bt_cb(lv_event_t *e);     // 重新链接蓝牙回调
static void enter_subpage_cb(lv_event_t *e); // 记录进入子页的来源条目
static void focus_async_cb(void *obj_p);     // 异步将焦点移回来源条目
static void choose_cb(lv_event_t *e);        // 下拉菜单选中回调
static void sys_info_timer_cb(lv_timer_t *); // 系统信息更新timer
static void back_btn_focus_cb(lv_event_t *e);
static void scroll_event_cb(lv_event_t *e);

static lv_obj_t *ui_create_text(lv_obj_t *parent, const void *icon, const char *txt,
                                lv_menu_builder_variant_t builder_variant, lv_obj_t **label_o = nullptr, bool is_from_svg = false);
static lv_obj_t *ui_create_slider(lv_obj_t *parent, const void *icon, const char *txt, int32_t min, int32_t max,
                                  int32_t val, lv_obj_t **slider_obj = nullptr);
static lv_obj_t *ui_create_dropdown(lv_obj_t *parent, const void *icon, const char *txt, const char *options, lv_obj_t **dd_o = nullptr);
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
    lv_obj_set_style_bg_color(menu, lv_color_hex(0xF5F7FA), 0);
    lv_menu_set_mode_root_back_button(menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);

    lv_obj_add_event_cb(menu, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(menu, lv_display_get_horizontal_resolution(NULL) - 5, lv_display_get_vertical_resolution(NULL) - 5);
    lv_obj_center(menu);

    // 获取返回按钮并应用焦点样式
    lv_obj_t *main_header = lv_menu_get_main_header(menu);
    if (main_header)
    {
        lv_obj_t *back_btn = lv_obj_get_child_by_type(main_header, 0, &lv_button_class);
        if (back_btn)
        {
            // 去除按钮本身的焦点边框
            lv_obj_set_style_outline_width(back_btn, 0, LV_STATE_FOCUSED);
            lv_obj_set_style_outline_width(back_btn, 0, LV_STATE_FOCUS_KEY);
            // 添加焦点事件回调来改变图标颜色
            lv_obj_add_event_cb(back_btn, back_btn_focus_cb, LV_EVENT_FOCUSED, NULL);
            lv_obj_add_event_cb(back_btn, back_btn_focus_cb, LV_EVENT_DEFOCUSED, NULL);
        }
    }

    lv_obj_t *cont;

    /*Create sub pages*/
    lv_obj_t *sub_about_page = ui_create_sub_page(menu, "关于", false);
    lv_obj_t *sub_usb_page = ui_create_sub_page(menu, "USB传输设置");
    lv_obj_t *sub_system_page = ui_create_sub_page(menu, "系统信息");
    lv_obj_t *sub_audio_page = ui_create_sub_page(menu, "音频设置");
    lv_obj_t *sub_wireless_page = ui_create_sub_page(menu, "无线连接设置");

    // 关于
    // section = lv_menu_section_create(sub_about_page);
    // 放入一个可滚动容器，使其显示滚动条，并可被编码器聚焦控制
    lv_obj_t *scroller = lv_obj_create(sub_about_page);
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
    lv_span_set_text_fmt(span, "固件版本: v%u.%u\n作者: %s\n外部链接: %s",
                         CONFIG_VERSION_VALUE >> 8, CONFIG_VERSION_VALUE & 0xFF, AUTHOR, WEBSITE);
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

    // section = lv_menu_section_create(sub_system_page); // CPU核1、2占用 运行内存（IRAM PSRAM） tf储存
    ui_create_text(sub_system_page, NULL, "CPU 1", LV_MENU_ITEM_BUILDER_VARIANT_1, &cpu1);
    ui_create_text(sub_system_page, NULL, "CPU 2", LV_MENU_ITEM_BUILDER_VARIANT_1, &cpu2);
    ui_create_text(sub_system_page, NULL, "IRAM", LV_MENU_ITEM_BUILDER_VARIANT_1, &iram);
    ui_create_text(sub_system_page, NULL, "PSRAM", LV_MENU_ITEM_BUILDER_VARIANT_1, &psram);
    ui_create_text(sub_system_page, NULL, "外置存储", LV_MENU_ITEM_BUILDER_VARIANT_1, &sd);

    lv_label_set_recolor(cpu1, true);
    lv_label_set_recolor(cpu2, true);
    lv_label_set_recolor(iram, true);
    lv_label_set_recolor(psram, true);
    lv_label_set_recolor(sd, true);

    // section = lv_menu_section_create(sub_usb_page);
    ui_create_dropdown(sub_usb_page, NULL, "传输模式", "默认"
                                                       "音频\n"
                                                       "读卡器\n"
                                                       "JTAG",

                       &dd_usb_mode);
    lv_obj_add_event_cb(dd_usb_mode, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)1);
    // 音频 频率 比特 通道 下拉菜单
    // 48000 96000 192000 Hz
    // 16 24 32 bit
    // 单声道 立体
    // section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(sub_audio_page, NULL, "采样率", "48000Hz\n"
                                                       "96000Hz\n"
                                                       "192000Hz",
                       &dd_audio_freq);
    lv_obj_add_event_cb(dd_audio_freq, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)2);
    // section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(sub_audio_page, NULL, "位深度", "16bit\n"
                                                       "24bit\n"
                                                       "32bit",
                       &dd_audio_bit);
    lv_obj_add_event_cb(dd_audio_bit, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)3);
    // section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(sub_audio_page, NULL, "通道数", "单通道\n"
                                                       "立体声",
                       &dd_audio_channel);
    lv_obj_add_event_cb(dd_audio_channel, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)4);
    ui_create_dropdown(sub_audio_page, NULL, "增益模式", "自动增益\n"
                                                         "峰值减少\n"
                                                         "手动",
                       &dd_audio_volumn_mode);
    lv_obj_add_event_cb(dd_audio_volumn_mode, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)5);
    // TODO:缺少事件
    ui_create_slider(sub_audio_page, NULL, "增益", 0, 60, 0);

    // section = lv_menu_section_create(sub_wireless_page);
    ui_create_dropdown(sub_wireless_page, NULL, "传输协议", "BLE\n"
                                                            "WIFI\n",
                       &dd_wireless_mode);
    lv_obj_add_event_cb(dd_wireless_mode, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)6);

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
    root_page_ref = root_page; // 保存引用
    lv_obj_add_style(root_page, &scroll_style, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(root_page, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    // lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), LV_PART_MAIN), 0);
    // section = lv_menu_section_create(root_page);
    // cont = create_text(section, NULL, "Mechanics", LV_MENU_ITEM_BUILDER_VARIANT_1);
    // lv_group_add_obj(lv_group_get_default(), cont);
    // lv_menu_set_load_page_event(menu, cont, sub_mechanics_page);
    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_audio, "音频设置", LV_MENU_ITEM_BUILDER_VARIANT_1, nullptr, true);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_audio_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_audio_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_bt, "设备连接", LV_MENU_ITEM_BUILDER_VARIANT_1, nullptr, true); // finish
    lv_group_add_obj(lv_group_get_default(), cont);
    // lv_menu_set_load_page_event(menu, cont, sub_bt_page);
    lv_obj_add_event_cb(cont, relink_bt_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_wifi, "无线传输设置", LV_MENU_ITEM_BUILDER_VARIANT_1, nullptr, true);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_wireless_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_wireless_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_usb, "USB传输设置", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_usb_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_usb_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_system_info, "系统信息", LV_MENU_ITEM_BUILDER_VARIANT_1, nullptr, true);
    lv_group_add_obj(lv_group_get_default(), cont);
    lv_menu_set_load_page_event(menu, cont, sub_system_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_system_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_about, "关于", LV_MENU_ITEM_BUILDER_VARIANT_1, nullptr, true);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_about_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // lv_menu_set_sidebar_page(menu, root_page);

    // lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
    //                   NULL);
    lv_menu_set_page(menu, root_page);
    lv_obj_set_scroll_dir(root_page, LV_DIR_VER);
    lv_obj_send_event(root_page, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(root_page, 0), LV_ANIM_OFF);

    sys_info_timer = lv_timer_create(sys_info_timer_cb, SYSTEM_INFO_REFLUSH_TIME, NULL);
    // 获取数据
    get_config();
}

static void get_config()
{
    uint32_t i;
    // 通道
    switch (config::config.audio.channel)
    {
    case 1:
        i = 0;
        break;
    case 2:
        i = 1;
        break;
    default:
        i = 1;
        break;
    }
    lv_dropdown_set_selected(dd_audio_channel, i);

    // 位深度
    switch (config::config.audio.bit)
    {
    case 16:
        i = 0;
        break;
    case 24:
        i = 1;
        break;
    case 32:
        i = 2;
        break;
    default:
        i = 0;
        break;
    }
    lv_dropdown_set_selected(dd_audio_bit, i);

    // 采样率
    switch (config::config.audio.rate)
    {
    case 48000:
        i = 0;
        break;
    case 96000:
        i = 1;
        break;
    case 192000:
        i = 2;
        break;
    default:
        i = 0;
        break;
    }
    lv_dropdown_set_selected(dd_audio_freq, i);

    // usb传输设置
    switch (config::config.usb.mode)
    {
    case USB_MODE_NONE:
        i = 0;
        break;
    case USB_MODE_AUDIO:
        i = 1;
        break;
    case USB_MODE_SD:
        i = 2;
        break;
    case USB_MODE_JTAG:
        i = 3;
        break;
    default:
        i = 1;
        break;
    }
    lv_dropdown_set_selected(dd_usb_mode, i);

    // 增益
    lv_slider_set_value(slider_audio_volumn, config::config.audio.volumn, LV_ANIM_OFF);

    // 无限传输协议
    if (config::config.rf.mode)
        lv_dropdown_set_selected(dd_wireless_mode, 1);
    else
        lv_dropdown_set_selected(dd_wireless_mode, 0);
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
        ui_popwin_msgbox("是否立即生效", nullptr, nullptr, LV_SYMBOL_BELL, "请选择:", false, "是", [](lv_event_t *e)
                         { ui_update_config(); }, "否");
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
                                lv_menu_builder_variant_t builder_variant, lv_obj_t **label_o, bool is_from_svg)
{
    lv_obj_t *obj = lv_menu_cont_create(parent);
    lv_obj_t *img = NULL;
    lv_obj_t *label = NULL;

    // 菜单项样式
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), 0);                // 默认背景
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xF0F0F0), LV_STATE_PRESSED); // 按下背景
    // lv_obj_set_style_bg_color(obj, lv_color_hex(0xE8E8E8), LV_STATE_FOCUS_KEY); // 焦点背景
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_pad_all(obj, 5, 0);
    lv_obj_set_style_margin_bottom(obj, 2, 0); // 增加间距
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 10, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 2, 0);

    if (icon)
    {
        img = lv_image_create(obj);
        lv_image_set_src(img, icon);
        if (is_from_svg)
        {
            lv_img_set_zoom(img, 32);
            lv_obj_set_size(img, 16, 16);
        }
    }

    if (txt)
    {
        label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_obj_set_style_text_color(label, lv_color_hex(0x333333), 0);
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
    if (label_o)
    {
        *label_o = label;
    }
    return obj;
}
static lv_obj_t *ui_create_slider(lv_obj_t *parent, const void *icon, const char *txt, int32_t min, int32_t max,
                                  int32_t val, lv_obj_t **slider_obj)
{
    lv_obj_t *title;
    lv_obj_t *obj = ui_create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1, &title);
    lv_group_remove_obj(obj);
    lv_obj_t *slider = lv_slider_create(obj);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
    lv_obj_align_to(slider, title, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_size(slider, lv_pct(73), 20);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);

    lv_obj_t *pct = lv_label_create(slider);
    lv_label_set_text_fmt(pct, "%d%%", 0);
    lv_obj_set_style_text_color(pct, lv_color_hex(0x333333), 0);
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(slider, [](lv_event_t *e)
                        {
        lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* pct = (lv_obj_t*) lv_event_get_user_data(e);
        lv_label_set_text_fmt(pct,"%d",lv_slider_get_value(slider)); }, LV_EVENT_VALUE_CHANGED, pct);
    lv_obj_add_event_cb(slider, [](lv_event_t *e)
                        {
        lv_obj_t* target = lv_event_get_target_obj(e);
        lv_obj_t* parent = lv_obj_get_parent(target);
        lv_obj_scroll_to_view_recursive(parent,LV_ANIM_ON); }, LV_EVENT_FOCUSED, NULL);
    if (slider_obj)
    {
        *slider_obj = slider;
    }

    return obj;
}
// 创建下拉列表， options: apple\nbanana...
static lv_obj_t *ui_create_dropdown(lv_obj_t *parent, const void *icon, const char *txt, const char *options, lv_obj_t **dd_o)
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

    lv_obj_t *list = lv_dropdown_get_list(dd);
    for (int i = 0; i < lv_obj_get_child_count_by_type(list, &lv_button_class); i++)
    {
        lv_obj_t *child = lv_obj_get_child_by_type(list, i, &lv_button_class);
        lv_obj_add_event_cb(child, [](lv_event_t *e)
                            {
            lv_obj_t* target = (lv_obj_t*)lv_event_get_target_obj(e);
            lv_obj_scroll_to_view_recursive(target,LV_ANIM_ON); }, LV_EVENT_FOCUSED, NULL);
    }

    lv_obj_add_event_cb(dd, [](lv_event_t *e)
                        {
        lv_obj_t *dd = lv_event_get_target_obj(e);
        lv_obj_t *container = lv_obj_get_parent(dd); 
        if (container && lv_obj_is_valid(container)) {
            lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
        } }, LV_EVENT_FOCUSED, NULL);

    if (dd_o)
    {
        *dd_o = dd;
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
    lv_obj_add_event_cb(sub_page, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    return sub_page;
}

// 记录进入子页的来源条目
static void enter_subpage_cb(lv_event_t *e)
{
    last_enter_btn = lv_event_get_target_obj(e);
    lv_obj_t *page = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_send_event(page, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(page, 0), LV_ANIM_OFF);
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
        switch (choice)
        {
        case 0:
            config::config.usb.mode = USB_MODE_NONE;
            break;
        case 1:
            config::config.usb.mode = USB_MODE_AUDIO;
            break;
        case 2:
            config::config.usb.mode = USB_MODE_SD;
            break;
        case 3:
            config::config.usb.mode = USB_MODE_JTAG;
            break;
        default:
            config::config.usb.mode = USB_MODE_NONE;
            break;
        }
        break;
    case 2:
        switch (choice)
        {
        case 0:
            config::config.audio.rate = 48000;
            break;
        case 1:
            config::config.audio.rate = 96000;
            break;
        case 2:
            config::config.audio.rate = 192000;
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (choice)
        {
        case 0:
            config::config.audio.bit = 16;
            break;
        case 1:
            config::config.audio.bit = 24;
            break;
        case 2:
            config::config.audio.bit = 32;
            break;
        default:
            break;
        }
        break;
    case 4:
        switch (choice)
        {
        case 0:
            config::config.audio.channel = 1;
            break;
        case 1:
            config::config.audio.channel = 2;
            break;
        default:
            break;
        }
        break;
    case 5:
    // TODO: 增益需要修改
    //  config::config.audio.autoVolumn
    case 6:
        config::config.rf.mode = choice;
    default:
        break;
    }
    config::save();
}
static void sys_info_timer_cb(lv_timer_t *)
{
    lv_label_set_text_fmt(cpu1, "CPU1   #626367 %d%#", SystemInfo::cpu1);
    lv_label_set_text_fmt(cpu2, "CPU2   #626367 %d%#", SystemInfo::cpu2);
    lv_label_set_text_fmt(iram, "IRAM   #626367 %dbit#", SystemInfo::IRAM);
    lv_label_set_text_fmt(psram, "PSRAM   #626367 %dbit#", SystemInfo::PSRAM);
    lv_label_set_text_fmt(sd, "SD   #626367 %dbit#", SystemInfo::SD);
}
// 返回按钮焦点事件回调
static void back_btn_focus_cb(lv_event_t *e)
{
    lv_obj_t *back_icon = lv_obj_get_child(lv_event_get_target_obj(e), 0);
    if (!back_icon)
        return;
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED)
        lv_obj_set_style_text_color(back_icon, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    else
        lv_obj_set_style_text_color(back_icon, lv_color_black(), LV_PART_MAIN);
}
static void scroll_event_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target_obj(e);

    // 只对root_page应用滑入动画
    if (cont == root_page_ref)
    {
        lv_area_t cont_a;
        lv_obj_get_coords(cont, &cont_a);
        int32_t cont_bottom = cont_a.y2;
        int32_t cont_top = cont_a.y1;

        int32_t child_cnt = (int32_t)lv_obj_get_child_count(cont);
        for (int32_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(cont, i);
            lv_area_t child_a;
            lv_obj_get_coords(child, &child_a);

            // 当子项离开可视区域时，重置状态
            if (child_a.y1 > cont_bottom || child_a.y2 < cont_top)
            {
                lv_obj_set_user_data(child, (void *)0);
                lv_obj_set_style_translate_x(child, 100, 0);
                lv_obj_set_style_opa(child, LV_OPA_TRANSP, 0);
            }
            // 当子项进入可视区域时触发动画
            else if (child_a.y1 <= cont_bottom && child_a.y2 >= cont_top)
            {
                // 检查是否已播放过动画
                if (lv_obj_get_user_data(child) == (void *)1)
                    continue;

                lv_obj_set_user_data(child, (void *)1);

                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, child);
                lv_anim_set_values(&a, 100, 0);
                lv_anim_set_duration(&a, 300);
                lv_anim_set_exec_cb(&a, [](void *var, int32_t v)
                                    { lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0); });
                lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
                lv_anim_start(&a);

                // 透明度动画
                lv_anim_set_exec_cb(&a, [](void *var, int32_t v)
                                    { lv_obj_set_style_opa((lv_obj_t *)var, v, 0); });
                lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
                lv_anim_start(&a);
            }
        }
    }
    else
    {
        // 其他页面保持原有的滚轮效果
        lv_area_t cont_a;
        lv_obj_get_coords(cont, &cont_a);
        int32_t cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;

        int32_t r = lv_obj_get_height(cont) * 7 / 10;
        int32_t i;
        int32_t child_cnt = (int32_t)lv_obj_get_child_count(cont);
        for (i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(cont, i);
            lv_area_t child_a;
            lv_obj_get_coords(child, &child_a);

            int32_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;

            int32_t diff_y = child_y_center - cont_y_center;
            diff_y = LV_ABS(diff_y);

            /*Get the x of diff_y on a circle.*/
            int32_t x;
            /*If diff_y is out of the circle use the last point of the circle (the radius)*/
            if (diff_y >= r)
            {
                x = r;
            }
            else
            {
                /*Use Pythagoras theorem to get x from radius and y*/
                uint32_t x_sqr = r * r - diff_y * diff_y;
                lv_sqrt_res_t res;
                lv_sqrt(x_sqr, &res, 0x8000); /*Use lvgl's built in sqrt root function*/
                x = r - res.i;
            }

            /*Translate the item by the calculated X coordinate*/
            lv_obj_set_style_translate_x(child, x, 0);

            /*Use some opacity with larger translations*/
            lv_opa_t opa = (lv_opa_t)lv_map(x, 0, r, LV_OPA_TRANSP, LV_OPA_COVER);
            lv_obj_set_style_opa(child, LV_OPA_COVER - opa, 0);
        }
    }
}
// 传输模式 0:音频转换 1:usb
void ui_update_config()
{
}