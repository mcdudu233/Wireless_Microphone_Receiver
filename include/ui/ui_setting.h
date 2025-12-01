#pragma once

typedef enum
{
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
} lv_menu_builder_variant_t;

// 系统信息刷新周期 ms
#define SYSTEM_INFO_REFLUSH_TIME 1000

// 固件版本
#define VERSION "0.0.1"
#define EXTERNAL_LINK "https://www.github.com/"

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
// 传输模式 0:音频转换 1:usb
void ui_set_transmit_mode(const uint32_t &choice);
// 采样率 0:48000 1:96000 2:192000
void ui_set_sample_freq(const uint32_t &choice);
// 位深度 0:16 1:24 2:32
void ui_set_bit(const uint32_t &choice);
// 通道数 0:单通道 1:立体声
void ui_set_channel(const uint32_t &choice);
// 传输协议 0:BLE 1:UDP 2:TCP
void ui_set_transmit_protocol(const uint32_t &choice);