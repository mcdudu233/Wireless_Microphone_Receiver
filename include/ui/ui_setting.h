#pragma once
#include "config.h"
typedef enum
{
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
} lv_menu_builder_variant_t;

// 系统信息刷新周期 ms
#define SYSTEM_INFO_REFLUSH_TIME 1000
#define AUTHOR "awa"
#define WEBSITE "https://www.github.com"
// api
namespace SystemInfo
{
    extern uint8_t cpu1;
    extern uint8_t cpu2;
    extern uint64_t IRAM;
    extern uint64_t PSRAM;
    extern uint64_t SD;
}

// 定义

// 更新新的配置时调用
void ui_update_config();