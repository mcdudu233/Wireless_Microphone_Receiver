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

// 定义
bool ui_bt_unlink(const std::string &mac);
bool ui_bt_link(const std::string &mac);
void ui_bt_search();
void ui_bt_pause_search();
// 完成设备连接并切换到主界面(调用方需持有LVGL锁;供完成按钮与协议切换任务共用)
void ui_bt_finish_to_main();