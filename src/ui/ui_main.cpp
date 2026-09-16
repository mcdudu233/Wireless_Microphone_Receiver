#include "ui/ui_main.h"
#include "module/screen.h"
#include "config.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <new>

static lv_style_t style_indic_h; // 横向bar样式
static lv_obj_t *info_widget;
static lv_obj_t *main_widget;
static lv_obj_t *tabview;                                                                           // 最多4个                                                                              // 纵向bar样式
static lv_obj_t *ui_create_device_card(lv_obj_t *parent, std::string &device_mac, bool color_test); // 创建自定义card容器组件

// 电量/信号等级(0-100)映射为渐变色:0红→50黄→100绿
static lv_color_t ui_level_color(uint8_t level)
{
    return lv_color_hsv_to_rgb(level * 120U / 100U, 80, 90);
}

// RSSI(dBm)映射为0-100信号强度;0视为未知信号
static uint8_t ui_rssi_to_level(int8_t rssi)
{
    if (rssi == 0)
        return 0;
    if (rssi <= -90)
        return 5; // 极弱信号仍显示一格(红色)
    if (rssi >= -35)
        return 100;
    return (uint8_t)((rssi + 90) * 100 / 55);
}

static std::vector<std::string> linked_devices;
static std::vector<device_card_data *> cards;

static lv_timer_t *timer_update;
static uint16_t timer_update_elapsed;
static lv_group_t *main_group;
static bool style_indic_h_initialized;

// 状态栏图标(采样率/USB模式/传输模式)与配置缓存:
// 设置页可修改这三项配置且主界面不重建,定时器对比缓存按需换图
static lv_obj_t *status_img_rate;
static lv_obj_t *status_img_usb;
static lv_obj_t *status_img_rf;
static AudioRate cached_rate;
static USBMode cached_usb_mode;
static RFMode cached_rf_mode;

// 采样率→音质图标(48k青/96k蓝/192k紫)
static const lv_image_dsc_t *ui_rate_icon(AudioRate rate)
{
    switch (rate)
    {
    case AUDIO_RATE_96000:
        return &ui_img_rate96;
    case AUDIO_RATE_192000:
        return &ui_img_rate192;
    case AUDIO_RATE_48000:
    default:
        return &ui_img_rate48;
    }
}

// USB模式→图标(关闭灰/音频蓝/SD卡琥珀/JTAG紫)
static const lv_image_dsc_t *ui_usb_icon(USBMode mode)
{
    switch (mode)
    {
    case USB_MODE_AUDIO:
        return &ui_img_usb_audio;
    case USB_MODE_SD:
        return &ui_img_usb_sd;
    case USB_MODE_JTAG:
        return &ui_img_usb_jtag;
    case USB_MODE_NONE:
    default:
        return &ui_img_usb_none;
    }
}

// 传输模式→图标(BLE蓝牙蓝/WiFi绿)
static const lv_image_dsc_t *ui_rf_icon(RFMode mode)
{
    return mode == RF_MODE_BLE ? &ui_img_rf_ble : &ui_img_rf_wifi;
}

// 断线重连提示状态
static lv_obj_t *reconnect_chip;               // "重连中"状态条(挂在info_widget上,非模态)
static std::vector<std::string> reconnect_macs; // 处于重连等待的设备MAC
static bool pending_return_bt = false;           // 设置页在前台时挂起的"返回设备连接页"请求

static void setting_widget_cb(lv_event_t *e); // 设置按钮回调
static void ui_main_show_link_lost_msgbox(const char *line1, const char *line2); // 连接失败提示框(自动关闭)

void ui_main_init()
{
    main_group = lv_group_create();
    lv_group_set_default(main_group);

    if (!style_indic_h_initialized)
    {
        lv_style_init(&style_indic_h);
        lv_style_set_bg_opa(&style_indic_h, LV_OPA_COVER);
        lv_style_set_bg_color(&style_indic_h, lv_palette_main(LV_PALETTE_GREEN));
        lv_style_set_bg_grad_color(&style_indic_h, lv_palette_main(LV_PALETTE_RED));
        lv_style_set_bg_grad_dir(&style_indic_h, LV_GRAD_DIR_HOR);
        style_indic_h_initialized = true;
    }

    lv_obj_t *btn;
    lv_obj_t *label;

    main_widget = ui_add_win();
    info_widget = lv_obj_create(main_widget);
    lv_obj_set_style_bg_color(info_widget, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(info_widget, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(info_widget, 0, 0);
    lv_obj_set_style_border_width(info_widget, 0, 0);
    lv_obj_set_style_radius(info_widget, 6, 0);
    lv_obj_set_style_shadow_width(info_widget, 6, 0);
    lv_obj_set_style_shadow_opa(info_widget, LV_OPA_10, 0);
    lv_obj_set_style_shadow_ofs_y(info_widget, 2, 0);
    lv_obj_set_size(info_widget, 160 * 0.6, 70);
    lv_obj_align_to(info_widget, main_widget, LV_ALIGN_TOP_LEFT, 5, 5);

    tabview = lv_tabview_create(info_widget);
    lv_obj_set_size(tabview, 96, 70);
    lv_obj_set_style_pad_all(tabview, 0, 0);
    lv_obj_set_style_border_width(tabview, 0, 0);
    lv_tabview_set_tab_bar_size(tabview, 20);
    lv_obj_align(tabview, LV_ALIGN_CENTER, 0, 0);

    int i = 0;

    linked_devices = ui_bt_get_linked();
    for (std::string dev : linked_devices)
    {
        // logger::infoln(dev.c_str());
        // 页签名使用设备持久编号,与设备连接页"设备N"称谓一致(列表已按编号排序)
        lv_obj_t *tab = lv_tabview_add_tab(tabview, std::to_string(ui_info_get_number(dev)).c_str());
        lv_obj_t *card = ui_create_device_card(tab, dev, false);
        lv_obj_set_style_pad_all(tab, 0, 0);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
        device_card_data *data = (device_card_data *)lv_obj_get_user_data(card);
        data->tab = tab;
        i++;
    }
    if (linked_devices.empty())
    {
        lv_obj_t *empty_label = lv_label_create(info_widget);
        lv_label_set_text(empty_label, "暂无已连接设备");
        lv_obj_set_style_text_font(empty_label, &lv_font_harmonyos_12, 0);
        lv_obj_align(empty_label, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview); // 标题栏
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0xF4F5F7), 0);
    uint32_t cnt = lv_obj_get_child_count_by_type(tab_bar, &lv_button_class);
    for (int i = 0; i < cnt; i++)
    {
        btn = lv_obj_get_child_by_type(tab_bar, i, &lv_button_class);
        label = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_font(label, &lv_font_harmonyos_12, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x6B7280), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D6BDB), LV_STATE_CHECKED);
        lv_obj_set_style_radius(btn, 4, 0);
    }

    btn = ui_add_button(main_widget, "设置", 160 * 0.3, UI_ACTION_HEIGHT, UI_FONT_BODY);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x2D6BDB), 0);
    label = lv_obj_get_child(btn, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x2D6BDB), 0);
    lv_obj_align_to(btn, info_widget, LV_ALIGN_OUT_RIGHT_MID, 3, 20);
    lv_obj_add_event_cb(btn, setting_widget_cb, LV_EVENT_CLICKED, main_widget); // 切换窗体并隐藏

    // 状态栏:三个16x16彩色Tabler图标(左→右:采样率/USB模式/传输模式),
    // 颜色语义化编码当前状态,替代原灰色字体符号
    status_img_rf = lv_img_create(main_widget);
    lv_img_set_src(status_img_rf, ui_rf_icon(config::config.rf.mode));
    lv_obj_set_size(status_img_rf, 16, 16);
    lv_img_set_zoom(status_img_rf, 64);
    lv_obj_align(status_img_rf, LV_ALIGN_TOP_RIGHT, -UI_SCREEN_EDGE, 2);

    status_img_usb = lv_img_create(main_widget);
    lv_img_set_src(status_img_usb, ui_usb_icon(config::config.usb.mode));
    lv_obj_set_size(status_img_usb, 16, 16);
    lv_img_set_zoom(status_img_usb, 64);
    lv_obj_align_to(status_img_usb, status_img_rf, LV_ALIGN_OUT_LEFT_MID, -3, 0);

    status_img_rate = lv_img_create(main_widget);
    lv_img_set_src(status_img_rate, ui_rate_icon(config::config.audio.rate));
    lv_obj_set_size(status_img_rate, 16, 16);
    lv_img_set_zoom(status_img_rate, 64);
    lv_obj_align_to(status_img_rate, status_img_usb, LV_ALIGN_OUT_LEFT_MID, -3, 0);

    cached_rate = config::config.audio.rate;
    cached_usb_mode = config::config.usb.mode;
    cached_rf_mode = config::config.rf.mode;

    ui_bind_group_to_all_encoders(lv_group_get_default());

    timer_update_elapsed = UPDATE_INFO_PERIOD;
    timer_update = lv_timer_create([](lv_timer_t *t)
                                   {
                                        // 挂起的"返回设备连接页":设置页退出、主界面重新可见后执行
                                        if (pending_return_bt && main_widget != nullptr && lv_obj_is_valid(main_widget) && !lv_obj_has_flag(main_widget, LV_OBJ_FLAG_HIDDEN))
                                        {
                                            pending_return_bt = false;
                                            ui_free_main_widget();
                                            ui_bt_init();
                                            ui_main_show_link_lost_msgbox("设备连接失败", "已返回选择设备");
                                            return;
                                        }

                                         uint32_t act = lv_tabview_get_tab_active(tabview);
                                         lv_obj_t *content = lv_tabview_get_content(tabview);
                                         if (content == nullptr || act >= lv_obj_get_child_count(content))
                                             return;
                                         lv_obj_t *cur_tab = lv_obj_get_child(content, act);
                                         if (cur_tab == nullptr)
                                             return;
                                         lv_obj_t *card = lv_obj_get_child(cur_tab, 0);
                                         if (card == nullptr)
                                             return;
                                        device_card_data *card_data = (device_card_data *)lv_obj_get_user_data(card);
                                        
                                        // 设置卡片内信息
                                        if (card_data == nullptr)
                                            return;

                                        // RF侧已经生成平滑包络，避免连续创建LVGL动画造成延迟和额外开销。
                                        lv_bar_set_value(card_data->left_voice_bar, ui_info_get_left_voice(card_data->device_mac), LV_ANIM_OFF);
                                        lv_bar_set_value(card_data->right_voice_bar, ui_info_get_right_voice(card_data->device_mac), LV_ANIM_OFF);
                                         timer_update_elapsed += UPDATE_TIMER_PERIOD;
                                         if (timer_update_elapsed >= UPDATE_INFO_PERIOD)
                                         {
                                             timer_update_elapsed = 0;
                                             // 状态栏图标:设置页可能修改采样率/USB/传输模式,
                                             // 主界面不重建,仅在配置变化时换图
                                             if (config::config.audio.rate != cached_rate)
                                             {
                                                 cached_rate = config::config.audio.rate;
                                                 lv_img_set_src(status_img_rate, ui_rate_icon(cached_rate));
                                             }
                                             if (config::config.usb.mode != cached_usb_mode)
                                             {
                                                 cached_usb_mode = config::config.usb.mode;
                                                 lv_img_set_src(status_img_usb, ui_usb_icon(cached_usb_mode));
                                             }
                                             if (config::config.rf.mode != cached_rf_mode)
                                             {
                                                 cached_rf_mode = config::config.rf.mode;
                                                 lv_img_set_src(status_img_rf, ui_rf_icon(cached_rf_mode));
                                             }
                                             // 电池:电量条填充宽度+渐变色(满绿→半黄→低红)
                                             const uint8_t battery = (uint8_t)std::clamp((int)ui_info_get_power(card_data->device_mac), 0, 100);
                                             lv_bar_set_value(card_data->battery_fill, battery, LV_ANIM_OFF);
                                             lv_obj_set_style_bg_color(card_data->battery_fill, ui_level_color(battery), LV_PART_INDICATOR);
                                             // 信号:按RSSI点亮格数并用渐变色提示强弱
                                             const uint8_t sig_level = ui_rssi_to_level(ui_info_get_signal(card_data->device_mac));
                                             const uint8_t sig_bars = sig_level ? (uint8_t)((sig_level * 4 + 99) / 100) : 0;
                                             const lv_color_t sig_color = ui_level_color(sig_level);
                                             for (uint8_t i = 0; i < 4; i++)
                                             {
                                                 lv_obj_set_style_bg_color(card_data->signal_bars[i],
                                                                           i < sig_bars ? sig_color : lv_color_hex(0xE5E7EB), 0);
                                             }
                                             // 丢包率:0%绿色,非0%红色
                                             const uint8_t loss = ui_info_get_loss(card_data->device_mac);
                                             lv_label_set_text_fmt(card_data->loss_label, "%u%%", (unsigned)loss);
                                             lv_obj_set_style_text_color(card_data->loss_label,
                                                                         loss ? lv_palette_main(LV_PALETTE_RED) : lv_color_hex(0x059669), 0);
                                         } },
                                   UPDATE_TIMER_PERIOD, NULL);
}

// 设置按钮回调 -> 进入设置界面
static void setting_widget_cb(lv_event_t *e)
{
    ui_setting_init(main_widget);
}

// 创建设备卡片
static lv_obj_t *ui_create_device_card(lv_obj_t *parent, std::string &device_mac, bool color_test)
{
    device_card_data *data = new (std::nothrow) device_card_data{};
    LV_ASSERT_MALLOC(data);

    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_size(card, 94, 48);

    lv_obj_t *left_label = lv_label_create(card);
    lv_label_set_text(left_label, "L");
    lv_obj_align(left_label, LV_ALIGN_CENTER, -40, -15);
    lv_obj_set_style_text_font(left_label, &lv_font_harmonyos_12, 0);
    lv_obj_set_style_text_color(left_label, lv_color_hex(0x6B7280), 0);
    lv_obj_t *right_label = lv_label_create(card);
    lv_label_set_text(right_label, "R");
    lv_obj_align(right_label, LV_ALIGN_CENTER, -40, 0);
    lv_obj_set_style_text_font(right_label, &lv_font_harmonyos_12, 0);
    lv_obj_set_style_text_color(right_label, lv_color_hex(0x6B7280), 0);

    data->left_voice_bar = lv_bar_create(card);
    lv_obj_add_style(data->left_voice_bar, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(data->left_voice_bar, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_size(data->left_voice_bar, 77, 10);
    lv_obj_align_to(data->left_voice_bar, left_label, LV_ALIGN_OUT_RIGHT_MID, 5, 3);
    lv_bar_set_range(data->left_voice_bar, 0, 100);

    data->right_voice_bar = lv_bar_create(card);
    lv_obj_add_style(data->right_voice_bar, &style_indic_h, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(data->right_voice_bar, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_size(data->right_voice_bar, 77, 10);
    lv_obj_align_to(data->right_voice_bar, right_label, LV_ALIGN_OUT_RIGHT_MID, 5, 3);
    lv_bar_set_range(data->right_voice_bar, 0, 100);

    // 电池图标:边框+右侧极耳+内部电量填充条(填充宽度与颜色表示电量,满绿→黄→低红)
    // 底部状态行(左→右):信号格 → 丢包率 → 电池,整体右对齐成组,电池贴卡片右缘
    lv_obj_t *battery = lv_obj_create(card);
    lv_obj_set_size(battery, 24, 10);
    lv_obj_align(battery, LV_ALIGN_BOTTOM_RIGHT, 0, -1);
    lv_obj_set_style_pad_all(battery, 0, 0);
    lv_obj_set_style_border_width(battery, 0, 0);
    lv_obj_set_style_bg_opa(battery, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(battery, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *battery_body = lv_obj_create(battery);
    lv_obj_set_size(battery_body, 21, 10);
    lv_obj_align(battery_body, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(battery_body, 1, 0);
    lv_obj_set_style_border_width(battery_body, 1, 0);
    lv_obj_set_style_border_color(battery_body, lv_color_hex(0x6B7280), 0);
    lv_obj_set_style_radius(battery_body, 2, 0);
    lv_obj_set_style_bg_opa(battery_body, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(battery_body, LV_OBJ_FLAG_SCROLLABLE);

    data->battery_fill = lv_bar_create(battery_body);
    lv_obj_set_size(data->battery_fill, 17, 6);
    lv_bar_set_range(data->battery_fill, 0, 100);
    lv_bar_set_value(data->battery_fill, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(data->battery_fill, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(data->battery_fill, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(data->battery_fill, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(data->battery_fill, 1, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(data->battery_fill, ui_level_color(100), LV_PART_INDICATOR);

    lv_obj_t *battery_cap = lv_obj_create(battery);
    lv_obj_set_size(battery_cap, 2, 4);
    lv_obj_align(battery_cap, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_pad_all(battery_cap, 0, 0);
    lv_obj_set_style_border_width(battery_cap, 0, 0);
    lv_obj_set_style_radius(battery_cap, 1, 0);
    lv_obj_set_style_bg_color(battery_cap, lv_color_hex(0x6B7280), 0);
    lv_obj_set_style_bg_opa(battery_cap, LV_OPA_COVER, 0);
    lv_obj_remove_flag(battery_cap, LV_OBJ_FLAG_SCROLLABLE);

    // 丢包率:0%绿色,非0%红色(先定字体/尺寸/长文本模式,最后对齐,避免按空标签尺寸定位)
    data->loss_label = lv_label_create(card);
    lv_obj_set_style_text_font(data->loss_label, UI_FONT_BODY, 0);
    lv_obj_set_size(data->loss_label, 30, lv_font_get_line_height(UI_FONT_BODY));
    lv_label_set_long_mode(data->loss_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(data->loss_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(data->loss_label, "0%");
    lv_obj_set_style_text_color(data->loss_label, lv_color_hex(0x059669), 0);
    // 标签盒高15px(行高)而其余状态件高10px:较底对齐下移1px光学校正,使文字中心对齐10px状态带中心
    // (数字与%无下降部,盒底溢出卡片1px处不会绘制字形)
    lv_obj_align(data->loss_label, LV_ALIGN_BOTTOM_RIGHT, -28, 1);

    // 信号强度:4根递增格,点亮数量与渐变色表示强弱
    lv_obj_t *signal = lv_obj_create(card);
    lv_obj_set_size(signal, 14, 10);
    // 右缘距卡片右缘62px:右邻丢包率标签(盒36..66)留4px间距,盒18..32
    lv_obj_align(signal, LV_ALIGN_BOTTOM_RIGHT, -62, -1);
    lv_obj_set_style_pad_all(signal, 0, 0);
    lv_obj_set_style_border_width(signal, 0, 0);
    lv_obj_set_style_bg_opa(signal, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(signal, LV_OBJ_FLAG_SCROLLABLE);
    static const uint8_t signal_bar_heights[4] = {4, 6, 8, 10};
    for (uint8_t i = 0; i < 4; i++)
    {
        data->signal_bars[i] = lv_obj_create(signal);
        lv_obj_set_size(data->signal_bars[i], 2, signal_bar_heights[i]);
        lv_obj_align(data->signal_bars[i], LV_ALIGN_BOTTOM_LEFT, i * 4, 0);
        lv_obj_set_style_pad_all(data->signal_bars[i], 0, 0);
        lv_obj_set_style_border_width(data->signal_bars[i], 0, 0);
        lv_obj_set_style_radius(data->signal_bars[i], 1, 0);
        lv_obj_set_style_bg_color(data->signal_bars[i], lv_color_hex(0xE5E7EB), 0);
        lv_obj_set_style_bg_opa(data->signal_bars[i], LV_OPA_COVER, 0);
        lv_obj_remove_flag(data->signal_bars[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    if (color_test)
    {
        // 颜色测试
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_exec_cb(&a, ui_set_bar_val);
        lv_anim_set_duration(&a, 3000);
        lv_anim_set_reverse_duration(&a, 3000);
        lv_anim_set_var(&a, data->left_voice_bar);
        lv_anim_set_values(&a, 0, 100);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);

        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_exec_cb(&b, ui_set_bar_val);
        lv_anim_set_duration(&b, 2000);
        lv_anim_set_var(&b, data->right_voice_bar);
        lv_anim_set_values(&b, 0, 100);
        lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&b);
    }

    data->device_mac = device_mac;

    lv_obj_set_user_data(card, data);
    cards.push_back(data);
    return card;
}

void ui_free_main_widget()
{
    if (timer_update != nullptr)
    {
        lv_timer_delete(timer_update);
        timer_update = nullptr;
    }
    for (size_t i = 0; i < cards.size(); ++i)
    {
        device_card_data *card_data = cards[i];
        if (card_data)
        {
            delete card_data;
        }
    }
    cards.clear();

    // 重连提示状态随界面一并复位(reconnect_chip挂在main_widget子树内,随之销毁)
    reconnect_macs.clear();
    reconnect_chip = nullptr;
    pending_return_bt = false;

    if (main_group)
    {
        lv_group_delete(main_group);
        main_group = nullptr;
    }

    if (main_widget && lv_obj_is_valid(main_widget))
    {
        lv_obj_delete(main_widget);
        main_widget = NULL;
    }
}

// api

device_card_data *ui_info_get_obj(const std::string &mac)
{
    int i = 0;
    for (device_card_data *dev : cards)
    {
        if (dev->device_mac == mac)
        {
            return dev;
        }
        i++;
    }
    return nullptr;
}

void ui_info_set_bar_pct(lv_obj_t *bar, int8_t pct)
{
    LV_LOCK();
    lv_bar_set_value(bar, pct, LV_ANIM_ON);
    LV_UNLOCK();
}

void ui_info_del_card(const std::string &mac)
{
    int i = 0;
    for (device_card_data *card : cards)
    {
        if (card->device_mac == mac)
        {
            bool defer = false;
            LV_LOCK();
            lv_obj_del(card->tab);
            delete card;
            cards.erase(cards.begin() + i);
            // 设置页在前台(主界面被隐藏)时不能立即切换页面,挂起请求等待其退出
            if (cards.empty() && main_widget != nullptr && lv_obj_is_valid(main_widget) &&
                lv_obj_has_flag(main_widget, LV_OBJ_FLAG_HIDDEN))
            {
                pending_return_bt = true;
                defer = true;
            }
            LV_UNLOCK();

            // 如果所有卡片都被删除，返回蓝牙连接窗口
            // (本函数可能被rf任务调用,销毁/重建界面必须持有LVGL锁)
            if (cards.empty() && !defer)
            {
                LV_LOCK();
                ui_free_main_widget();
                ui_bt_init();
                LV_UNLOCK();
            }
            break;
        }
        i++;
    }
}
void ui_info_del_card(device_card_data *card)
{
    ui_info_del_card(std::string(card->device_mac));
}

/*****************************
      断线重连提示
*****************************/
// 刷新"重连中"状态条(调用方需已持有LVGL锁)
// 非模态:不进入任何编码器分组,不阻挡主界面操作
static void reconnect_chip_refresh()
{
    if (reconnect_macs.empty())
    {
        if (reconnect_chip != nullptr && lv_obj_is_valid(reconnect_chip))
            lv_obj_delete(reconnect_chip);
        reconnect_chip = nullptr;
        return;
    }

    if (reconnect_chip == nullptr || !lv_obj_is_valid(reconnect_chip))
    {
        reconnect_chip = lv_obj_create(info_widget);
        lv_obj_set_size(reconnect_chip, 94, 18);
        lv_obj_align(reconnect_chip, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_pad_all(reconnect_chip, 0, 0);
        lv_obj_set_style_border_width(reconnect_chip, 0, 0);
        lv_obj_set_style_radius(reconnect_chip, 4, 0);
        lv_obj_set_style_bg_color(reconnect_chip, lv_color_hex(0xFFFBEB), 0); // status-warning-bg
        lv_obj_set_style_bg_opa(reconnect_chip, LV_OPA_COVER, 0);

        lv_obj_t *label = lv_label_create(reconnect_chip);
        lv_obj_set_style_text_font(label, UI_FONT_BODY, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0); // text-primary
        lv_obj_set_width(label, 88);
        lv_obj_set_height(label, lv_font_get_line_height(UI_FONT_BODY));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_label_set_text(label, "重连中..."); // 先有文本再对齐(随后由刷新覆写为带序号形式)
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t *label = lv_obj_get_child(reconnect_chip, 0);
    if (label == nullptr)
        return;
    if (reconnect_macs.size() == 1)
    {
        // 单设备标注设备序号(持久编号,与页签/设备连接页称谓一致)
        lv_label_set_text_fmt(label, "设备%d重连中...", ui_info_get_number(reconnect_macs[0]));
    }
    else
    {
        lv_label_set_text_fmt(label, "重连中... x%d", (int)reconnect_macs.size());
    }
}

bool ui_main_has_device(const std::string &mac)
{
    LV_LOCK();
    bool has = main_widget != nullptr && lv_obj_is_valid(main_widget) &&
               ui_info_get_obj(mac) != nullptr;
    LV_UNLOCK();
    return has;
}

void ui_main_set_reconnect(const std::string &mac, bool reconnecting)
{
    LV_LOCK();
    if (reconnecting)
    {
        bool exists = false;
        for (const std::string &m : reconnect_macs)
        {
            if (m == mac)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            reconnect_macs.push_back(mac);
    }
    else
    {
        for (auto it = reconnect_macs.begin(); it != reconnect_macs.end(); ++it)
        {
            if (*it == mac)
            {
                reconnect_macs.erase(it);
                break;
            }
        }
    }
    if (main_widget != nullptr && lv_obj_is_valid(main_widget))
        reconnect_chip_refresh();
    LV_UNLOCK();
}

// 连接失败提示框:无按键,点击或2.5秒后自动关闭
static void ui_main_show_link_lost_msgbox(const char *line1, const char *line2)
{
    LV_LOCK();
    char text[64];
    if (line2 != nullptr)
        snprintf(text, sizeof(text), "%s\n%s", line1, line2);
    else
        snprintf(text, sizeof(text), "%s", line1);

    lv_obj_t *popup = ui_popwin_msgbox(text, nullptr, nullptr, &ui_img_tips, "连接断开:");
    lv_timer_t *autoclose = lv_timer_create(
        [](lv_timer_t *timer)
        {
            lv_obj_t *cont = (lv_obj_t *)lv_timer_get_user_data(timer);
            if (cont != nullptr && lv_obj_is_valid(cont))
                lv_obj_send_event(cont, LV_EVENT_CLICKED, NULL); // 复用弹窗点击关闭逻辑(恢复分组并销毁)
        },
        2500, popup);
    lv_timer_set_repeat_count(autoclose, 1); // 仅执行一次,到点自动删除定时器
    LV_UNLOCK();
}

bool ui_main_notify_link_lost(const std::string &mac)
{
    LV_LOCK();
    bool has = main_widget != nullptr && lv_obj_is_valid(main_widget) &&
               ui_info_get_obj(mac) != nullptr;
    bool hidden = has && lv_obj_has_flag(main_widget, LV_OBJ_FLAG_HIDDEN);
    bool last = has && (cards.size() == 1);
    // 设备称谓用持久编号(与页签/设备连接页一致)
    const uint8_t number = ui_info_get_number(mac);
    for (auto it = reconnect_macs.begin(); it != reconnect_macs.end(); ++it)
    {
        if (*it == mac)
        {
            reconnect_macs.erase(it);
            break;
        }
    }
    LV_UNLOCK();

    if (!has)
        return false; // 设备不在主界面(如用户已在设备页手动断开)

    // 移除卡片;若为最后一张,内部会切回设备连接页(设置页在前台时改为挂起请求)
    ui_info_del_card(mac);

    if (!hidden)
    {
        char line1[32];
        snprintf(line1, sizeof(line1), "设备%d连接失败", number);
        ui_main_show_link_lost_msgbox(line1, last ? "已返回选择设备" : nullptr);
    }
    // 设置页在前台时静默处理:挂起的返回请求由刷新定时器执行,返回后再补提示
    return true;
}
