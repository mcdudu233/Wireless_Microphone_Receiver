#pragma once

#include "ui/ui.h"

struct device_data
{
    std::string name;
    bool is_link = false;
};

#define BT_LINKED ((void *)1)
#define BT_UNLINKED ((void *)0)

#define COLOR_LINKED lv_palette_main(LV_PALETTE_GREEN)
#define COLOR_SELECTED lv_palette_main(LV_PALETTE_RED)
#define COLOR_NONE std::nullopt

#define EVENT_UPDATE (lv_event_code_t)(LV_EVENT_LAST + 1)

// api
bool ui_unlink_bt(const char *device_name);
bool ui_link_bt(const char *device_name);
void ui_search_bt();
void ui_update_bt(device_data dev); // 调用
