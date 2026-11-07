/*******************************************************************************
 * Size: 7 px
 * Bpp: 4
 * Opts: --font ttf/JetBrainsMono-Regular.ttf -r 0x20-0x7E,0xB0-0xB3,0xB5,0xB7,0xD7,0xD8,0xF8,0x401,0x410-0x44F,0x451,0x2013,0x2014,0x201C,0x201E,0x2026,0x2033,0x2212 --size 7 --bpp 4 --format lvgl --no-compress --lv-include lvgl.h --lv-font-name font_num_sm_t10 -o gen/font_num_sm_t10.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef FONT_NUM_SM_T10
#define FONT_NUM_SM_T10 1
#endif

#if FONT_NUM_SM_T10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x47, 0x36, 0x36, 0x24, 0x46,

    /* U+0022 "\"" */
    0xb8, 0x3a, 0x72, 0x11, 0x0,

    /* U+0023 "#" */
    0x7, 0x7, 0x3b, 0x99, 0x7, 0x52, 0x6a, 0xb5,
    0x52, 0x70,

    /* U+0024 "$" */
    0x2, 0x60, 0x2a, 0xa6, 0x3a, 0x50, 0x5, 0xd4,
    0x33, 0x6a, 0x2b, 0xc4, 0x1, 0x50,

    /* U+0025 "%" */
    0x99, 0x8, 0x6, 0x76, 0x10, 0x3, 0xb7, 0x0,
    0x78, 0x71, 0x71, 0x79, 0x0,

    /* U+0026 "&" */
    0x1a, 0xa0, 0x1, 0x82, 0x10, 0x5c, 0x46, 0xa,
    0x9, 0x70, 0x79, 0x79, 0x0,

    /* U+0027 "'" */
    0x37, 0x36, 0x1,

    /* U+0028 "(" */
    0x8, 0x46, 0x40, 0xa0, 0xa, 0x0, 0x90, 0x5,
    0x60, 0x7, 0x40,

    /* U+0029 ")" */
    0x19, 0x10, 0x29, 0x0, 0xa0, 0xa, 0x0, 0xa0,
    0x38, 0x18, 0x0,

    /* U+002A "*" */
    0x2, 0x60, 0x6a, 0xb8, 0x9, 0x90, 0x14, 0x22,

    /* U+002B "+" */
    0x3, 0x60, 0x4a, 0xb6, 0x3, 0x60, 0x1, 0x20,

    /* U+002C "," */
    0x1, 0x47, 0x73,

    /* U+002D "-" */
    0x9, 0x91,

    /* U+002E "." */
    0x0, 0x58,

    /* U+002F "/" */
    0x0, 0x4, 0x0, 0x54, 0x0, 0xa0, 0x2, 0x80,
    0x8, 0x20, 0xa, 0x0, 0x46, 0x0,

    /* U+0030 "0" */
    0x2a, 0xa4, 0x63, 0x9, 0x75, 0x59, 0x63, 0x9,
    0x2a, 0xa4,

    /* U+0031 "1" */
    0x1a, 0xa0, 0x20, 0xa0, 0x0, 0xa0, 0x0, 0xa0,
    0x39, 0xd7,

    /* U+0032 "2" */
    0x2a, 0xa4, 0x31, 0x18, 0x0, 0x83, 0x7, 0x40,
    0x4d, 0x96,

    /* U+0033 "3" */
    0x39, 0xc5, 0x3, 0x90, 0x2, 0x94, 0x31, 0x18,
    0x3b, 0xa3,

    /* U+0034 "4" */
    0x1, 0xa0, 0x9, 0x10, 0x46, 0x37, 0x69, 0xa7,
    0x0, 0x37,

    /* U+0035 "5" */
    0x4b, 0x94, 0x3a, 0x91, 0x0, 0x28, 0x42, 0x19,
    0x1a, 0xa3,

    /* U+0036 "6" */
    0x3, 0x50, 0x9, 0x81, 0x43, 0x9, 0x72, 0x9,
    0x2a, 0x93,

    /* U+0037 "7" */
    0x6a, 0x9c, 0x21, 0x37, 0x0, 0x91, 0x1, 0x90,
    0x7, 0x30,

    /* U+0038 "8" */
    0x1a, 0xa3, 0x45, 0x28, 0x1c, 0xc2, 0x72, 0x9,
    0x2a, 0xa5,

    /* U+0039 "9" */
    0x2a, 0x94, 0x72, 0x9, 0x19, 0xb7, 0x0, 0xc0,
    0x6, 0x60,

    /* U+003A ":" */
    0x58, 0x0, 0x0, 0x58,

    /* U+003B ";" */
    0x58, 0x0, 0x1, 0x46, 0x73,

    /* U+003C "<" */
    0x0, 0x0, 0x2, 0x95, 0x59, 0x0, 0x7, 0x92,
    0x0, 0x3,

    /* U+003D "=" */
    0x39, 0x95, 0x0, 0x0, 0x39, 0x95,

    /* U+003E ">" */
    0x0, 0x0, 0x3a, 0x40, 0x0, 0x68, 0x18, 0x81,
    0x21, 0x0,

    /* U+003F "?" */
    0x9, 0xa1, 0x0, 0x45, 0x6, 0x90, 0x4, 0x0,
    0x8, 0x20,

    /* U+0040 "@" */
    0x29, 0x75, 0x81, 0x8a, 0x94, 0x49, 0x94, 0x49,
    0x81, 0x86, 0x29, 0x80,

    /* U+0041 "A" */
    0x7, 0x90, 0x9, 0x90, 0x9, 0x62, 0x4b, 0x97,
    0x82, 0xa,

    /* U+0042 "B" */
    0x5b, 0xa4, 0x54, 0x28, 0x5a, 0xb2, 0x54, 0x9,
    0x5b, 0xa5,

    /* U+0043 "C" */
    0x1b, 0xa4, 0x55, 0x4, 0x54, 0x0, 0x55, 0x4,
    0x1b, 0xa4,

    /* U+0044 "D" */
    0x5b, 0xa2, 0x54, 0x18, 0x54, 0x9, 0x54, 0x18,
    0x5b, 0xa3,

    /* U+0045 "E" */
    0x4b, 0x96, 0x45, 0x0, 0x4b, 0x93, 0x45, 0x0,
    0x4b, 0x95,

    /* U+0046 "F" */
    0x5b, 0x96, 0x54, 0x0, 0x5b, 0x94, 0x54, 0x0,
    0x54, 0x0,

    /* U+0047 "G" */
    0x1b, 0xa4, 0x64, 0x3, 0x64, 0x97, 0x64, 0x9,
    0x1b, 0xa4,

    /* U+0048 "H" */
    0x54, 0x18, 0x54, 0x18, 0x5a, 0x98, 0x54, 0x18,
    0x54, 0x18,

    /* U+0049 "I" */
    0x2a, 0xc4, 0x3, 0x60, 0x3, 0x60, 0x3, 0x60,
    0x2a, 0xc4,

    /* U+004A "J" */
    0x0, 0x27, 0x0, 0x27, 0x0, 0x27, 0x50, 0x37,
    0x4a, 0xb2,

    /* U+004B "K" */
    0x54, 0x18, 0x54, 0x91, 0x5a, 0xa0, 0x54, 0x92,
    0x54, 0x19,

    /* U+004C "L" */
    0x18, 0x0, 0x18, 0x0, 0x18, 0x0, 0x18, 0x0,
    0x1c, 0x97,

    /* U+004D "M" */
    0x88, 0x6a, 0x89, 0x79, 0x85, 0x59, 0x81, 0x9,
    0x81, 0x9,

    /* U+004E "N" */
    0x6a, 0x9, 0x6b, 0x19, 0x66, 0x69, 0x63, 0x98,
    0x63, 0x78,

    /* U+004F "O" */
    0x1b, 0xa3, 0x54, 0x18, 0x64, 0x9, 0x54, 0x18,
    0x1b, 0xa3,

    /* U+0050 "P" */
    0x5b, 0x96, 0x54, 0xb, 0x5a, 0x94, 0x54, 0x0,
    0x54, 0x0,

    /* U+0051 "Q" */
    0x2b, 0xa4, 0x63, 0x9, 0x73, 0xa, 0x63, 0x9,
    0x2b, 0xb3, 0x0, 0x65,

    /* U+0052 "R" */
    0x5b, 0xa5, 0x54, 0xa, 0x5a, 0xc3, 0x54, 0x82,
    0x54, 0x19,

    /* U+0053 "S" */
    0x1a, 0xa3, 0x46, 0x2, 0x6, 0x92, 0x31, 0xa,
    0x2a, 0xa5,

    /* U+0054 "T" */
    0x5a, 0xc7, 0x3, 0x60, 0x3, 0x60, 0x3, 0x60,
    0x3, 0x60,

    /* U+0055 "U" */
    0x64, 0x19, 0x64, 0x19, 0x64, 0x19, 0x54, 0x18,
    0x1b, 0xb3,

    /* U+0056 "V" */
    0x82, 0xa, 0x46, 0x27, 0x9, 0x62, 0x9, 0x90,
    0x7, 0x90,

    /* U+0057 "W" */
    0x94, 0x78, 0x8, 0x66, 0x80, 0x87, 0x68, 0x6,
    0x86, 0x80, 0x59, 0x67, 0x0,

    /* U+0058 "X" */
    0x55, 0x28, 0xa, 0x91, 0x6, 0x90, 0xa, 0x91,
    0x64, 0x19,

    /* U+0059 "Y" */
    0x82, 0x9, 0x19, 0x63, 0x8, 0xa0, 0x3, 0x60,
    0x3, 0x60,

    /* U+005A "Z" */
    0x39, 0xb7, 0x0, 0x91, 0x4, 0x60, 0xa, 0x0,
    0x5b, 0x95,

    /* U+005B "[" */
    0x79, 0x19, 0x10, 0x91, 0x9, 0x10, 0x91, 0x9,
    0x10, 0x79, 0x10,

    /* U+005C "\\" */
    0x32, 0x0, 0x18, 0x0, 0xa, 0x0, 0x5, 0x50,
    0x0, 0xa0, 0x0, 0x81, 0x0, 0x27,

    /* U+005D "]" */
    0x89, 0xa, 0xa, 0xa, 0xa, 0xa, 0x89,

    /* U+005E "^" */
    0x6, 0x80, 0x8, 0x71, 0x35, 0x26,

    /* U+005F "_" */
    0x58, 0x86,

    /* U+0060 "`" */
    0x73,

    /* U+0061 "a" */
    0x19, 0xa4, 0x18, 0x88, 0x73, 0x19, 0x3a, 0x79,

    /* U+0062 "b" */
    0x54, 0x0, 0x59, 0xa4, 0x55, 0x9, 0x55, 0x9,
    0x59, 0xa4,

    /* U+0063 "c" */
    0x1b, 0xa4, 0x54, 0x3, 0x54, 0x3, 0x1b, 0xa4,

    /* U+0064 "d" */
    0x0, 0x19, 0x2b, 0x89, 0x63, 0x19, 0x63, 0x19,
    0x2b, 0x89,

    /* U+0065 "e" */
    0x19, 0x94, 0x69, 0x87, 0x64, 0x1, 0x1a, 0xa3,

    /* U+0066 "f" */
    0x5, 0xa6, 0x5c, 0x96, 0x8, 0x20, 0x8, 0x20,
    0x8, 0x20,

    /* U+0067 "g" */
    0x2b, 0x78, 0x64, 0x18, 0x55, 0x28, 0x9, 0x78,
    0x8, 0xa4,

    /* U+0068 "h" */
    0x54, 0x0, 0x59, 0xa4, 0x55, 0x18, 0x54, 0x19,
    0x54, 0x19,

    /* U+0069 "i" */
    0x2, 0x80, 0x29, 0x90, 0x0, 0x90, 0x0, 0x90,
    0x39, 0xd8,

    /* U+006A "j" */
    0x0, 0x90, 0x39, 0xd0, 0x0, 0x90, 0x0, 0x90,
    0x0, 0x90, 0x39, 0x90,

    /* U+006B "k" */
    0x55, 0x0, 0x55, 0x56, 0x5b, 0xb0, 0x55, 0x92,
    0x55, 0x19,

    /* U+006C "l" */
    0x7c, 0x20, 0x7, 0x20, 0x7, 0x20, 0x7, 0x20,
    0x4, 0xa7,

    /* U+006D "m" */
    0x88, 0x88, 0x92, 0x59, 0x92, 0x59, 0x92, 0x59,

    /* U+006E "n" */
    0x59, 0x94, 0x54, 0x18, 0x54, 0x19, 0x54, 0x19,

    /* U+006F "o" */
    0x1a, 0xa3, 0x64, 0x9, 0x64, 0x9, 0x1a, 0xa3,

    /* U+0070 "p" */
    0x59, 0x94, 0x54, 0x9, 0x55, 0x9, 0x59, 0xa4,
    0x54, 0x0,

    /* U+0071 "q" */
    0x2b, 0x89, 0x63, 0x19, 0x64, 0x19, 0x2b, 0x89,
    0x0, 0x19,

    /* U+0072 "r" */
    0x3a, 0x86, 0x36, 0x7, 0x36, 0x0, 0x36, 0x0,

    /* U+0073 "s" */
    0x2a, 0xa4, 0x39, 0x40, 0x2, 0x57, 0x3a, 0xa5,

    /* U+0074 "t" */
    0x9, 0x10, 0x6d, 0x95, 0x9, 0x10, 0x8, 0x10,
    0x5, 0xa5,

    /* U+0075 "u" */
    0x64, 0x19, 0x64, 0x19, 0x54, 0x18, 0x1b, 0xa3,

    /* U+0076 "v" */
    0x73, 0x9, 0x28, 0x55, 0xa, 0x90, 0x7, 0x90,

    /* U+0077 "w" */
    0x84, 0x78, 0x87, 0x77, 0x68, 0x77, 0x39, 0x66,

    /* U+0078 "x" */
    0x37, 0x46, 0x8, 0xa0, 0x9, 0xa0, 0x56, 0x38,

    /* U+0079 "y" */
    0x64, 0x9, 0x9, 0x64, 0x9, 0xa0, 0x4, 0x70,
    0x8, 0x10,

    /* U+007A "z" */
    0x29, 0xb7, 0x1, 0xa0, 0x9, 0x10, 0x5c, 0x95,

    /* U+007B "{" */
    0x0, 0x84, 0x0, 0x90, 0x0, 0x90, 0x4a, 0x50,
    0x0, 0x90, 0x0, 0x90, 0x0, 0x84,

    /* U+007C "|" */
    0x13, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,

    /* U+007D "}" */
    0x29, 0x0, 0x6, 0x30, 0x8, 0x20, 0x3, 0xb6,
    0x8, 0x20, 0x6, 0x30, 0x29, 0x0,

    /* U+007E "~" */
    0x39, 0x17, 0x61, 0x95,

    /* U+00B0 "°" */
    0x56, 0x7, 0x61, 0x56, 0x0,

    /* U+00B1 "±" */
    0x3, 0x60, 0x4a, 0xb6, 0x3, 0x60, 0x49, 0xb5,

    /* U+00B2 "²" */
    0x89, 0x10, 0x70, 0xb7, 0x10,

    /* U+00B3 "³" */
    0x6, 0xb1, 0x1, 0x91, 0x9, 0x91,

    /* U+00B5 "µ" */
    0x54, 0x18, 0x54, 0x18, 0x54, 0x18, 0x5a, 0x88,
    0x54, 0x0,

    /* U+00B7 "·" */
    0x58, 0x0,

    /* U+00D7 "×" */
    0x15, 0x33, 0x8, 0xa0, 0x18, 0x62, 0x0, 0x0,

    /* U+00D8 "Ø" */
    0x1b, 0xa9, 0x64, 0x88, 0x66, 0x79, 0x5b, 0x19,
    0x6b, 0xa3,

    /* U+00F8 "ø" */
    0x0, 0x4, 0x1b, 0xa7, 0x54, 0x98, 0x5b, 0x28,
    0x6b, 0xa3,

    /* U+0401 "Ё" */
    0x9, 0x53, 0x0, 0x0, 0x4b, 0x96, 0x45, 0x0,
    0x4b, 0x93, 0x45, 0x0, 0x4b, 0x95,

    /* U+0410 "А" */
    0x7, 0x90, 0x9, 0x90, 0x9, 0x62, 0x4b, 0x97,
    0x82, 0xa,

    /* U+0411 "Б" */
    0x5b, 0x94, 0x54, 0x0, 0x5a, 0x93, 0x54, 0xa,
    0x5b, 0xa5,

    /* U+0412 "В" */
    0x5b, 0xa4, 0x54, 0x28, 0x5a, 0xb2, 0x54, 0x9,
    0x5b, 0xa5,

    /* U+0413 "Г" */
    0x1d, 0x98, 0x18, 0x0, 0x18, 0x0, 0x18, 0x0,
    0x18, 0x0,

    /* U+0414 "Д" */
    0x1c, 0x98, 0x1, 0x81, 0x80, 0x17, 0x18, 0x3,
    0x61, 0x80, 0xaa, 0x9c, 0x9, 0x0, 0x80,

    /* U+0415 "Е" */
    0x4b, 0x96, 0x45, 0x0, 0x4b, 0x93, 0x45, 0x0,
    0x4b, 0x95,

    /* U+0416 "Ж" */
    0xa2, 0x5a, 0x5, 0x66, 0x80, 0x1c, 0xc4, 0x6,
    0x66, 0x80, 0xa2, 0x59, 0x0,

    /* U+0417 "З" */
    0x2a, 0xa3, 0x31, 0x27, 0x5, 0xc3, 0x30, 0xa,
    0x3a, 0xa4,

    /* U+0418 "И" */
    0x63, 0x78, 0x63, 0x98, 0x66, 0x69, 0x6b, 0x19,
    0x6a, 0x9,

    /* U+0419 "Й" */
    0x8, 0x62, 0x5, 0x60, 0x63, 0x78, 0x63, 0x98,
    0x66, 0x69, 0x6b, 0x19, 0x6a, 0x9,

    /* U+041A "К" */
    0x54, 0x18, 0x54, 0x91, 0x5a, 0xa0, 0x54, 0x92,
    0x54, 0x19,

    /* U+041B "Л" */
    0x1c, 0xa8, 0x18, 0x18, 0x27, 0x18, 0x27, 0x18,
    0x93, 0x18,

    /* U+041C "М" */
    0x88, 0x6a, 0x89, 0x79, 0x85, 0x59, 0x81, 0x9,
    0x81, 0x9,

    /* U+041D "Н" */
    0x54, 0x18, 0x54, 0x18, 0x5a, 0x98, 0x54, 0x18,
    0x54, 0x18,

    /* U+041E "О" */
    0x1b, 0xa3, 0x54, 0x18, 0x64, 0x9, 0x54, 0x18,
    0x1b, 0xa3,

    /* U+041F "П" */
    0x5b, 0xa8, 0x54, 0x18, 0x54, 0x18, 0x54, 0x18,
    0x54, 0x18,

    /* U+0420 "Р" */
    0x5b, 0x96, 0x54, 0xb, 0x5a, 0x94, 0x54, 0x0,
    0x54, 0x0,

    /* U+0421 "С" */
    0x1b, 0xa4, 0x55, 0x4, 0x54, 0x0, 0x55, 0x4,
    0x1b, 0xa4,

    /* U+0422 "Т" */
    0x5a, 0xc7, 0x3, 0x60, 0x3, 0x60, 0x3, 0x60,
    0x3, 0x60,

    /* U+0423 "У" */
    0x73, 0x9, 0x18, 0x54, 0x9, 0x90, 0x4, 0x80,
    0x7, 0x30,

    /* U+0424 "Ф" */
    0x1, 0x20, 0x2a, 0xb4, 0x93, 0x69, 0x93, 0x69,
    0x93, 0x69, 0x2a, 0xc4, 0x1, 0x20,

    /* U+0425 "Х" */
    0x55, 0x28, 0xa, 0x91, 0x6, 0x90, 0xa, 0x91,
    0x64, 0x19,

    /* U+0426 "Ц" */
    0x54, 0x37, 0x54, 0x37, 0x54, 0x37, 0x54, 0x37,
    0x5b, 0xab, 0x0, 0x9,

    /* U+0427 "Ч" */
    0x72, 0x18, 0x73, 0x18, 0x29, 0x98, 0x0, 0x18,
    0x0, 0x18,

    /* U+0428 "Ш" */
    0x92, 0x59, 0x92, 0x59, 0x92, 0x59, 0x92, 0x59,
    0x9a, 0xbb,

    /* U+0429 "Щ" */
    0x92, 0x59, 0x9, 0x25, 0x90, 0x92, 0x59, 0x9,
    0x25, 0x90, 0x9a, 0xbc, 0x10, 0x0, 0x62,

    /* U+042A "Ъ" */
    0x9, 0x90, 0x0, 0x19, 0x0, 0x1, 0xd9, 0x40,
    0x19, 0xa, 0x1, 0xd9, 0x60,

    /* U+042B "Ы" */
    0x90, 0x9, 0x90, 0x9, 0x99, 0x49, 0x90, 0x99,
    0x99, 0x69,

    /* U+042C "Ь" */
    0x54, 0x0, 0x54, 0x0, 0x5a, 0x94, 0x54, 0xa,
    0x5b, 0x96,

    /* U+042D "Э" */
    0x2b, 0xb3, 0x31, 0x18, 0x5, 0x99, 0x21, 0x18,
    0x2a, 0xa3,

    /* U+042E "Ю" */
    0x92, 0x99, 0x95, 0x39, 0x9b, 0x39, 0x95, 0x39,
    0x92, 0xa9,

    /* U+042F "Я" */
    0x3a, 0x98, 0x73, 0x18, 0x1c, 0x98, 0xa, 0x18,
    0x65, 0x18,

    /* U+0430 "а" */
    0x19, 0xa4, 0x18, 0x88, 0x73, 0x19, 0x3a, 0x79,

    /* U+0431 "б" */
    0x1, 0x42, 0x2a, 0x51, 0x58, 0x92, 0x65, 0x28,
    0x54, 0x19, 0x1b, 0xa3,

    /* U+0432 "в" */
    0x5a, 0x96, 0x5a, 0xa4, 0x54, 0x9, 0x5a, 0xa5,

    /* U+0433 "г" */
    0x1d, 0x96, 0x19, 0x0, 0x19, 0x0, 0x19, 0x0,

    /* U+0434 "д" */
    0x1c, 0x97, 0x17, 0x27, 0x36, 0x27, 0xaa, 0xac,
    0x90, 0x8,

    /* U+0435 "е" */
    0x19, 0x94, 0x69, 0x87, 0x64, 0x1, 0x1a, 0xa3,

    /* U+0436 "ж" */
    0x83, 0x59, 0x2, 0xcc, 0x50, 0x65, 0x68, 0x9,
    0x25, 0x90,

    /* U+0437 "з" */
    0x29, 0xa6, 0x5, 0xb4, 0x20, 0x19, 0x3a, 0xa5,

    /* U+0438 "и" */
    0x54, 0x78, 0x54, 0x99, 0x5a, 0x39, 0x5a, 0x9,

    /* U+0439 "й" */
    0x4, 0x21, 0x6, 0x70, 0x54, 0x78, 0x54, 0x99,
    0x5a, 0x39, 0x5a, 0x9,

    /* U+043A "к" */
    0x45, 0x47, 0x4b, 0xb0, 0x45, 0x82, 0x45, 0x1a,

    /* U+043B "л" */
    0x1c, 0x99, 0x18, 0x19, 0x27, 0x19, 0x83, 0x19,

    /* U+043C "м" */
    0x87, 0x5a, 0x88, 0x79, 0x83, 0x39, 0x81, 0x9,

    /* U+043D "н" */
    0x54, 0x19, 0x5b, 0xa9, 0x54, 0x19, 0x54, 0x19,

    /* U+043E "о" */
    0x1a, 0xa3, 0x64, 0x9, 0x64, 0x9, 0x1a, 0xa3,

    /* U+043F "п" */
    0x5b, 0x99, 0x54, 0x19, 0x54, 0x19, 0x54, 0x19,

    /* U+0440 "р" */
    0x59, 0x94, 0x54, 0x9, 0x55, 0x9, 0x59, 0xa4,
    0x54, 0x0,

    /* U+0441 "с" */
    0x1b, 0xa4, 0x54, 0x3, 0x54, 0x3, 0x1b, 0xa4,

    /* U+0442 "т" */
    0x5a, 0xc7, 0x3, 0x60, 0x3, 0x60, 0x3, 0x60,

    /* U+0443 "у" */
    0x64, 0x9, 0x9, 0x64, 0x9, 0xa0, 0x4, 0x70,
    0x8, 0x10,

    /* U+0444 "ф" */
    0x3, 0x60, 0x4a, 0xb6, 0x92, 0x59, 0x92, 0x59,
    0x4a, 0xb6, 0x3, 0x60,

    /* U+0445 "х" */
    0x37, 0x46, 0x8, 0xa0, 0x9, 0xa0, 0x56, 0x38,

    /* U+0446 "ц" */
    0x54, 0x27, 0x54, 0x27, 0x54, 0x27, 0x5b, 0xac,
    0x0, 0x8,

    /* U+0447 "ч" */
    0x72, 0x28, 0x63, 0x28, 0x19, 0x98, 0x0, 0x28,

    /* U+0448 "ш" */
    0x92, 0x59, 0x92, 0x59, 0x92, 0x59, 0x99, 0xbb,

    /* U+0449 "щ" */
    0x93, 0x49, 0x9, 0x34, 0x90, 0x93, 0x49, 0x9,
    0xaa, 0xc1, 0x0, 0x6, 0x20,

    /* U+044A "ъ" */
    0x7b, 0x0, 0xb, 0x96, 0xa, 0xa, 0xc, 0x99,

    /* U+044B "ы" */
    0x90, 0x9, 0x98, 0x39, 0x90, 0x99, 0x99, 0x69,

    /* U+044C "ь" */
    0x45, 0x0, 0x4b, 0x95, 0x45, 0xa, 0x4b, 0x97,

    /* U+044D "э" */
    0x3a, 0xa3, 0x2, 0x99, 0x32, 0x19, 0x1a, 0xa3,

    /* U+044E "ю" */
    0x92, 0x98, 0x9a, 0x39, 0x94, 0x39, 0x91, 0x99,

    /* U+044F "я" */
    0x2a, 0x99, 0x64, 0x19, 0x1c, 0x99, 0x38, 0x19,

    /* U+0451 "ё" */
    0x8, 0x63, 0x0, 0x0, 0x19, 0x94, 0x69, 0x87,
    0x64, 0x1, 0x1a, 0xa3,

    /* U+2013 "–" */
    0x39, 0x95,

    /* U+2014 "—" */
    0x99, 0x99, 0x10,

    /* U+201C "“" */
    0xa, 0x73, 0x37, 0xa0, 0x2, 0x10,

    /* U+201E "„" */
    0x1, 0x0, 0xa, 0x19, 0x19, 0x55,

    /* U+2026 "…" */
    0x10, 0x12, 0x84, 0x69,

    /* U+2033 "″" */
    0xb, 0x65, 0x28, 0x81, 0x1, 0x10,

    /* U+2212 "−" */
    0x9, 0x91
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 67, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 67, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 67, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 10, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 34, .adv_w = 67, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 67, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 67, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 63, .adv_w = 67, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 74, .adv_w = 67, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 85, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 101, .adv_w = 67, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 104, .adv_w = 67, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 106, .adv_w = 67, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 122, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 67, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 67, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 231, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 241, .adv_w = 67, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 247, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 257, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 279, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 299, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 309, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 319, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 349, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 359, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 369, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 379, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 389, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 399, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 409, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 419, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 429, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 439, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 451, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 481, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 491, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 501, .adv_w = 67, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 514, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 524, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 534, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 544, .adv_w = 67, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 555, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 569, .adv_w = 67, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 576, .adv_w = 67, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 582, .adv_w = 67, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 584, .adv_w = 67, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 585, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 593, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 603, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 611, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 621, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 629, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 639, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 649, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 659, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 669, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 681, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 691, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 701, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 709, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 717, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 725, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 735, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 745, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 753, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 761, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 771, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 779, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 787, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 795, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 803, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 813, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 821, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 835, .adv_w = 67, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 842, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 856, .adv_w = 67, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 860, .adv_w = 67, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 865, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 873, .adv_w = 67, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 878, .adv_w = 67, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 884, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 894, .adv_w = 67, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 896, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 904, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 914, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 924, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 938, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 948, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 958, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 968, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 978, .adv_w = 67, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 993, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1003, .adv_w = 67, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1016, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1026, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1036, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1050, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1060, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1070, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1080, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1090, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1100, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1110, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1120, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1130, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1140, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1150, .adv_w = 67, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1164, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1174, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1186, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1196, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1206, .adv_w = 67, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1221, .adv_w = 67, .box_w = 5, .box_h = 5, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 1234, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1244, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1254, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1264, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1274, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1284, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1292, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1304, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1312, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1320, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1330, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1338, .adv_w = 67, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1348, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1356, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1364, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1376, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1384, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1392, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1400, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1408, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1416, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1424, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1434, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1442, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1450, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1460, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1472, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1480, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1490, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1498, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1506, .adv_w = 67, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1519, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1527, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1535, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1543, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1551, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1559, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1567, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1579, .adv_w = 67, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 1581, .adv_w = 67, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 1584, .adv_w = 67, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 1590, .adv_w = 67, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1596, .adv_w = 67, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1600, .adv_w = 67, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 1606, .adv_w = 67, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x1, 0x2, 0x3, 0x5, 0x7, 0x27, 0x28,
    0x48, 0x351
};

static const uint16_t unicode_list_3[] = {
    0x0, 0x1bc2, 0x1bc3, 0x1bcb, 0x1bcd, 0x1bd5, 0x1be2, 0x1dc1
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 176, .range_length = 850, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 10, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    },
    {
        .range_start = 1040, .range_length = 64, .glyph_id_start = 106,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 1105, .range_length = 7618, .glyph_id_start = 170,
        .unicode_list = unicode_list_3, .glyph_id_ofs_list = NULL, .list_length = 8, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
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
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 4,
    .bpp = 4,
    .kern_classes = 0,
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
const lv_font_t font_num_sm_t10 = {
#else
lv_font_t font_num_sm_t10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 8,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_NUM_SM_T10*/

