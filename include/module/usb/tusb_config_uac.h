/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "usb_descriptors.h"

// 最高频率
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE 192000
#define CFG_TUD_AUDIO_FUNC_1_SUBMAX_SAMPLE_RATE 96000 // 全速模式的32bit和16bit最大支持不到192khz

// bit格式
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS 3
// 16bit in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX 2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX 16
// 24bit in 24bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_N_BYTES_PER_SAMPLE_TX 3
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_RESOLUTION_RX 24
// 32bit in 32bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_3_N_BYTES_PER_SAMPLE_TX 4
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_3_RESOLUTION_RX 32

// 通道数
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX 2

// 计算缓存大小
#define CFG_TUD_AUDIO_EP_IN_SW_BUF_SZ_CHUNK 8 // 块大小 4ms传输一次那么最小为4
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_EP_SZ_IN TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_SUBMAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_FORMAT_2_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_3_EP_SZ_IN TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_SUBMAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_FORMAT_3_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)

/******************************************* TinyUSB 配置区 *******************************************/
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN TUD_AUDIO_MIC_TWO_CH_DESC_LEN // 描述符长度
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 1                             // 配置AS接口数量
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64                         // 控制块的缓冲大小

#define CFG_TUD_AUDIO_ENABLE_EP_IN 1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX TU_MAX(TU_MAX(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN, CFG_TUD_AUDIO_FUNC_1_FORMAT_2_EP_SZ_IN), CFG_TUD_AUDIO_FUNC_1_FORMAT_3_EP_SZ_IN) // 硬件最大发送大小
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ (CFG_TUD_AUDIO_EP_IN_SW_BUF_SZ_CHUNK * CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX)                                                           // 软件缓冲大小
  /******************************************************************************************************/

#ifdef __cplusplus
}
#endif
