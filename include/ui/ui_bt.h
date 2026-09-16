#pragma once

#include "ui/ui.h"

#define BT_LINKED ((void *)1)
#define BT_UNLINKED ((void *)0)

// 选中/连接态使用 DESIGN.md 的柔和状态色，禁止高饱和调色板整行染色
#define COLOR_LINKED lv_color_hex(0xECFDF5)  // status-success-bg
#define COLOR_LINKING lv_color_hex(0xDBEAFE) // accent-surface
#define COLOR_SELECTED lv_color_hex(0xFFFBEB) // status-warning-bg
#define COLOR_NONE LV_PALETTE_NONE

// api
void ui_bt_update(const std::string &mac, bool is_link = false);
// 初始化设备页时填充已知设备;已连接设备不会重新广播,扫描无法再次发现
void ui_bt_seed_devices();

// 定义
bool ui_bt_unlink(const std::string &mac);
bool ui_bt_link(const std::string &mac);
void ui_bt_search();
void ui_bt_pause_search();
// WiFi模式且无BLE待迁移链路时为真:设备页点"完成"可直接进主界面,跳过协议切换
bool ui_bt_wifi_ready();
// 完成设备连接并切换到主界面(调用方需持有LVGL锁;供完成按钮与协议切换任务共用)
void ui_bt_finish_to_main();
