#pragma once

#include "module/usb/usb_audio_format.h"

enum
{
  ITF_NUM_AUDIO_CONTROL = 0,
  ITF_NUM_AUDIO_STREAMING,
  ITF_NUM_CDC_CMD,
  ITF_NUM_CDC_DATA,
  ITF_NUM_TOTAL
};

enum { ITF_NUM_MSC = 0, ITF_NUM_MSC_TOTAL };

#define UAC2_ENTITY_INPUT_TERMINAL 0x01
#define UAC2_ENTITY_FEATURE_UNIT 0x02
#define UAC2_ENTITY_OUTPUT_TERMINAL 0x03
#define UAC2_ENTITY_CLOCK 0x04

namespace usb::descriptors
{
  // Call only with the stack stopped: Windows caches the descriptor by serial.
  void prepare(const AudioFormat &format);
}
