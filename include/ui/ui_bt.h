#pragma once

#include "ui/ui.h"

struct device_data
{
    std::string name;
    bool is_link = false;
};

static lv_obj_t *bt_list;
static lv_obj_t *bt_widget;
static lv_group_t *g1; // list 俩个按钮
static lv_group_t *g2; // list内部设备选项

static void bt_list_click_event_cb(lv_event_t *e); // bt_list 被点击
static void list_event_handler(lv_event_t *e);     // bt_list 项被点击
static void bt_list_update_event_cb(lv_event_t *e);

#define BT_LINKED ((void *)1)
#define BT_UNLINKED ((void *)0)

#define COLOR_LINKED lv_palette_main(LV_PALETTE_GREEN)
#define COLOR_SELECTED lv_palette_main(LV_PALETTE_RED)
#define COLOR_NONE std::nullopt

#define EVENT_UPDATE (lv_event_code_t)(LV_EVENT_LAST + 1)



bool unlink_bt(const char *device_name);
bool link_bt(const char *device_name);
static void search_bt();

// api
void update_bt(device_data dev);
