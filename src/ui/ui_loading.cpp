#include "config.h"
#include "module/screen.h"
#include "ui/ui.h"
#include "ui/ui_bt.h"
#include "ui/ui_loading.h"

// 开机自动连接等待:模块加载完成后先扫描并自动连接,
// 等待期内任一设备连上则直接进主界面,超时未连上才进设备连接页
#define LOADING_CONNECT_TIMEOUT_MS 5000
#define LOADING_CONNECT_TIMER_PERIOD 200

static lv_obj_t *loading_widget;
static lv_obj_t *bar;
static lv_obj_t *pct;       // 标签
static uint8_t percent = 0; // 加载百分比
static std::string loading_part = "";
static uint32_t connectStartTick = 0; // 连接等待起始时刻

void ui_loading_set_percent(uint8_t p)
{
    LV_LOCK();
    percent = p;
    LV_UNLOCK();
}

// 设置当前加载部分的名字
void ui_loading_set_part(const std::string &part)
{
    LV_LOCK();
    loading_part = part;
    LV_UNLOCK();
}

// 开机连接等待定时器:任一设备连上直接进主界面,超时进设备连接页
static void loading_connect_timer_cb(lv_timer_t *timer)
{
    const bool connected = ui_bt_linked_count() >= 1;
    if (!connected && lv_tick_elaps(connectStartTick) < LOADING_CONNECT_TIMEOUT_MS)
    {
        return;
    }
    lv_timer_delete(timer);
    lv_obj_delete(loading_widget);
    loading_widget = nullptr;
    if (connected)
    {
        if (config::config.rf.mode == RF_MODE_WIFI)
        {
            // WiFi模式:经设备页走既有"完成"流程(BLE->WiFi迁移后自动进主界面,
            // 迁移失败时留在设备页,可手动重试)
            ui_bt_init();
            ui_bt_try_finish();
        }
        else
        {
            // BLE模式:下发音频启动命令后直接进主界面
            ui_bt_pause_search();
            ui_main_init();
        }
    }
    else
    {
        // 未发现/未连上任何设备:进设备连接页手动配对(发现后仍会自动连接)
        ui_bt_init();
    }
}

// 加载timer的周期回调函数
static void loding_timer_cb(lv_timer_t *timer)
{
    uint32_t val = lv_bar_get_value(bar);
    lv_label_set_text_fmt(pct, "正在加载:%s", loading_part.c_str());
    if (percent > val)
    {
        val++;
        lv_bar_set_value(bar, val, LV_ANIM_ON);
        // lv_label_set_text_fmt(pct, "%d%%", val);
    }
    if (val >= lv_bar_get_max_value(bar))
    {
        // 模块加载完成:转入"正在连接设备"阶段,
        // 由连接定时器决定去主界面(连上)还是设备连接页(超时)
        lv_label_set_text(pct, "正在连接设备...");
        lv_obj_set_style_text_color(pct, lv_color_hex(0x6B7280), 0);
        ui_bt_search(); // 开始扫描+发现即自动连接(设备页未打开,行更新为空操作)
        connectStartTick = lv_tick_get();
        lv_timer_delete(timer);
        lv_timer_create(loading_connect_timer_cb, LOADING_CONNECT_TIMER_PERIOD, NULL);
        LV_LOG_INFO("加载完成,等待设备连接");
    }
}

// ui初始化函数
void ui_loading_init()
{
    // 设置全局样式和字体
    lv_obj_set_style_text_font(lv_screen_active(), UI_FONT_BODY, LV_STATE_DEFAULT);

    lv_obj_t *label;

    // 设置屏幕背景色为白色
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

    loading_widget = lv_obj_create(lv_screen_active());
    lv_obj_set_size(loading_widget, 150, 72);
    lv_obj_center(loading_widget);
    lv_obj_set_style_pad_all(loading_widget, 6, 0);
    lv_obj_set_style_radius(loading_widget, 8, 0);
    lv_obj_set_style_bg_color(loading_widget, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(loading_widget, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(loading_widget, 10, 0);
    lv_obj_set_style_shadow_opa(loading_widget, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(loading_widget, 3, 0);

    lv_obj_t *img = lv_img_create(loading_widget);
    lv_img_set_src(img, &ui_img_microphone);
    lv_img_set_zoom(img, 64);
    lv_obj_set_size(img, 32, 32);
    lv_obj_align(img, LV_ALIGN_TOP_MID, -45, 0);

    label = lv_label_create(loading_widget);
    lv_label_set_text(label, "无线麦克风");
    lv_obj_set_style_text_font(label, UI_FONT_DISPLAY, 0);
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 6, -7);

    label = lv_label_create(loading_widget);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "#626367 版本: %d.%d#", CONFIG_VERSION_VALUE >> 8, CONFIG_VERSION_VALUE & 0xff);
    lv_obj_set_style_text_font(label, UI_FONT_BODY, 0);
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 15, 10);

    bar = lv_bar_create(loading_widget);
    lv_obj_set_width(bar, lv_pct(85));
    lv_obj_set_height(bar, 6);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, 0);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2D6BDB), LV_PART_INDICATOR);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 9);

    pct = lv_label_create(loading_widget);
    lv_label_set_text(pct, "正在加载:screen");
    lv_obj_set_width(pct, 130);
    lv_obj_set_height(pct, lv_font_get_line_height(UI_FONT_BODY));
    lv_obj_set_style_text_align(pct, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(pct, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(pct, lv_color_hex(0x6B7280), 0);
    lv_obj_set_style_text_font(pct, UI_FONT_BODY, 0);
    lv_obj_align_to(pct, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    lv_timer_create(loding_timer_cb, 1, NULL); // 设置加载速率
    LV_LOG_INFO("timer已创建");
    // ui_popwin_load("正在连接中...");
    // ui_popwin_load("正在连接蓝牙...", ui_lottie_Bluetooth_connect_json, ui_lottie_Bluetooth_connect_json_len, 1000, 64, 64);
    // ui_popwin_finish("连接成功.", ui_lottie_Bluetooth_finish_json, ui_lottie_Bluetooth_finish_json_len, 1000, 64, 64);
    // ui_popwin_finish("连接失败!", ui_lottie_fail_json, ui_lottie_fail_json_len, 1000, 48, 48);
    // ui_popwin_msgbox("awa");
}
