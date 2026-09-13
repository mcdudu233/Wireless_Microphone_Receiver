#pragma once
#include "config.h"

// 系统信息刷新周期 ms
#define SYSTEM_INFO_REFLUSH_TIME 1000
#define AUTHOR "dudu233, tiosa"
#define WEBSITE "www.mcso.top/mic"

// 获取系统信息调用
void ui_setting_system_page_rcb(float &cpu1_pct, float &cpu2_pct, size_t &iram_current, size_t &psram_current, size_t &l_iram_max, size_t &l_psram_max);

// 更新新的配置时调用

// 音频输入页面读取、保存并立即生效回调
void ui_setting_audio_input_page_rcb(AudioBit &bit, AudioChannel &channel, AudioRate &rate, AudioGain &gain, AudioMode &mode);
void ui_setting_audio_input_page_scb(AudioBit bit, AudioChannel channel, AudioRate rate, AudioGain gain, AudioMode mode);

// 音频输出页面读取、保存并立即生效回调
void ui_setting_audio_output_page_rcb(bool &enabled, AudioOutputMode &mode);
void ui_setting_audio_output_page_scb(bool enabled, AudioOutputMode mode);

// 屏幕页面读取、保存并立即生效回调
void ui_setting_screen_page_rcb(uint8_t &brightness, ScreenTimeout &timeout);
void ui_setting_screen_page_scb(uint8_t brightness, ScreenTimeout timeout);

void ui_setting_usb_page_rcb(USBMode &mode);
void ui_setting_usb_page_scb(USBMode mode);

void ui_setting_rf_page_rcb(RFMode &mode);
void ui_setting_rf_page_scb(RFMode mode);
