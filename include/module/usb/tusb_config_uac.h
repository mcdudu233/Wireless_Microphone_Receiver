#pragma once

// Actual channel count, sample width and endpoint size are in the runtime
// descriptor. The fixed allocation must fit the S3's shared 1 KB hardware FIFO.
#define CFG_TUD_AUDIO_ENABLE_EP_IN 1
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL 1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX 640
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ (16 * CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX)
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 1
