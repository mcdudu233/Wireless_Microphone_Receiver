#include "module/usb/usb_descriptors.h"
#include "module/usb/usb.h"
#include "esp_mac.h"
#include "tusb.h"
#include <cstdio>
#include <cstring>

static_assert(CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX == usb::AUDIO_EP_MAX_BYTES, "USB FIFO budget mismatch");

static const tusb_desc_device_t desc_device_audio = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, .idProduct = 0x8000, .bcdDevice = CONFIG_VERSION_BCD,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1};
static const tusb_desc_device_t desc_device_msc = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, .idProduct = 0x8001, .bcdDevice = CONFIG_VERSION_BCD,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1};

#define AUDIO_DESC_LEN(ch) (TUD_AUDIO20_DESC_IAD_LEN + TUD_AUDIO20_DESC_STD_AC_LEN + \
    TUD_AUDIO20_DESC_CS_AC_LEN + TUD_AUDIO20_DESC_CLK_SRC_LEN + TUD_AUDIO20_DESC_INPUT_TERM_LEN + \
    TUD_AUDIO20_DESC_OUTPUT_TERM_LEN + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(ch) + \
    2 * TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_CS_AS_INT_LEN + \
    TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)
#define AUDIO_CONFIG_LEN(ch) (TUD_CONFIG_DESC_LEN + AUDIO_DESC_LEN(ch) + TUD_CDC_DESC_LEN)
#define MUTE_CONTROL (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS)
#define AUDIO_DESCRIPTOR(ch, layout, ...) \
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, AUDIO_CONFIG_LEN(ch), 0, 500), \
    TUD_AUDIO20_DESC_IAD(ITF_NUM_AUDIO_CONTROL, 2, 4), \
    TUD_AUDIO20_DESC_STD_AC(ITF_NUM_AUDIO_CONTROL, 0, 4), \
    TUD_AUDIO20_DESC_CS_AC(0x0200, AUDIO20_FUNC_MICROPHONE, \
        TUD_AUDIO20_DESC_CLK_SRC_LEN + TUD_AUDIO20_DESC_INPUT_TERM_LEN + \
        TUD_AUDIO20_DESC_OUTPUT_TERM_LEN + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(ch), 0), \
    TUD_AUDIO20_DESC_CLK_SRC(UAC2_ENTITY_CLOCK, AUDIO20_CLOCK_SOURCE_ATT_INT_FIX_CLK, \
        AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS | \
        AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_VAL_POS, 0, 0), \
    TUD_AUDIO20_DESC_INPUT_TERM(UAC2_ENTITY_INPUT_TERMINAL, AUDIO_TERM_TYPE_IN_GENERIC_MIC, 0, \
        UAC2_ENTITY_CLOCK, ch, layout, 0, 0, 0), \
    TUD_AUDIO20_DESC_OUTPUT_TERM(UAC2_ENTITY_OUTPUT_TERMINAL, AUDIO_TERM_TYPE_USB_STREAMING, 0, \
        UAC2_ENTITY_FEATURE_UNIT, UAC2_ENTITY_CLOCK, 0, 0), \
    TUD_AUDIO20_DESC_FEATURE_UNIT(UAC2_ENTITY_FEATURE_UNIT, UAC2_ENTITY_INPUT_TERMINAL, 0, __VA_ARGS__), \
    TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING, 0, 0, 0), \
    TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING, 1, 1, 4), \
    TUD_AUDIO20_DESC_CS_AS_INT(UAC2_ENTITY_OUTPUT_TERMINAL, 0, AUDIO20_FORMAT_TYPE_I, \
        AUDIO20_DATA_FORMAT_TYPE_I_PCM, ch, layout, 0), \
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(static_cast<uint8_t>(format.bit / 8), static_cast<uint8_t>(format.bit)), \
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(0x83, static_cast<uint8_t>(TUSB_XFER_ISOCHRONOUS) | static_cast<uint8_t>(TUSB_ISO_EP_ATT_ASYNCHRONOUS), \
        format.packet_bytes(), 1), \
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, 0, 0, 0), \
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_CMD, 5, 0x81, 8, 0x02, 0x82, 64)

static uint8_t desc_configuration_audio[AUDIO_CONFIG_LEN(2)];
static const uint8_t desc_configuration_msc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_MSC_TOTAL, 0, TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN, 0, 500),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 6, 0x01, 0x81, 64)};
static char audio_serial[32];
static char msc_serial[32];

void usb::descriptors::prepare(const AudioFormat &format)
{
    if (format.channels == AUDIO_CHANNEL_SINGLE)
    {
        const uint8_t desc[] = {AUDIO_DESCRIPTOR(1, AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, MUTE_CONTROL, MUTE_CONTROL)};
        static_assert(sizeof(desc) == AUDIO_CONFIG_LEN(1), "Mono descriptor length mismatch");
        memcpy(desc_configuration_audio, desc, sizeof(desc));
    }
    else
    {
        const uint8_t desc[] = {AUDIO_DESCRIPTOR(2, AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT,
                                               MUTE_CONTROL, MUTE_CONTROL, MUTE_CONTROL)};
        static_assert(sizeof(desc) == AUDIO_CONFIG_LEN(2), "Stereo descriptor length mismatch");
        memcpy(desc_configuration_audio, desc, sizeof(desc));
    }
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);
    char id[13];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(audio_serial, sizeof(audio_serial), "%s-A%lu-%u-%u", id,
             static_cast<unsigned long>(format.rate / 1000), static_cast<unsigned>(format.bit), static_cast<unsigned>(format.channels));
    snprintf(msc_serial, sizeof(msc_serial), "%s-SD", id);
}

uint8_t const *tud_descriptor_device_cb()
{
    return reinterpret_cast<const uint8_t *>(usb::active_mode() == USB_MODE_SD ? &desc_device_msc : &desc_device_audio);
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    if (index != 0)
        return nullptr;
    return usb::active_mode() == USB_MODE_SD ? desc_configuration_msc : desc_configuration_audio;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t)
{
    static uint16_t desc[32];
    static const char *const strings[] = {nullptr, "dudu233", "Wireless Microphone", nullptr,
        "WirelessMicrophone Audio (UAC)", "WirelessMicrophone UART (CDC)", "WirelessMicrophone TF (MSC)"};
    if (index == 0)
    {
        desc[0] = (TUSB_DESC_STRING << 8) | 4;
        desc[1] = 0x0409;
        return desc;
    }
    if (index >= TU_ARRAY_SIZE(strings))
        return nullptr;
    const bool storage = usb::active_mode() == USB_MODE_SD;
    const char *str = index == 3 ? (storage ? msc_serial : audio_serial) :
                      index == 2 && storage ? "Wireless Microphone TF Card" : strings[index];
    const size_t count = strlen(str) < 31 ? strlen(str) : 31;
    for (size_t i = 0; i < count; ++i)
        desc[i + 1] = static_cast<uint8_t>(str[i]);
    desc[0] = (TUSB_DESC_STRING << 8) | (2 * count + 2);
    return desc;
}
