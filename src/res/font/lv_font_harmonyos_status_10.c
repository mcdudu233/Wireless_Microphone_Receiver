/*******************************************************************************
 * Size: 10 px
 * Bpp: 2
 * Opts: --font docs/resources/fonts/HarmonyOS_Sans_SC_Regular.ttf --size 10 --bpp 2 --no-compress --range 32-127 --format lvgl --lv-font-name lv_font_harmonyos_status_10 -o Receiver/src/res/font/lv_font_harmonyos_status_10.c
 ******************************************************************************/

#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_HARMONYOS_STATUS_10
#define LV_FONT_HARMONYOS_STATUS_10 1
#endif

#if LV_FONT_HARMONYOS_STATUS_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x62, 0x22, 0x20, 0x60,

    /* U+0022 "\"" */
    0x1, 0x65, 0x80,

    /* U+0023 "#" */
    0x8, 0x20, 0x31, 0x42, 0xef, 0x5, 0x30, 0xbb,
    0xd0, 0xc9, 0x2, 0x20, 0x0,

    /* U+0024 "$" */
    0x0, 0x0, 0x90, 0x3f, 0xc5, 0x54, 0x39, 0x0,
    0xb8, 0x5, 0x96, 0x9c, 0x1f, 0x40, 0x50,

    /* U+0025 "%" */
    0x68, 0x20, 0x85, 0x60, 0x68, 0xc0, 0x2, 0x40,
    0x2, 0x28, 0xc, 0x85, 0x14, 0x68,

    /* U+0026 "&" */
    0xb, 0x40, 0x24, 0xc0, 0xe, 0x40, 0x2b, 0x24,
    0x61, 0xf0, 0x60, 0xb0, 0x2a, 0x58,

    /* U+0027 "'" */
    0x5, 0x50,

    /* U+0028 "(" */
    0x4, 0x18, 0x30, 0x20, 0x60, 0x20, 0x30, 0x20,
    0x8,

    /* U+0029 ")" */
    0x40, 0x82, 0x46, 0x8, 0x21, 0x4c, 0x50,

    /* U+002A "*" */
    0x4, 0x3d, 0x6a, 0x4,

    /* U+002B "+" */
    0x0, 0x0, 0x80, 0x6f, 0x80, 0x80, 0x8, 0x0,

    /* U+002C "," */
    0x3, 0x20,

    /* U+002D "-" */
    0x6a, 0x0,

    /* U+002E "." */
    0x6,

    /* U+002F "/" */
    0x6, 0x8, 0xc, 0x14, 0x30, 0x60, 0x80,

    /* U+0030 "0" */
    0x1f, 0x3, 0xc, 0x60, 0xc5, 0x8, 0x60, 0xc3,
    0xc, 0x1f, 0x0,

    /* U+0031 "1" */
    0xb, 0x37, 0x3, 0x3, 0x3, 0x3, 0x3,

    /* U+0032 "2" */
    0x2f, 0x42, 0xc, 0x0, 0xc0, 0x24, 0xa, 0x2,
    0x80, 0x7f, 0xc0,

    /* U+0033 "3" */
    0x2f, 0x42, 0xc, 0x1, 0xc0, 0xb4, 0x0, 0xc5,
    0xc, 0x2f, 0x40,

    /* U+0034 "4" */
    0x3, 0x0, 0x90, 0xc, 0x2, 0x58, 0x21, 0x8b,
    0xbd, 0x1, 0x80,

    /* U+0035 "5" */
    0x2e, 0x83, 0x0, 0x3e, 0x1, 0xc, 0x0, 0xc2,
    0xc, 0x2f, 0x0,

    /* U+0036 "6" */
    0x2, 0x0, 0x80, 0x1f, 0x43, 0xc, 0x60, 0x96,
    0xc, 0x2f, 0x40,

    /* U+0037 "7" */
    0x6a, 0xc0, 0x8, 0x2, 0x40, 0x30, 0x9, 0x0,
    0xc0, 0x18, 0x0,

    /* U+0038 "8" */
    0x1f, 0x43, 0xc, 0x30, 0xc2, 0xf4, 0x60, 0xc6,
    0xc, 0x2f, 0x40,

    /* U+0039 "9" */
    0x2f, 0x46, 0xc, 0x90, 0xc6, 0xc, 0x1f, 0x0,
    0x60, 0xc, 0x0,

    /* U+003A ":" */
    0x20, 0x0, 0x20,

    /* U+003B ";" */
    0x30, 0x0, 0x32, 0x0,

    /* U+003C "<" */
    0x0, 0x40, 0xb4, 0x74, 0x1, 0xa4, 0x0, 0x80,

    /* U+003D "=" */
    0x6a, 0x80, 0x0, 0x6a, 0x80,

    /* U+003E ">" */
    0x40, 0x2, 0xd0, 0x2, 0xc1, 0xa0, 0x50, 0x0,

    /* U+003F "?" */
    0xb, 0x42, 0xc, 0x0, 0x80, 0x20, 0x5, 0x0,
    0x0, 0x6, 0x0,

    /* U+0040 "@" */
    0x2, 0xa8, 0x1, 0x80, 0x24, 0x31, 0xe8, 0xc5,
    0x30, 0xc9, 0x56, 0xc, 0x55, 0x30, 0xc8, 0x31,
    0xa6, 0x81, 0x80, 0x0, 0x2, 0xa8, 0x0,

    /* U+0041 "A" */
    0x7, 0x0, 0x29, 0x0, 0x88, 0x9, 0x30, 0x3f,
    0xe1, 0x80, 0xc8, 0x2, 0x40,

    /* U+0042 "B" */
    0x3f, 0x80, 0xc1, 0x83, 0x6, 0xe, 0xf4, 0x30,
    0x30, 0xc0, 0xc3, 0xad, 0x0,

    /* U+0043 "C" */
    0xb, 0xd0, 0xc0, 0x86, 0x0, 0x14, 0x0, 0x60,
    0x0, 0xc0, 0x80, 0xbd, 0x0,

    /* U+0044 "D" */
    0x3b, 0x80, 0xc0, 0xc3, 0x2, 0x4c, 0x6, 0x30,
    0x24, 0xc0, 0xc3, 0xf8, 0x0,

    /* U+0045 "E" */
    0x3f, 0xd3, 0x0, 0x30, 0x3, 0xf8, 0x30, 0x3,
    0x0, 0x3f, 0xd0,

    /* U+0046 "F" */
    0x3f, 0xd3, 0x0, 0x30, 0x3, 0xf8, 0x30, 0x3,
    0x0, 0x30, 0x0,

    /* U+0047 "G" */
    0xb, 0xe0, 0xc0, 0x46, 0x0, 0x14, 0x2d, 0x60,
    0x18, 0xd0, 0x60, 0xbe, 0x0,

    /* U+0048 "H" */
    0x30, 0x18, 0xc0, 0x63, 0x1, 0x8f, 0xfe, 0x30,
    0x18, 0xc0, 0x63, 0x1, 0x80,

    /* U+0049 "I" */
    0x33, 0x33, 0x33, 0x30,

    /* U+004A "J" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x46, 0xbc,

    /* U+004B "K" */
    0x30, 0x30, 0xc3, 0x3, 0x30, 0xf, 0xc0, 0x31,
    0x80, 0xc2, 0x83, 0x3, 0x40,

    /* U+004C "L" */
    0x30, 0x3, 0x0, 0x30, 0x3, 0x0, 0x30, 0x3,
    0x0, 0x3f, 0xd0,

    /* U+004D "M" */
    0x30, 0x7, 0x38, 0xf, 0x29, 0x27, 0x23, 0x63,
    0x21, 0xc3, 0x20, 0x3, 0x20, 0x3,

    /* U+004E "N" */
    0x30, 0x18, 0xe0, 0x62, 0x91, 0x88, 0xc6, 0x20,
    0xd8, 0x81, 0xe2, 0x2, 0x80,

    /* U+004F "O" */
    0xb, 0xe0, 0x30, 0x28, 0x60, 0xc, 0x50, 0xc,
    0x60, 0xc, 0x30, 0x28, 0xb, 0xe0,

    /* U+0050 "P" */
    0x3b, 0x83, 0x9, 0x30, 0x63, 0x9, 0x3b, 0x43,
    0x0, 0x30, 0x0,

    /* U+0051 "Q" */
    0xb, 0xe0, 0x30, 0x28, 0x60, 0xc, 0x50, 0xc,
    0x60, 0xc, 0x30, 0x28, 0xb, 0xe0, 0x0, 0x34,
    0x0, 0x4,

    /* U+0052 "R" */
    0x3a, 0x80, 0xc1, 0x83, 0xa, 0xe, 0xe0, 0x31,
    0x80, 0xc2, 0x43, 0x3, 0x0,

    /* U+0053 "S" */
    0x2f, 0x86, 0x8, 0x70, 0x1, 0xb4, 0x0, 0x99,
    0x9, 0x2f, 0x80,

    /* U+0054 "T" */
    0xbf, 0xd0, 0x90, 0x9, 0x0, 0x90, 0x9, 0x0,
    0x90, 0x9, 0x0,

    /* U+0055 "U" */
    0x20, 0x18, 0x80, 0x62, 0x1, 0x88, 0x6, 0x30,
    0x14, 0xc0, 0xc0, 0xbd, 0x0,

    /* U+0056 "V" */
    0x90, 0x25, 0x80, 0xc3, 0x6, 0x5, 0x20, 0xc,
    0x80, 0x29, 0x0, 0x30, 0x0,

    /* U+0057 "W" */
    0x80, 0x90, 0x56, 0xe, 0x8, 0x31, 0xb0, 0xc2,
    0x22, 0x54, 0x17, 0xa, 0x0, 0xe0, 0xf0, 0x9,
    0xa, 0x0,

    /* U+0058 "X" */
    0x60, 0x30, 0x92, 0x40, 0xe8, 0x1, 0xc0, 0xe,
    0x80, 0x92, 0x4a, 0x3, 0x0,

    /* U+0059 "Y" */
    0x90, 0x30, 0xc3, 0x1, 0xd8, 0x2, 0xc0, 0x6,
    0x0, 0x18, 0x0, 0x60, 0x0,

    /* U+005A "Z" */
    0x7f, 0xd0, 0x18, 0x3, 0x0, 0x90, 0x18, 0x3,
    0x0, 0xbf, 0xd0,

    /* U+005B "[" */
    0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x38,

    /* U+005C "\\" */
    0x80, 0x60, 0x30, 0x14, 0xc, 0x8, 0x6,

    /* U+005D "]" */
    0xb4, 0x61, 0x86, 0x18, 0x61, 0x86, 0xb4,

    /* U+005E "^" */
    0x4, 0x7, 0x3, 0x21, 0x48,

    /* U+005F "_" */
    0xaa, 0x0,

    /* U+0060 "`" */
    0x10, 0x80,

    /* U+0061 "a" */
    0x2f, 0x4, 0x22, 0xad, 0x83, 0x2a, 0xc0,

    /* U+0062 "b" */
    0x20, 0x2, 0x0, 0x2b, 0x83, 0x9, 0x20, 0x63,
    0x9, 0x2b, 0x80,

    /* U+0063 "c" */
    0x2f, 0x58, 0x9, 0x1, 0x80, 0x1f, 0x40,

    /* U+0064 "d" */
    0x0, 0x90, 0x9, 0x2e, 0x96, 0xd, 0x90, 0x96,
    0xd, 0x2a, 0x90,

    /* U+0065 "e" */
    0x2f, 0x46, 0xc, 0xba, 0xc6, 0x0, 0x2e, 0x40,

    /* U+0066 "f" */
    0x1d, 0x30, 0xb8, 0x30, 0x30, 0x30, 0x30,

    /* U+0067 "g" */
    0x2e, 0x96, 0xd, 0x90, 0x96, 0xd, 0x2e, 0x90,
    0xc, 0x2f, 0x40,

    /* U+0068 "h" */
    0x20, 0x2, 0x0, 0x2b, 0x43, 0xc, 0x20, 0x82,
    0x8, 0x20, 0x80,

    /* U+0069 "i" */
    0x20, 0x22, 0x22, 0x20,

    /* U+006A "j" */
    0x2, 0x0, 0x2, 0x2, 0x2, 0x2, 0x2, 0x6,
    0x3c,

    /* U+006B "k" */
    0x20, 0x2, 0x0, 0x21, 0x82, 0x60, 0x3d, 0x3,
    0x30, 0x20, 0xc0,

    /* U+006C "l" */
    0x22, 0x22, 0x22, 0x20,

    /* U+006D "m" */
    0x2b, 0x6d, 0x30, 0xc3, 0x20, 0x83, 0x20, 0x83,
    0x20, 0x83,

    /* U+006E "n" */
    0x2a, 0x43, 0xc, 0x20, 0x82, 0x8, 0x20, 0x80,

    /* U+006F "o" */
    0x1f, 0x46, 0xc, 0x90, 0x96, 0xc, 0x1f, 0x40,

    /* U+0070 "p" */
    0x2a, 0x83, 0x9, 0x20, 0x63, 0x9, 0x2b, 0x82,
    0x0, 0x20, 0x0,

    /* U+0071 "q" */
    0x2e, 0x96, 0xd, 0x90, 0x96, 0xd, 0x2e, 0x90,
    0x9, 0x0, 0x90,

    /* U+0072 "r" */
    0x29, 0x30, 0x20, 0x20, 0x20,

    /* U+0073 "s" */
    0x2e, 0x18, 0x2, 0xa1, 0x8, 0x3e, 0x0,

    /* U+0074 "t" */
    0x10, 0x30, 0xb8, 0x30, 0x30, 0x30, 0x2d,

    /* U+0075 "u" */
    0x60, 0xc6, 0xc, 0x60, 0xc2, 0xc, 0x2a, 0xc0,

    /* U+0076 "v" */
    0x90, 0xc6, 0x14, 0x32, 0x1, 0xa0, 0xc, 0x0,

    /* U+0077 "w" */
    0x82, 0x46, 0x63, 0xc8, 0x31, 0x88, 0x2c, 0x74,
    0xc, 0x30,

    /* U+0078 "x" */
    0xa1, 0x4a, 0x80, 0xc0, 0x98, 0x92, 0x40,

    /* U+0079 "y" */
    0x90, 0xc6, 0x14, 0x33, 0x1, 0xa0, 0xc, 0x0,
    0x80, 0xb0, 0x0,

    /* U+007A "z" */
    0x6b, 0x1, 0x80, 0xc0, 0xc0, 0xba, 0x40,

    /* U+007B "{" */
    0x9, 0x24, 0x24, 0x24, 0xb0, 0x24, 0x24, 0x24,
    0x9,

    /* U+007C "|" */
    0x5, 0x55, 0x55, 0x55, 0x50,

    /* U+007D "}" */
    0xa0, 0x20, 0x20, 0x24, 0xd, 0x24, 0x20, 0x20,
    0xa0,

    /* U+007E "~" */
    0x28, 0x85, 0x28
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 43, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 38, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 55, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 7, .adv_w = 103, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 91, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 35, .adv_w = 123, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 49, .adv_w = 113, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 30, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 65, .adv_w = 55, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 74, .adv_w = 55, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 81, .adv_w = 69, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 85, .adv_w = 91, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 93, .adv_w = 39, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 95, .adv_w = 78, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 97, .adv_w = 37, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 62, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 105, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 91, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 211, .adv_w = 41, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 42, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 218, .adv_w = 91, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 226, .adv_w = 91, .box_w = 6, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 231, .adv_w = 91, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 239, .adv_w = 69, .box_w = 6, .box_h = 7, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 156, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 273, .adv_w = 106, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 286, .adv_w = 104, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 299, .adv_w = 104, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 312, .adv_w = 114, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 325, .adv_w = 95, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 336, .adv_w = 90, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 112, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 119, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 373, .adv_w = 42, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 377, .adv_w = 73, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 108, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 397, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 408, .adv_w = 138, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 118, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 122, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 449, .adv_w = 95, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 460, .adv_w = 122, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 478, .adv_w = 103, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 491, .adv_w = 92, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 502, .adv_w = 91, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 513, .adv_w = 117, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 526, .adv_w = 105, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 539, .adv_w = 155, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 557, .adv_w = 105, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 570, .adv_w = 99, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 583, .adv_w = 92, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 594, .adv_w = 55, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 603, .adv_w = 62, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 610, .adv_w = 55, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 617, .adv_w = 76, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 622, .adv_w = 66, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 624, .adv_w = 48, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 626, .adv_w = 87, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 633, .adv_w = 97, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 644, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 651, .adv_w = 97, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 662, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 670, .adv_w = 55, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 677, .adv_w = 97, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 688, .adv_w = 93, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 699, .adv_w = 39, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 703, .adv_w = 38, .box_w = 4, .box_h = 9, .ofs_x = -2, .ofs_y = -2},
    {.bitmap_index = 712, .adv_w = 84, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 723, .adv_w = 38, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 727, .adv_w = 137, .box_w = 8, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 737, .adv_w = 93, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 745, .adv_w = 95, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 753, .adv_w = 97, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 764, .adv_w = 97, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 775, .adv_w = 60, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 780, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 787, .adv_w = 59, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 794, .adv_w = 93, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 802, .adv_w = 82, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 810, .adv_w = 125, .box_w = 8, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 820, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 827, .adv_w = 83, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 838, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 845, .adv_w = 59, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 854, .adv_w = 29, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 859, .adv_w = 59, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 868, .adv_w = 91, .box_w = 6, .box_h = 2, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 0, 1, 0, 0, 0, 0,
    1, 2, 3, 0, 4, 0, 4, 0,
    5, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 4,
    6, 7, 8, 9, 10, 7, 11, 12,
    13, 14, 14, 15, 16, 17, 14, 14,
    7, 18, 0, 19, 20, 21, 15, 5,
    22, 23, 24, 25, 2, 8, 3, 0,
    0, 0, 26, 27, 28, 29, 30, 31,
    32, 26, 0, 33, 34, 29, 26, 26,
    27, 27, 0, 35, 36, 37, 32, 38,
    38, 39, 38, 40, 2, 0, 3, 4
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 1, 0, 2, 0, 0, 0, 0,
    2, 0, 3, 0, 4, 5, 4, 5,
    6, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 0,
    7, 8, 6, 9, 8, 9, 9, 9,
    8, 9, 9, 10, 9, 9, 9, 9,
    8, 9, 8, 9, 11, 12, 13, 14,
    15, 16, 17, 18, 0, 14, 3, 0,
    5, 0, 19, 20, 21, 21, 21, 22,
    21, 20, 0, 23, 20, 20, 24, 24,
    21, 0, 21, 24, 25, 26, 27, 28,
    28, 29, 28, 30, 0, 0, 3, 4
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 0, 0, 0, 0, 0, 2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 3, 0, 3, 3, 2,
    0, 2, 0, 0, 11, 0, 0, 4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 5, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -8, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -6, 3, 3, -6, -22, -14, 4, -5,
    0, -18, -1, 3, 0, 0, 0, 0,
    0, 0, -11, 0, -11, -3, 0, -6,
    -8, 0, -7, -6, -8, -8, 0, 0,
    0, -5, -15, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, 0,
    -7, -5, 0, 0, 0, -6, 0, -5,
    0, -5, -3, -5, -8, -4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -6, -19, 0, -10, 5, 0,
    -11, -5, 0, 0, 0, -13, -2, -14,
    -10, 0, -17, 3, 0, 0, -2, 0,
    0, 0, 0, 0, 0, -6, 0, 0,
    0, -2, 0, 0, 0, -2, 0, 0,
    0, 2, 0, -6, 0, -7, -2, 0,
    -8, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 2,
    0, -5, 4, 0, 5, -2, 0, 0,
    0, 1, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -4, 0, 0, 0, 0, 0, 0,
    2, 0, 1, -2, 0, 2, 0, 0,
    0, -2, 0, 0, -2, 0, -2, 0,
    -2, -3, 0, 0, -1, -2, -2, -3,
    -1, 0, -2, 4, 0, 1, -21, -9,
    6, -1, 0, -23, 0, 3, 0, 0,
    0, 0, 0, 0, -7, 0, -5, -2,
    0, -3, 0, -2, 0, -3, -6, -4,
    0, 0, 0, 0, 3, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -5, -2, 0, 0, 0, -5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -6, 0, 0, -18, 3, 0,
    0, -9, -2, 0, -2, 0, -4, 0,
    0, 0, 0, 0, -4, 0, -5, -6,
    0, -2, -2, -5, -6, -10, -5, 0,
    -8, -16, 0, -14, 5, 0, -11, -8,
    0, 3, -2, -20, -7, -24, -17, 0,
    -28, 0, -1, 0, -4, -3, 0, 0,
    0, -4, -5, -15, 0, 0, -2, 2,
    0, 2, -23, -13, 2, 0, 0, -25,
    0, 0, 0, 0, 0, -3, 0, -5,
    -5, 0, -5, 0, 0, 0, 0, 0,
    0, 2, 0, 0, 0, -1, 0, -2,
    7, 0, -1, -2, 0, 0, 1, -1,
    -2, -4, -2, 0, -7, 0, 0, 0,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 4, 0, 0, -2,
    0, 0, -3, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -5, 5, 0, -13, -18, -13, 5, -5,
    0, -22, 0, 3, 0, 3, 3, 0,
    0, 0, -18, 0, -18, -8, 0, -14,
    -18, -5, -13, -17, -16, -14, -2, 3,
    0, -3, -12, -10, 0, -3, 0, -11,
    0, 3, 0, 0, 0, 0, 0, 0,
    -12, 0, -10, -2, 0, -6, -6, 0,
    -4, -3, -5, -5, 0, 0, 3, -14,
    2, 0, 2, -5, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    -5, 0, 0, -3, -3, -4, -4, -9,
    0, 0, -5, 2, 3, -11, -21, -17,
    1, -8, 0, -21, -3, 0, 0, 0,
    0, 0, 0, 0, -17, 0, -16, -8,
    0, -13, -14, -5, -11, -11, -10, -12,
    0, 0, 2, -8, 3, 0, 1, -5,
    0, 0, -2, 0, 0, 0, 0, 0,
    0, 0, -1, 0, -2, 0, 0, 0,
    0, 0, 0, -5, 0, 0, 0, -7,
    0, 0, 0, 0, -4, 0, 0, 0,
    0, -13, 0, -11, -10, -1, -15, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -1, 0, 0, 0, -8, 0, -2,
    -5, 0, -6, 0, 0, 0, 0, -18,
    0, -11, -10, -5, -16, 0, -2, 0,
    0, -1, 0, 0, 0, -1, 0, -3,
    -4, -4, 0, 0, 0, 3, 4, 0,
    -2, 0, 0, 0, 0, -12, 0, -7,
    -5, 3, -11, 0, 0, 0, -1, 2,
    0, 0, 0, 3, 0, 0, 1, 2,
    0, 0, 2, 0, 0, 0, 2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 0, 0, -4, 0, 0, 0,
    0, -11, 0, -8, -7, -2, -13, 0,
    0, 0, 0, 0, 0, 0, 2, 0,
    0, 0, -1, 0, 0, 6, 0, -2,
    -12, 0, 7, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    -4, 0, 0, 0, 0, 2, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, -14, 0, -6,
    -6, 0, -13, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 5, 0,
    0, 0, 0, 0, 0, 0, -3, 0,
    0, -4, 4, 0, -7, 0, 0, 0,
    0, -15, 0, -9, -8, 0, -13, 0,
    -5, 0, -4, 0, 0, 0, -2, 0,
    -1, 0, 0, 0, 0, 3, 0, 1,
    -16, -6, -5, 0, 0, -17, 0, 0,
    0, -5, 0, -7, -11, 0, -7, 0,
    -5, 0, 0, 0, 0, 4, 0, 0,
    0, 0, 0, -5, 0, 0, 0, 0,
    -5, 0, 0, 0, 0, -16, 0, -11,
    -8, 0, -16, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, -1, 0, 2,
    0, -1, 1, 0, 4, 0, -4, 0,
    0, 0, 0, -12, 0, -7, 0, 0,
    -10, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -14, -6, -4, 0, 0, -12,
    0, -17, 0, -6, -3, -9, -11, 0,
    -3, 0, -3, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, -3,
    2, 0, -7, 0, 0, 0, 0, -16,
    0, -8, -5, 0, -10, 0, -2, 0,
    -4, 0, 0, 0, 0, 2, 0, 0,
    0, 0, 0, 0, 0, 0, 2, 0,
    -5, 0, 0, 0, 0, -16, 0, -8,
    -4, 0, -11, 0, -3, 0, -4, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 40,
    .right_class_cnt     = 30,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 2,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_harmonyos_status_10 = {
#else
lv_font_t lv_font_harmonyos_status_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 11,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_HARMONYOS_STATUS_10*/
