#include "ui/ui.h"
#include "ui/ui_setting.h"
#include "module/tf.h"

#include <cstdio>
#include <cstring>

static lv_obj_t *last_widget;

static lv_obj_t *menu;
static lv_obj_t *setting_widget;
static lv_obj_t *last_enter_btn = NULL; // 记录进入子页面所用的条目
static lv_timer_t *sys_info_timer;
static lv_style_t scroll_style;
static bool scroll_style_initialized;
static lv_group_t *setting_group;
static lv_group_t *last_group;
static lv_obj_t *main_back_btn;

static lv_obj_t *file_list;
static lv_obj_t *file_path;
static lv_timer_t *file_timer;
static char current_file_path[TF_PATH_MAX] = "/";
static tf::FileEntry file_entries[TF_FILE_LIST_MAX];
static char selected_file_path[TF_PATH_MAX];
static size_t file_entry_count = 0;
static bool file_list_truncated = false;
static bool file_operation_pending = false;
static uint32_t file_request_id = 0;

static bool enter_bottom = false;
static lv_obj_t *root_page_ref = NULL;

static lv_obj_t *dd_audio_bit;
static lv_obj_t *dd_audio_channel;
static lv_obj_t *dd_audio_rate;
static lv_obj_t *dd_audio_audio_mode;
static lv_obj_t *slider_audio_gain;

static lv_obj_t *dd_rf_mode;

static lv_obj_t *dd_usb_mode;

static lv_obj_t *cpu1;
static lv_obj_t *cpu2;
static lv_obj_t *iram;
static lv_obj_t *psram;
// static lv_obj_t *sd;

static void save_config(uint8_t &p, bool now);
static void back_cb(lv_event_t *e);          // 设置页面back按钮回调
static void relink_bt_cb(lv_event_t *e);     // 重新链接蓝牙回调
static void enter_subpage_cb(lv_event_t *e); // 记录进入子页的来源条目
static void focus_async_cb(void *obj_p);     // 异步将焦点移回来源条目
static void sys_info_timer_cb(lv_timer_t *); // 系统信息更新timer
static void back_btn_focus_cb(lv_event_t *e);
static void scroll_event_cb(lv_event_t *e);
static void request_file_page();
static void render_file_page(bool success);
static void file_timer_cb(lv_timer_t *);
static void file_row_cb(lv_event_t *e);
static void file_delete_cb(lv_event_t *e);
static void file_cancel_cb(lv_event_t *e);
static void file_refresh_cb(lv_event_t *e);
static void file_parent_cb(lv_event_t *e);
static bool join_file_path(const char *name, char *path, size_t path_size);
static void set_settings_focus(uint8_t page);
static void delete_settings_group();

static lv_obj_t *ui_create_text(lv_obj_t *parent, const void *icon, const char *txt, lv_obj_t **label_o = nullptr, bool is_from_svg = false);
static lv_obj_t *ui_create_slider(lv_obj_t *parent, const void *icon, const char *txt, int32_t min, int32_t max,
                                  int32_t val, lv_obj_t **slider_obj = nullptr);
static lv_obj_t *ui_create_dropdown(lv_obj_t *parent, const void *icon, const char *txt, const char *options, lv_obj_t **dd_o = nullptr);
static lv_obj_t *ui_create_sub_page(lv_obj_t *parent, const char *title, bool display_scroll = true); // 新建子页面

enum e_page
{
    usb_page = 1,
    audio_page = 2,
    rf_page = 3,
    about_page = 4,
    system_page = 5,
    file_page = 6
};

void ui_setting_init(lv_obj_t *ui_from)
{
    lv_obj_t *btn;
    char *from_data;

    if (!scroll_style_initialized)
    {
        lv_style_init(&scroll_style);
        lv_style_set_pad_right(&scroll_style, 0);
        lv_style_set_width(&scroll_style, 2);
        lv_style_set_bg_color(&scroll_style, lv_color_hex(0x2D6BDB));
        lv_style_set_bg_opa(&scroll_style, LV_OPA_COVER);
        scroll_style_initialized = true;
    }
    last_widget = ui_from;
    from_data = (char *)lv_obj_get_user_data(last_widget);
    lv_obj_set_flag(last_widget, LV_OBJ_FLAG_HIDDEN, true);

    last_group = lv_group_get_default();
    setting_group = lv_group_create();
    lv_group_set_default(setting_group);
    ui_bind_group_to_all_encoders(setting_group);
    setting_widget = ui_add_win();
    menu = lv_menu_create(setting_widget);
    lv_obj_set_style_text_font(menu, UI_FONT_BODY, 0);
    lv_obj_set_style_bg_color(menu, lv_color_hex(0xF5F7FA), 0);
    lv_menu_set_mode_root_back_button(menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);

    lv_obj_add_event_cb(menu, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(menu, lv_display_get_horizontal_resolution(NULL) - 5, lv_display_get_vertical_resolution(NULL) - 5);
    lv_obj_center(menu);

    // 获取返回按钮并应用焦点样式
    lv_obj_t *main_header = lv_menu_get_main_header(menu);
    if (main_header)
    {
        lv_obj_set_style_bg_color(main_header, lv_color_hex(0xFFFFFF), 0);
        main_back_btn = lv_obj_get_child_by_type(main_header, 0, &lv_button_class);
        if (main_back_btn)
        {
            // 去除按钮本身的焦点边框
            lv_obj_set_style_outline_width(main_back_btn, 0, LV_STATE_FOCUSED);
            lv_obj_set_style_outline_width(main_back_btn, 0, LV_STATE_FOCUS_KEY);
            lv_obj_set_style_bg_color(main_back_btn, lv_color_hex(0xDBEAFE), LV_STATE_FOCUSED);
            lv_obj_set_style_bg_opa(main_back_btn, LV_OPA_COVER, LV_STATE_FOCUSED);
            lv_obj_set_style_radius(main_back_btn, 3, LV_STATE_FOCUSED);
            // 添加焦点事件回调来改变图标颜色
            lv_obj_add_event_cb(main_back_btn, back_btn_focus_cb, LV_EVENT_FOCUSED, NULL);
            lv_obj_add_event_cb(main_back_btn, back_btn_focus_cb, LV_EVENT_DEFOCUSED, NULL);
        }
    }

    lv_obj_t *cont;

    /*Create sub pages*/
    lv_obj_t *sub_about_page = ui_create_sub_page(menu, "关于", false);
    lv_obj_set_user_data(sub_about_page, (void *)e_page::about_page);
    lv_obj_t *sub_usb_page = ui_create_sub_page(menu, "USB传输设置");
    lv_obj_set_user_data(sub_usb_page, (void *)e_page::usb_page);
    lv_obj_t *sub_file_page = ui_create_sub_page(menu, "文件系统管理");
    lv_obj_set_user_data(sub_file_page, (void *)e_page::file_page);
    lv_obj_t *sub_system_page = ui_create_sub_page(menu, "系统信息");
    lv_obj_set_user_data(sub_system_page, (void *)e_page::system_page);
    lv_obj_t *sub_audio_page = ui_create_sub_page(menu, "音频设置");
    lv_obj_set_user_data(sub_audio_page, (void *)e_page::audio_page);
    lv_obj_t *sub_rf_page = ui_create_sub_page(menu, "无线连接设置");
    lv_obj_set_user_data(sub_rf_page, (void *)e_page::rf_page);

    // 关于
    lv_obj_t *label_title = lv_label_create(sub_about_page);
    lv_label_set_text_fmt(label_title, "无线麦克风 V%u.%u",
                          CONFIG_VERSION_VALUE >> 8, CONFIG_VERSION_VALUE & 0xFF);
    lv_obj_set_width(label_title, lv_pct(100));
    lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x626367), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_harmonyos_14, 0);

    lv_obj_t *label_author = lv_label_create(sub_about_page);
    lv_label_set_text_fmt(label_author, "作者:%s", AUTHOR);
    lv_obj_set_width(label_author, lv_pct(100));
    lv_obj_set_style_text_align(label_author, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_author, lv_color_hex(0x626367), 0);
    lv_obj_set_style_text_font(label_author, &lv_font_harmonyos_12, 0);

    lv_obj_t *label_link = lv_label_create(sub_about_page);
    lv_label_set_text(label_link, WEBSITE);
    lv_obj_set_width(label_link, lv_pct(100));
    lv_obj_set_style_text_align(label_link, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_link, lv_color_hex(0x626367), 0);
    lv_obj_set_style_text_font(label_link, &lv_font_harmonyos_12, 0);

    lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align_to(label_author, label_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    lv_obj_align_to(label_link, label_author, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    // 系统信息
    lv_obj_t *system_row = ui_create_text(sub_system_page, NULL, "CPU 1", &cpu1);
    lv_obj_set_style_margin_bottom(system_row, 6, 0);
    system_row = ui_create_text(sub_system_page, NULL, "CPU 2", &cpu2);
    lv_obj_set_style_margin_bottom(system_row, 6, 0);
    system_row = ui_create_text(sub_system_page, NULL, "IRAM", &iram);
    lv_obj_set_style_margin_bottom(system_row, 6, 0);
    system_row = ui_create_text(sub_system_page, NULL, "PSRAM", &psram);
    lv_obj_set_style_margin_bottom(system_row, 6, 0);
    // ui_create_text(sub_system_page, NULL, "外置存储", &sd);

    lv_label_set_recolor(cpu1, true);
    lv_label_set_recolor(cpu2, true);
    lv_label_set_recolor(iram, true);
    lv_label_set_recolor(psram, true);
    // lv_label_set_recolor(sd, true);

    // section = lv_menu_section_create(sub_usb_page);
    ui_create_dropdown(sub_usb_page, NULL, "传输模式", "默认\n"
                                                       "音频\n"
                                                       "读卡器\n"
                                                       "JTAG",

                       &dd_usb_mode);
    // lv_obj_add_event_cb(dd_usb_mode, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)1);
    // 音频 频率 比特 通道 下拉菜单
    // 48000 96000 192000 Hz
    // 16 24 32 bit
    // 单声道 立体
    // section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(sub_audio_page, NULL, "采样率", "48000Hz\n"
                                                       "96000Hz\n"
                                                       "192000Hz",
                       &dd_audio_rate);
    // lv_obj_add_event_cb(dd_audio_rate, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)2);
    // section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(sub_audio_page, NULL, "位深度", "16bit\n"
                                                       "24bit\n"
                                                       "32bit",
                       &dd_audio_bit);
    // lv_obj_add_event_cb(dd_audio_bit, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)3);
    // section = lv_menu_section_create(sub_audio_page);
    ui_create_dropdown(sub_audio_page, NULL, "通道数", "单通道\n"
                                                       "立体声",
                       &dd_audio_channel);
    // lv_obj_add_event_cb(dd_audio_channel, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)4);
    ui_create_dropdown(sub_audio_page, NULL, "增益模式", "自动增益\n"
                                                         "峰值减少\n"
                                                         "手动",
                       &dd_audio_audio_mode);
    ui_create_slider(sub_audio_page, NULL, "增益", 0, 60, 0, &slider_audio_gain);

    ui_create_dropdown(sub_rf_page, NULL, "传输协议", "BLE\n"
                                                      "WIFI",
                       &dd_rf_mode);
    // lv_obj_add_event_cb(dd_rf_mode, &choose_cb, LV_EVENT_VALUE_CHANGED, (void *)6);

    lv_obj_t *file_widget = lv_obj_create(sub_file_page);
    lv_obj_set_size(file_widget, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(file_widget, 0, 0);
    lv_obj_set_style_border_width(file_widget, 0, 0);
    lv_obj_set_style_bg_opa(file_widget, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(file_widget, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(file_widget, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(file_widget, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(sub_file_page, LV_DIR_NONE);
    file_path = lv_label_create(file_widget);
    lv_label_set_long_mode(file_path, LV_LABEL_LONG_DOT);
    lv_obj_set_width(file_path, lv_pct(100));
    lv_obj_set_height(file_path, lv_font_get_line_height(UI_FONT_BODY));
    lv_label_set_text(file_path, "TF:/");
    lv_obj_set_style_text_font(file_path, &lv_font_harmonyos_12, 0);
    file_list = lv_list_create(file_widget);
    lv_obj_set_width(file_list, lv_pct(95));
    lv_obj_set_flex_grow(file_list, 1);

    lv_obj_t *root_page = lv_menu_page_create(menu, "设置");
    root_page_ref = root_page; // 保存引用
    lv_obj_add_style(root_page, &scroll_style, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(root_page, scroll_event_cb, LV_EVENT_SCROLL, NULL);

    cont = ui_create_text(root_page, &ui_img_audio, "音频设置", nullptr, true);
    lv_menu_set_load_page_event(menu, cont, sub_audio_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_audio_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    if (!(from_data && lv_strcmp(from_data, "bt_list") == 0)) // 判断是否为蓝牙连接界面过来的
    {
        cont = ui_create_text(root_page, &ui_img_bt, "设备连接", nullptr, true); // finish
        // lv_menu_set_load_page_event(menu, cont, sub_bt_page);
        lv_obj_add_event_cb(cont, relink_bt_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_translate_x(cont, 100, 0);
        lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画
    }
    cont = ui_create_text(root_page, &ui_img_wifi, "无线传输设置", nullptr, true);
    lv_menu_set_load_page_event(menu, cont, sub_rf_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_rf_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_usb, "USB传输设置");
    lv_menu_set_load_page_event(menu, cont, sub_usb_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_usb_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // 文件系统管理
    cont = ui_create_text(root_page, &ui_img_file, "文件系统管理",NULL,true);
    lv_menu_set_load_page_event(menu, cont, sub_file_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_file_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_system_info, "系统信息", nullptr, true);
    lv_menu_set_load_page_event(menu, cont, sub_system_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_system_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    // section = lv_menu_section_create(root_page);
    cont = ui_create_text(root_page, &ui_img_about, "关于", nullptr, true);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);
    lv_obj_add_event_cb(cont, enter_subpage_cb, LV_EVENT_CLICKED, sub_about_page);
    lv_obj_set_style_translate_x(cont, 100, 0);
    lv_obj_set_style_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(cont, (void *)0); // 标记未播放动画

    //  lv_menu_set_sidebar_page(menu, root_page);

    // lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
    //                   NULL);
    lv_menu_set_page(menu, root_page);
    lv_obj_set_scroll_dir(root_page, LV_DIR_VER);
    lv_obj_send_event(root_page, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(root_page, 0), LV_ANIM_OFF);

    sys_info_timer = lv_timer_create(sys_info_timer_cb, SYSTEM_INFO_REFLUSH_TIME, NULL);
    lv_timer_pause(sys_info_timer);
    file_timer = lv_timer_create(file_timer_cb, 50, NULL);
    lv_timer_pause(file_timer);
    set_settings_focus(0);
}

static void add_focus_object(lv_obj_t *obj)
{
    if (obj && lv_obj_is_valid(obj))
        lv_group_add_obj(setting_group, obj);
}

static void set_settings_focus(uint8_t page)
{
    lv_group_remove_all_objs(setting_group);
    add_focus_object(main_back_btn);
    lv_obj_t *first = main_back_btn;

    switch (page)
    {
    case 0:
        for (uint32_t i = 0; i < lv_obj_get_child_count(root_page_ref); ++i)
            add_focus_object(lv_obj_get_child(root_page_ref, i));
        if (lv_obj_get_child_count(root_page_ref) > 0)
            first = lv_obj_get_child(root_page_ref, 0);
        break;
    case e_page::usb_page:
        add_focus_object(dd_usb_mode);
        first = dd_usb_mode;
        break;
    case e_page::audio_page:
        add_focus_object(dd_audio_rate);
        add_focus_object(dd_audio_bit);
        add_focus_object(dd_audio_channel);
        add_focus_object(dd_audio_audio_mode);
        add_focus_object(slider_audio_gain);
        first = dd_audio_rate;
        break;
    case e_page::rf_page:
        add_focus_object(dd_rf_mode);
        first = dd_rf_mode;
        break;
    case e_page::file_page:
        for (uint32_t i = 0; i < lv_obj_get_child_count(file_list); ++i)
            add_focus_object(lv_obj_get_child(file_list, i));
        if (lv_obj_get_child_count(file_list) > 0)
            first = lv_obj_get_child(file_list, 0);
        break;
    default:
        break;
    }

    ui_bind_group_to_all_encoders(setting_group);
    if (first)
        lv_group_focus_obj(first);
}

static void delete_settings_group()
{
    lv_group_set_default(last_group);
    ui_bind_group_to_all_encoders(last_group);
    if (setting_group)
    {
        lv_group_delete(setting_group);
        setting_group = nullptr;
    }
}

// 设置页面back按钮回调
static void back_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (obj && lv_menu_back_button_is_root(menu, obj))
    {
        lv_obj_set_flag(last_widget, LV_OBJ_FLAG_HIDDEN, false);
        lv_obj_send_event(last_widget, (lv_event_code_t)(LV_EVENT_LAST + 1), NULL);
        lv_timer_delete(sys_info_timer);
        sys_info_timer = nullptr;
        lv_timer_delete(file_timer);
        file_timer = nullptr;
        lv_obj_del(setting_widget);
        delete_settings_group();
    }
    else
    {
        // 返回到上一级（主页面），将焦点移回进入该子页的条目
        if (last_enter_btn && lv_obj_is_valid(last_enter_btn))
        {
            set_settings_focus(0);
            // 异步执行，确保菜单已完成页面切换
            lv_obj_t *page_obj = (lv_obj_t *)lv_obj_get_user_data(last_enter_btn);
            uint8_t p = (uint8_t)(intptr_t)lv_obj_get_user_data(page_obj);
            if (p == e_page::system_page)
            {
                lv_timer_pause(sys_info_timer);
            }
            else if (p == e_page::file_page)
            {
                lv_timer_pause(file_timer);
            }
            else if (p == e_page::audio_page || p == e_page::rf_page || p == e_page::usb_page)
            {
                ui_popwin_msgbox("是否立即生效", nullptr, last_enter_btn, LV_SYMBOL_BELL, "请选择:", false, "是", [](lv_event_t *e)
                                 {
                                    uint8_t p = (uint8_t)(intptr_t) lv_event_get_user_data(e);
                                     save_config(p,true);
                                }, (void *)(intptr_t)p, "否", [](lv_event_t *e)
                                 {
                                    uint8_t p = (uint8_t)(intptr_t) lv_event_get_user_data(e);
                                    save_config(p,false); }, (void *)(intptr_t)p);
            }

            lv_async_call(focus_async_cb, last_enter_btn);
        }
    }
}
// 重新连接蓝牙点击
static void relink_bt_cb(lv_event_t *e)
{
    lv_timer_delete(sys_info_timer);
    sys_info_timer = nullptr;
    lv_timer_delete(file_timer);
    file_timer = nullptr;
    lv_obj_del(setting_widget);
    delete_settings_group();
    ui_free_main_widget();
    ui_bt_init();
}

static lv_obj_t *ui_create_text(lv_obj_t *parent, const void *icon, const char *txt, lv_obj_t **label_o, bool is_from_svg)
{
    lv_obj_t *obj = lv_menu_cont_create(parent);
    lv_obj_t *img = NULL;
    lv_obj_t *label = NULL;

    // 菜单项样式
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), 0);                // 默认背景
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xE5E7EB), LV_STATE_PRESSED); // 按下背景
    lv_obj_set_style_radius(obj, 6, 0);
    lv_obj_set_style_pad_all(obj, 5, 0);
    lv_obj_set_style_margin_bottom(obj, UI_SPACE_1, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(obj, lv_color_hex(0x2D6BDB), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(obj, -2, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xDBEAFE), LV_STATE_FOCUSED);

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
        lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_text_font(label, UI_FONT_BODY, 0);
    }

    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_group_add_obj(lv_group_get_default(), obj);

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
    lv_obj_t *obj = ui_create_text(parent, icon, txt, &title);
    lv_group_remove_obj(obj);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_set_height(title, lv_font_get_line_height(UI_FONT_BODY));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_t *slider = lv_slider_create(obj);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
    lv_obj_set_size(slider, 92, 18);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);

    lv_obj_t *pct = lv_label_create(slider);
    lv_label_set_text_fmt(pct, "%ddB", 0);
    lv_obj_set_style_text_color(pct, lv_color_hex(0x1F2937), 0);
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(slider, [](lv_event_t *e)
                        {
        lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* pct = (lv_obj_t*) lv_event_get_user_data(e);
        lv_label_set_text_fmt(pct,"%ddB",lv_slider_get_value(slider)); }, LV_EVENT_VALUE_CHANGED, pct);
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
    lv_obj_t *obj = ui_create_text(parent, icon, txt, &label);

    // 从焦点组中移除标签容器，使其无法被编码器选中
    lv_group_remove_obj(obj);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_height(label, lv_font_get_line_height(UI_FONT_BODY));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

    lv_obj_t *dd = lv_dropdown_create(obj);
    lv_obj_add_style(dd, &scroll_style, LV_PART_SCROLLBAR);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_size(dd, 85, 20);
    lv_obj_set_style_text_font(dd, UI_FONT_BODY, 0);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0xF4F5F7), 0);
    lv_obj_set_style_radius(dd, 4, 0);
    lv_obj_set_style_border_width(dd, 1, 0);
    lv_obj_set_style_border_color(dd, lv_color_hex(0xD1D5DB), 0);
    lv_obj_set_style_outline_width(dd, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(dd, lv_color_hex(0x2D6BDB), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(dd, 1, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0xDBEAFE), LV_STATE_FOCUSED);
    lv_obj_add_flag(dd, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_group_add_obj(lv_group_get_default(), dd);

    lv_obj_t *list = lv_dropdown_get_list(dd);
    lv_obj_set_style_text_font(list, UI_FONT_BODY, 0);
    lv_obj_add_style(list, &scroll_style, LV_PART_SCROLLBAR);

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
    lv_obj_set_style_bg_color(sub_page, lv_color_hex(0xF4F5F7), 0);
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
    uint8_t p = (uint8_t)(intptr_t)lv_obj_get_user_data(page);
    LOGGER_INFO("进入页面");
    switch (p)
    {

    case e_page::system_page:
    {
        lv_timer_resume(sys_info_timer);
        break;
    }
    case e_page::audio_page:
    {
        AudioBit l_bit;
        AudioChannel l_channel;
        AudioGain l_gain;
        AudioMode l_audio_mode;
        AudioRate l_rate;
        ui_setting_audio_page_rcb(l_bit, l_channel, l_rate, l_gain, l_audio_mode);

        int sel_bit = 0;
        switch (l_bit)
        {
        case AUDIO_BIT_16:
            sel_bit = 0;
            break; // 16bit
        case AUDIO_BIT_24:
            sel_bit = 1;
            break;
        case AUDIO_BIT_32:
            sel_bit = 2;
            break;
        }
        lv_dropdown_set_selected(dd_audio_bit, sel_bit);

        int sel_channel = (l_channel == AUDIO_CHANNEL_STEREO) ? 1 : 0;
        lv_dropdown_set_selected(dd_audio_channel, sel_channel);

        int sel_rate = 0;
        switch (l_rate)
        {
        case AUDIO_RATE_48000:
            sel_rate = 0;
            break;
        case AUDIO_RATE_96000:
            sel_rate = 1;
            break;
        case AUDIO_RATE_192000:
            sel_rate = 2;
            break;
        }
        lv_dropdown_set_selected(dd_audio_rate, sel_rate);

        int sel_vol_mode = 0;
        switch (l_audio_mode)
        {
        case AUDIO_MODE_AUTO:
            sel_vol_mode = 0;
            break; // 自动增益
        case AUDIO_MODE_PEEK:
            sel_vol_mode = 1;
            break; // 峰值减少
        case AUDIO_MODE_MANUAL:
            sel_vol_mode = 2;
            break; // 手动
        }
        lv_dropdown_set_selected(dd_audio_audio_mode, sel_vol_mode);

        if (slider_audio_gain)
        {
            lv_slider_set_value(slider_audio_gain, l_gain, LV_ANIM_OFF);
        }
        break;
    }
    case e_page::usb_page:
    {
        USBMode mode = USB_MODE_NONE;
        ui_setting_usb_page_rcb(mode);
        int sel = 0;
        switch (mode)
        {
        case USB_MODE_NONE:
            sel = 0;
            break;
        case USB_MODE_AUDIO:
            sel = 1;
            break;
        case USB_MODE_SD:
            sel = 2;
            break;
        case USB_MODE_JTAG:
            sel = 3;
            break;
        }
        lv_dropdown_set_selected(dd_usb_mode, sel);
        break;
    }
    case e_page::rf_page:
    {
        RFMode rfmode;
        ui_setting_rf_page_rcb(rfmode);
        lv_dropdown_set_selected(dd_rf_mode, rfmode == RF_MODE_WIFI ? 1 : 0);
        break;
    }
    case e_page::file_page:
    {
        LOGGER_DEBUG("进入文件管理系统");
        std::snprintf(current_file_path, sizeof(current_file_path), "/");
        lv_timer_resume(file_timer);
        request_file_page();
        break;
    }
    }

    lv_obj_set_user_data(last_enter_btn, page); // 记录btn对应的page
    set_settings_focus(p);
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

static void sys_info_timer_cb(lv_timer_t *)
{
    float l_cpu1, l_cpu2;
    size_t l_iram, l_psram, l_iram_max, l_psram_max;

    ui_setting_system_page_rcb(l_cpu1, l_cpu2, l_iram, l_psram, l_iram_max, l_psram_max);
    lv_label_set_text_fmt(cpu1, "CPU1: %.2f%%", l_cpu1);
    lv_label_set_text_fmt(cpu2, "CPU2: %.2f%%", l_cpu2);
    lv_label_set_text_fmt(iram, "IRAM: %d/%dKB", l_iram / 1024, l_iram_max / 1024);
    lv_label_set_text_fmt(psram, "PSRAM: %d/%dKB", l_psram / 1024, l_psram_max / 1024);
    // lv_label_set_text_fmt(sd, "SD   #626367 %dbit#", l_sd);
}
// 返回按钮焦点事件回调
static void back_btn_focus_cb(lv_event_t *e)
{
    lv_obj_t *back_icon = lv_obj_get_child(lv_event_get_target_obj(e), 0);
    if (!back_icon)
        return;
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED)
        lv_obj_set_style_text_color(back_icon, lv_color_hex(0x2D6BDB), LV_PART_MAIN);
    else
        lv_obj_set_style_text_color(back_icon, lv_color_hex(0x1F2937), LV_PART_MAIN);
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
            // 子项第一次进入可视区域时触发一次入场动画
            if (child_a.y1 <= cont_bottom && child_a.y2 >= cont_top)
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
}

static void save_config(uint8_t &p, bool now)
{
    switch (p)
    {
    case e_page::usb_page:
        switch (lv_dropdown_get_selected(dd_usb_mode))
        {
        case 0:
            ui_setting_usb_page_scb(USB_MODE_NONE, now);
            break;
        case 1:
            ui_setting_usb_page_scb(USB_MODE_AUDIO, now);
            break;
        case 2:
            ui_setting_usb_page_scb(USB_MODE_SD, now);
            break;
        case 3:
            ui_setting_usb_page_scb(USB_MODE_JTAG, now);
            break;
        }
        break;
    case e_page::audio_page:
        AudioBit v_bit;
        AudioChannel v_channel;
        AudioGain v_gain;
        AudioMode v_audio_mode;
        AudioRate v_rate;

        switch (lv_dropdown_get_selected(dd_audio_bit))
        {
        case 0:
            v_bit = AUDIO_BIT_16;
            break;
        case 1:
            v_bit = AUDIO_BIT_24;
            break;
        case 2:
            v_bit = AUDIO_BIT_32;
            break;
        }
        switch (lv_dropdown_get_selected(dd_audio_channel))
        {
        case 0:
            v_channel = AUDIO_CHANNEL_SINGLE;
            break;
        case 1:
            v_channel = AUDIO_CHANNEL_STEREO;
            break;
        }
        switch (lv_dropdown_get_selected(dd_audio_rate))
        {
        case 0:
            v_rate = AUDIO_RATE_48000;
            break;
        case 1:
            v_rate = AUDIO_RATE_96000;
            break;
        case 2:
            v_rate = AUDIO_RATE_192000;
            break;
        }
        switch (lv_dropdown_get_selected(dd_audio_audio_mode))
        {
        case 0:
            v_audio_mode = AUDIO_MODE_AUTO;
            break;
        case 1:
            v_audio_mode = AUDIO_MODE_PEEK;
            break;
        case 2:
            v_audio_mode = AUDIO_MODE_MANUAL;
            break;
        }

        v_gain = lv_slider_get_value(slider_audio_gain);
        ui_setting_audio_page_scb(v_bit, v_channel, v_rate, v_gain, v_audio_mode, now);
        break;
    case e_page::rf_page:
        ui_setting_rf_page_scb(lv_dropdown_get_selected(dd_rf_mode) == 0 ? RF_MODE_BLE : RF_MODE_WIFI, now);
        break;
    }
}

static void set_file_row_font(lv_obj_t *row)
{
    lv_obj_t *label = lv_obj_get_child(row, 1);
    if (label)
        lv_obj_set_style_text_font(label, &lv_font_harmonyos_12, 0);
}

static lv_obj_t *add_file_row(const char *symbol, const char *text, lv_event_cb_t callback, void *user_data)
{
    lv_obj_t *row = lv_list_add_button(file_list, symbol, text);
    set_file_row_font(row);
    lv_obj_set_height(row, UI_LIST_ROW_HEIGHT);
    lv_obj_set_style_pad_ver(row, 1, 0);
    lv_obj_set_style_outline_width(row, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xDBEAFE), LV_STATE_FOCUSED);
    lv_obj_t *label = lv_obj_get_child(row, 1);
    if (label)
    {
        lv_obj_set_width(label, 115);
        lv_obj_set_height(label, lv_font_get_line_height(UI_FONT_BODY));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_group_add_obj(lv_group_get_default(), row);
    if (callback)
        lv_obj_add_event_cb(row, callback, LV_EVENT_CLICKED, user_data);
    return row;
}

static void clear_file_rows()
{
    const uint32_t count = lv_obj_get_child_count(file_list);
    for (uint32_t i = 0; i < count; ++i)
        lv_group_remove_obj(lv_obj_get_child(file_list, i));
    lv_obj_clean(file_list);
}

static void focus_first_file_row()
{
    lv_obj_t *row = lv_obj_get_child(file_list, 0);
    if (row)
    {
        lv_group_focus_obj(row);
        lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    }
}

static void request_file_page()
{
    if (!file_list || !file_path)
        return;

    clear_file_rows();
    lv_label_set_text_fmt(file_path, "TF:%s", current_file_path);
    lv_obj_t *refresh = add_file_row(LV_SYMBOL_REFRESH, "刷新", file_refresh_cb, nullptr);
    lv_group_focus_obj(refresh);

    if (!tf::is_mounted())
    {
        lv_obj_t *row = add_file_row(LV_SYMBOL_WARNING, "TF卡未挂载", nullptr, nullptr);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(row, LV_STATE_DISABLED);
        return;
    }

    lv_obj_t *row = add_file_row(LV_SYMBOL_REFRESH, "加载中...", nullptr, nullptr);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_state(row, LV_STATE_DISABLED);
    ++file_request_id;
    if (file_request_id == 0)
        ++file_request_id;
    if (!tf::request_list(current_file_path, file_request_id))
        render_file_page(false);
}

static void render_file_page(bool success)
{
    clear_file_rows();
    add_file_row(LV_SYMBOL_REFRESH, "刷新", file_refresh_cb, nullptr);

    if (std::strcmp(current_file_path, "/") != 0)
        add_file_row(LV_SYMBOL_LEFT, "返回上级", file_parent_cb, nullptr);

    if (!success)
    {
        lv_obj_t *row = add_file_row(LV_SYMBOL_WARNING, "读取失败", nullptr, nullptr);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(row, LV_STATE_DISABLED);
        focus_first_file_row();
        return;
    }

    if (file_entry_count == 0)
    {
        lv_obj_t *row = add_file_row(LV_SYMBOL_FILE, "目录为空", nullptr, nullptr);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(row, LV_STATE_DISABLED);
    }

    for (size_t i = 0; i < file_entry_count; ++i)
    {
        char row_text[TF_FILE_NAME_MAX + 20];
        if (!file_entries[i].name_complete)
        {
            std::snprintf(row_text, sizeof(row_text), "文件名过长");
            lv_obj_t *row = add_file_row(LV_SYMBOL_WARNING, row_text, nullptr, nullptr);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_state(row, LV_STATE_DISABLED);
            continue;
        }
        if (file_entries[i].directory)
            std::snprintf(row_text, sizeof(row_text), "%s/", file_entries[i].name);
        else if (file_entries[i].size >= 1024 * 1024)
            std::snprintf(row_text, sizeof(row_text), "%s %lluM", file_entries[i].name,
                          static_cast<unsigned long long>(file_entries[i].size / (1024 * 1024)));
        else if (file_entries[i].size >= 1024)
            std::snprintf(row_text, sizeof(row_text), "%s %lluK", file_entries[i].name,
                          static_cast<unsigned long long>(file_entries[i].size / 1024));
        else
            std::snprintf(row_text, sizeof(row_text), "%s %lluB", file_entries[i].name,
                          static_cast<unsigned long long>(file_entries[i].size));
        char entry_path[TF_PATH_MAX];
        const bool path_complete = join_file_path(file_entries[i].name, entry_path, sizeof(entry_path));
        const bool removable = !file_entries[i].directory && path_complete &&
                               tf::is_deletable_path(entry_path);
        lv_obj_t *row = add_file_row(file_entries[i].directory ? LV_SYMBOL_DIRECTORY :
                                         (removable ? LV_SYMBOL_TRASH : LV_SYMBOL_FILE),
                                      row_text, (file_entries[i].directory || removable) && !file_operation_pending ? file_row_cb : nullptr,
                                      &file_entries[i]);
        if ((!removable && !file_entries[i].directory) || file_operation_pending)
        {
            lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_state(row, LV_STATE_DISABLED);
        }
    }

    if (file_list_truncated)
    {
        lv_obj_t *row = add_file_row(LV_SYMBOL_WARNING, "仅显示前24项", nullptr, nullptr);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(row, LV_STATE_DISABLED);
    }
    focus_first_file_row();
}

static bool join_file_path(const char *name, char *path, size_t path_size)
{
    const char *separator = std::strcmp(current_file_path, "/") == 0 ? "" : "/";
    const int written = std::snprintf(path, path_size, "%s%s%s", current_file_path, separator, name);
    return written >= 0 && static_cast<size_t>(written) < path_size;
}

static void file_row_cb(lv_event_t *e)
{
    tf::FileEntry *entry = static_cast<tf::FileEntry *>(lv_event_get_user_data(e));
    if (!entry)
        return;
    if (entry->directory)
    {
        char next_path[TF_PATH_MAX];
        if (!join_file_path(entry->name, next_path, sizeof(next_path)))
        {
            render_file_page(false);
            return;
        }
        std::snprintf(current_file_path, sizeof(current_file_path), "%s", next_path);
        request_file_page();
        return;
    }

    if (!join_file_path(entry->name, selected_file_path, sizeof(selected_file_path)))
    {
        render_file_page(false);
        return;
    }
    ui_popwin_msgbox("确认删除这个文件?", nullptr, lv_event_get_target_obj(e),
                     LV_SYMBOL_TRASH, "删除文件", false, "删除", file_delete_cb,
                     nullptr, "取消", file_cancel_cb, nullptr);
}

static void file_delete_cb(lv_event_t *)
{
    ++file_request_id;
    if (file_request_id == 0)
        ++file_request_id;
    if (!tf::request_remove_file(selected_file_path, file_request_id))
        render_file_page(false);
    else
        file_operation_pending = true;
}

static void file_cancel_cb(lv_event_t *) {}

static void file_refresh_cb(lv_event_t *)
{
    request_file_page();
}

static void file_parent_cb(lv_event_t *)
{
    char *separator = std::strrchr(current_file_path, '/');
    if (separator && separator != current_file_path)
        *separator = '\0';
    if (!separator || separator == current_file_path)
        std::snprintf(current_file_path, sizeof(current_file_path), "/");
    request_file_page();
}

static void file_timer_cb(lv_timer_t *)
{
    bool success = false;
    uint32_t request_id = 0;
    if (tf::take_list_result(file_entries, TF_FILE_LIST_MAX, file_entry_count,
                             file_list_truncated, success, request_id))
    {
        if (request_id == file_request_id)
            render_file_page(success);
        else
            request_file_page();
        return;
    }
    if (tf::take_remove_result(success, request_id))
    {
        file_operation_pending = false;
        if (request_id != file_request_id)
        {
            request_file_page();
            return;
        }
        request_file_page();
        if (!success)
        {
            LOGGER_WARN("TF file removal failed.");
            ui_popwin_msgbox("删除失败,文件可能正在使用.", nullptr, lv_obj_get_child(file_list, 0),
                             LV_SYMBOL_WARNING, "提示", false, "确定", file_cancel_cb, nullptr);
        }
    }
}
