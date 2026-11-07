/*******************************************************************************
 * Size: 7 px
 * Bpp: 4
 * Opts: --font ttf/Roboto-Medium.ttf -r 0x20-0x7E,0xB0-0xB3,0xB5,0xB7,0xD7,0xD8,0xF8,0x401,0x410-0x44F,0x451,0x2013,0x2014,0x201C,0x201E,0x2026,0x2033,0x2212 --size 7 --bpp 4 --format lvgl --no-compress --lv-include lvgl.h --lv-font-name font_txt_sm_t10 -o gen/font_txt_sm_t10.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef FONT_TXT_SM_T10
#define FONT_TXT_SM_T10 1
#endif

#if FONT_TXT_SM_T10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x75, 0x75, 0x75, 0x43, 0x64,

    /* U+0022 "\"" */
    0x99, 0x77,

    /* U+0023 "#" */
    0x5, 0x38, 0x4, 0xca, 0xb1, 0x9, 0x63, 0x6,
    0xdc, 0x80, 0x17, 0x90, 0x0,

    /* U+0024 "$" */
    0x2, 0x40, 0x2c, 0xc3, 0x67, 0x25, 0x9, 0xa1,
    0x52, 0x49, 0x4c, 0xc4, 0x2, 0x30,

    /* U+0025 "%" */
    0x79, 0x43, 0x5, 0x84, 0x70, 0x0, 0x99, 0x60,
    0x57, 0x59, 0x3, 0x1a, 0x80,

    /* U+0026 "&" */
    0x1c, 0xc1, 0x3, 0x9a, 0x10, 0x2e, 0x74, 0x1a,
    0x3b, 0xc0, 0x5b, 0xbd, 0x10,

    /* U+0027 "'" */
    0x97,

    /* U+0028 "(" */
    0x5, 0x21, 0x90, 0x64, 0x8, 0x30, 0x83, 0x5,
    0x50, 0xa, 0x0, 0x32,

    /* U+0029 ")" */
    0x70, 0x3, 0x70, 0xb, 0x0, 0xc0, 0xc, 0x0,
    0xb0, 0x55, 0x5, 0x0,

    /* U+002A "*" */
    0x9, 0x0, 0x7d, 0x80, 0x58, 0x60, 0x0, 0x0,

    /* U+002B "+" */
    0x4, 0x30, 0x7, 0x50, 0x9e, 0xe8, 0x7, 0x50,
    0x2, 0x20,

    /* U+002C "," */
    0x82, 0x90, 0x0,

    /* U+002D "-" */
    0x8b, 0x0,

    /* U+002E "." */
    0x64,

    /* U+002F "/" */
    0x3, 0x60, 0x91, 0xa, 0x4, 0x50, 0x90, 0x5,
    0x0,

    /* U+0030 "0" */
    0x3c, 0xc3, 0x93, 0x49, 0xa3, 0x3a, 0x94, 0x49,
    0x3c, 0xc2,

    /* U+0031 "1" */
    0x2a, 0x92, 0x4a, 0x3, 0xa0, 0x3a, 0x3, 0xa0,

    /* U+0032 "2" */
    0x4c, 0xc3, 0x61, 0x57, 0x0, 0xb1, 0xb, 0x30,
    0x8d, 0xb8,

    /* U+0033 "3" */
    0x4c, 0xc3, 0x41, 0x67, 0x7, 0xd2, 0x40, 0x48,
    0x5c, 0xc3,

    /* U+0034 "4" */
    0x1, 0xe3, 0x9, 0xc3, 0x47, 0xa3, 0xab, 0xea,
    0x0, 0xa3,

    /* U+0035 "5" */
    0x3d, 0xb6, 0x5c, 0xb2, 0x0, 0x49, 0x53, 0x3a,
    0x2c, 0xc3,

    /* U+0036 "6" */
    0x9, 0xb0, 0x5e, 0xb2, 0x95, 0x49, 0x84, 0x3a,
    0x2c, 0xc3,

    /* U+0037 "7" */
    0x8b, 0xca, 0x0, 0x83, 0x1, 0xb0, 0x7, 0x50,
    0xc, 0x0,

    /* U+0038 "8" */
    0x3c, 0xc3, 0x75, 0x57, 0x3d, 0xd2, 0x93, 0x49,
    0x4c, 0xc4,

    /* U+0039 "9" */
    0x4c, 0xc2, 0xa3, 0x58, 0x3b, 0xb9, 0x0, 0x65,
    0xb, 0x90,

    /* U+003A ":" */
    0x74, 0x0, 0x0, 0x74,

    /* U+003B ";" */
    0x83, 0x0, 0x0, 0x73, 0x80,

    /* U+003C "<" */
    0x3, 0xb1, 0xaa, 0x10, 0x2a, 0xb0, 0x0, 0x20,

    /* U+003D "=" */
    0x5b, 0xb5, 0x0, 0x0, 0x5b, 0xb5,

    /* U+003E ">" */
    0x78, 0x10, 0x4, 0xd5, 0x5c, 0x70, 0x20, 0x0,

    /* U+003F "?" */
    0x7c, 0xb0, 0x30, 0xc1, 0x7, 0x70, 0x5, 0x0,
    0xa, 0x0,

    /* U+0040 "@" */
    0x4, 0x98, 0x91, 0x28, 0x0, 0x8, 0x81, 0x69,
    0x38, 0x91, 0x95, 0x48, 0x91, 0xb9, 0xa6, 0x55,
    0x0, 0x0, 0x8, 0x98, 0x20,

    /* U+0041 "A" */
    0x4, 0xe0, 0x0, 0xaa, 0x40, 0xc, 0x2a, 0x6,
    0xdb, 0xe1, 0xb2, 0x7, 0x60,

    /* U+0042 "B" */
    0x8d, 0xc8, 0x8, 0x50, 0xd0, 0x8e, 0xd9, 0x8,
    0x50, 0xd0, 0x8d, 0xba, 0x0,

    /* U+0043 "C" */
    0x1b, 0xba, 0x8, 0x50, 0x72, 0xa3, 0x0, 0x8,
    0x50, 0x72, 0x1b, 0xba, 0x0,

    /* U+0044 "D" */
    0x8d, 0xc6, 0x8, 0x50, 0xc1, 0x85, 0xa, 0x38,
    0x50, 0xd1, 0x8d, 0xc6, 0x0,

    /* U+0045 "E" */
    0x8d, 0xb8, 0x85, 0x0, 0x8d, 0xb4, 0x85, 0x0,
    0x8d, 0xb8,

    /* U+0046 "F" */
    0x8d, 0xb7, 0x85, 0x0, 0x8d, 0xb4, 0x85, 0x0,
    0x85, 0x0,

    /* U+0047 "G" */
    0x1b, 0xbb, 0x8, 0x60, 0x41, 0xa3, 0x6c, 0x48,
    0x60, 0x85, 0x1b, 0xbc, 0x20,

    /* U+0048 "H" */
    0x85, 0x6, 0x78, 0x50, 0x67, 0x8d, 0xbd, 0x78,
    0x50, 0x67, 0x85, 0x6, 0x70,

    /* U+0049 "I" */
    0x76, 0x76, 0x76, 0x76, 0x76,

    /* U+004A "J" */
    0x0, 0x76, 0x0, 0x76, 0x0, 0x76, 0x60, 0x76,
    0x6c, 0xc1,

    /* U+004B "K" */
    0x85, 0x2c, 0x18, 0x6c, 0x20, 0x8e, 0xb0, 0x8,
    0x79, 0x60, 0x85, 0xd, 0x20,

    /* U+004C "L" */
    0x85, 0x0, 0x85, 0x0, 0x85, 0x0, 0x85, 0x0,
    0x8d, 0xb7,

    /* U+004D "M" */
    0x8d, 0x0, 0xb9, 0x8d, 0x21, 0xd9, 0x88, 0x96,
    0x89, 0x85, 0xbb, 0x49, 0x85, 0x78, 0x49,

    /* U+004E "N" */
    0x8a, 0x6, 0x78, 0xe4, 0x67, 0x86, 0xc6, 0x78,
    0x55, 0xe7, 0x85, 0xb, 0x70,

    /* U+004F "O" */
    0x1b, 0xca, 0x8, 0x60, 0x85, 0xa3, 0x6, 0x78,
    0x60, 0x95, 0x1b, 0xca, 0x0,

    /* U+0050 "P" */
    0x8d, 0xbb, 0x8, 0x50, 0xb2, 0x8d, 0xb9, 0x8,
    0x50, 0x0, 0x85, 0x0, 0x0,

    /* U+0051 "Q" */
    0x1b, 0xca, 0x8, 0x50, 0x95, 0xa3, 0x6, 0x78,
    0x50, 0x95, 0x1b, 0xcd, 0x0, 0x0, 0x82,

    /* U+0052 "R" */
    0x8d, 0xba, 0x8, 0x50, 0xd0, 0x8d, 0xd8, 0x8,
    0x55, 0x80, 0x85, 0xd, 0x10,

    /* U+0053 "S" */
    0x3c, 0xc7, 0x86, 0x7, 0x19, 0xb4, 0x61, 0xe,
    0x4c, 0xc9,

    /* U+0054 "T" */
    0x9c, 0xeb, 0x10, 0x48, 0x0, 0x4, 0x80, 0x0,
    0x48, 0x0, 0x4, 0x80, 0x0,

    /* U+0055 "U" */
    0x94, 0xb, 0x29, 0x40, 0xb2, 0x94, 0xb, 0x28,
    0x50, 0xc1, 0x2c, 0xc9, 0x0,

    /* U+0056 "V" */
    0xc2, 0xa, 0x46, 0x70, 0xd0, 0x1c, 0x49, 0x0,
    0xbb, 0x30, 0x5, 0xd0, 0x0,

    /* U+0057 "W" */
    0xb1, 0x69, 0xd, 0x7, 0x5a, 0xc2, 0xb0, 0x48,
    0xb9, 0x77, 0x0, 0xd8, 0x5e, 0x30, 0xc, 0x41,
    0xf0, 0x0,

    /* U+0058 "X" */
    0x87, 0x1d, 0x0, 0xda, 0x50, 0x7, 0xe0, 0x0,
    0xda, 0x60, 0x96, 0x1d, 0x10,

    /* U+0059 "Y" */
    0xb3, 0xd, 0x2, 0xb6, 0x70, 0xa, 0xc0, 0x0,
    0x49, 0x0, 0x4, 0x90, 0x0,

    /* U+005A "Z" */
    0x8b, 0xcd, 0x0, 0xb4, 0x6, 0x90, 0x2c, 0x0,
    0xad, 0xbb,

    /* U+005B "[" */
    0x79, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x79,

    /* U+005C "\\" */
    0xb1, 0x5, 0x70, 0xc, 0x0, 0x93, 0x3, 0x90,
    0x6,

    /* U+005D "]" */
    0xb5, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0xb5,

    /* U+005E "^" */
    0x6, 0x3, 0xd3, 0x92, 0x90,

    /* U+005F "_" */
    0xbb, 0xb1,

    /* U+0060 "`" */
    0x57,

    /* U+0061 "a" */
    0x5b, 0xc1, 0x39, 0xc5, 0xa2, 0x85, 0x6b, 0xc6,

    /* U+0062 "b" */
    0x94, 0x0, 0x94, 0x0, 0x9b, 0xc3, 0x94, 0x3a,
    0x94, 0x3a, 0x9a, 0xc3,

    /* U+0063 "c" */
    0x3c, 0xc2, 0xa2, 0x23, 0xa2, 0x22, 0x3c, 0xc2,

    /* U+0064 "d" */
    0x0, 0x58, 0x0, 0x58, 0x4c, 0xc8, 0xa2, 0x58,
    0xa2, 0x58, 0x4c, 0xb8,

    /* U+0065 "e" */
    0x3c, 0xc3, 0xab, 0xa6, 0xa3, 0x0, 0x3c, 0xc3,

    /* U+0066 "f" */
    0x1c, 0x54, 0x80, 0xad, 0x34, 0x80, 0x48, 0x4,
    0x80,

    /* U+0067 "g" */
    0x4c, 0xb8, 0xa2, 0x48, 0xa3, 0x48, 0x4c, 0xc8,
    0x10, 0x58, 0x4b, 0xc2,

    /* U+0068 "h" */
    0x93, 0x0, 0x93, 0x0, 0x9b, 0xd3, 0x93, 0x57,
    0x93, 0x57, 0x93, 0x57,

    /* U+0069 "i" */
    0x74, 0x85, 0x85, 0x85, 0x85,

    /* U+006A "j" */
    0x7, 0x30, 0x84, 0x8, 0x40, 0x84, 0x8, 0x40,
    0x84, 0x3d, 0x10,

    /* U+006B "k" */
    0x94, 0x0, 0x94, 0x0, 0x94, 0xc2, 0x9d, 0x60,
    0x9a, 0xb0, 0x94, 0x95,

    /* U+006C "l" */
    0x85, 0x85, 0x85, 0x85, 0x85, 0x85,

    /* U+006D "m" */
    0x9a, 0xc9, 0xb6, 0x94, 0x57, 0x2a, 0x94, 0x57,
    0x2a, 0x94, 0x57, 0x2a,

    /* U+006E "n" */
    0x9a, 0xc3, 0x94, 0x57, 0x93, 0x57, 0x93, 0x57,

    /* U+006F "o" */
    0x3c, 0xc3, 0xa2, 0x3a, 0xa2, 0x3a, 0x3c, 0xc3,

    /* U+0070 "p" */
    0x9a, 0xc3, 0x94, 0x39, 0x94, 0x39, 0x9b, 0xd3,
    0x94, 0x0, 0x94, 0x0,

    /* U+0071 "q" */
    0x4c, 0xb8, 0xa2, 0x58, 0xa2, 0x58, 0x4c, 0xc8,
    0x0, 0x58, 0x0, 0x58,

    /* U+0072 "r" */
    0x9c, 0x49, 0x40, 0x94, 0x9, 0x40,

    /* U+0073 "s" */
    0x5b, 0xb1, 0x89, 0x41, 0x34, 0xc3, 0x5b, 0xb1,

    /* U+0074 "t" */
    0x57, 0xc, 0xc1, 0x57, 0x5, 0x70, 0x3d, 0x10,

    /* U+0075 "u" */
    0x93, 0x57, 0x93, 0x57, 0x93, 0x67, 0x5c, 0xb7,

    /* U+0076 "v" */
    0xc1, 0x93, 0x65, 0xc0, 0x1c, 0x90, 0xc, 0x40,

    /* U+0077 "w" */
    0xb0, 0xd0, 0xc0, 0x86, 0xd6, 0xa0, 0x4d, 0x4d,
    0x70, 0xe, 0xc, 0x30,

    /* U+0078 "x" */
    0x95, 0xc2, 0x1e, 0x80, 0x1d, 0x90, 0x94, 0xb2,

    /* U+0079 "y" */
    0xc1, 0xa3, 0x75, 0xd0, 0x3c, 0xa0, 0xe, 0x50,
    0xb, 0x0, 0x78, 0x0,

    /* U+007A "z" */
    0x7b, 0xf2, 0x6, 0x80, 0x2c, 0x0, 0xad, 0xb3,

    /* U+007B "{" */
    0x9, 0x12, 0xa0, 0x29, 0xa, 0x50, 0x39, 0x2,
    0xa0, 0xb, 0x10, 0x0,

    /* U+007C "|" */
    0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x10,

    /* U+007D "}" */
    0x0, 0x9, 0x20, 0x47, 0x4, 0x80, 0xd, 0x24,
    0x80, 0x47, 0x9, 0x20, 0x0, 0x0,

    /* U+007E "~" */
    0x2a, 0x45, 0x27, 0x18, 0xa0,

    /* U+00B0 "°" */
    0x48, 0x14, 0x81,

    /* U+00B1 "±" */
    0x3, 0x20, 0x7, 0x40, 0x7d, 0xc5, 0x6, 0x40,
    0x6b, 0xb3,

    /* U+00B2 "²" */
    0x8a, 0x20, 0xa1, 0x9a, 0x20,

    /* U+00B3 "³" */
    0x7a, 0x20, 0xa2, 0x7a, 0x30,

    /* U+00B5 "µ" */
    0x85, 0x3a, 0x85, 0x3a, 0x85, 0x3a, 0x8c, 0xba,
    0x85, 0x0, 0x85, 0x0,

    /* U+00B7 "·" */
    0x65,

    /* U+00D7 "×" */
    0x0, 0x0, 0x76, 0xa3, 0xe, 0x90, 0x78, 0xb2,
    0x10, 0x0,

    /* U+00D8 "Ø" */
    0x0, 0x6, 0x31, 0xbc, 0xd0, 0x86, 0x6b, 0x5a,
    0x48, 0x67, 0x8c, 0x28, 0x51, 0xec, 0xa0, 0x21,
    0x0, 0x0,

    /* U+00F8 "ø" */
    0x0, 0x12, 0x3c, 0xe3, 0xa3, 0x9a, 0xa9, 0x4a,
    0x3e, 0xc3, 0x21, 0x0,

    /* U+0401 "Ё" */
    0x8, 0x53, 0x0, 0x0, 0x8d, 0xb8, 0x85, 0x0,
    0x8d, 0xb4, 0x85, 0x0, 0x8d, 0xb8,

    /* U+0410 "А" */
    0x4, 0xe0, 0x0, 0xaa, 0x40, 0xc, 0x2a, 0x6,
    0xdb, 0xe1, 0xb2, 0x7, 0x60,

    /* U+0411 "Б" */
    0x8d, 0xb7, 0x8, 0x70, 0x0, 0x8d, 0xb8, 0x8,
    0x70, 0xc1, 0x8d, 0xba, 0x0,

    /* U+0412 "В" */
    0x8d, 0xc8, 0x8, 0x50, 0xd0, 0x8e, 0xd9, 0x8,
    0x50, 0xd0, 0x8d, 0xba, 0x0,

    /* U+0413 "Г" */
    0x7d, 0xb7, 0x76, 0x0, 0x76, 0x0, 0x76, 0x0,
    0x76, 0x0,

    /* U+0414 "Д" */
    0xa, 0xcd, 0x70, 0xb, 0x25, 0x70, 0xc, 0x5,
    0x70, 0x1d, 0x5, 0x70, 0xbe, 0xbd, 0xd1, 0xd0,
    0x0, 0xc1, 0x60, 0x0, 0x60,

    /* U+0415 "Е" */
    0x8d, 0xb8, 0x85, 0x0, 0x8d, 0xb4, 0x85, 0x0,
    0x8d, 0xb8,

    /* U+0416 "Ж" */
    0x88, 0xd, 0xc, 0x40, 0xc2, 0xd5, 0xa0, 0x7,
    0xef, 0xe4, 0x1, 0xd1, 0xd4, 0xc0, 0x96, 0xd,
    0xa, 0x50,

    /* U+0417 "З" */
    0x4c, 0xc7, 0x41, 0xd, 0x5, 0xc8, 0x51, 0xd,
    0x5c, 0xc7,

    /* U+0418 "И" */
    0x85, 0xb, 0x68, 0x55, 0xe6, 0x86, 0xc7, 0x68,
    0xe4, 0x76, 0x8a, 0x7, 0x60,

    /* U+0419 "Й" */
    0x4, 0x4, 0x0, 0x39, 0x30, 0x85, 0xb, 0x68,
    0x55, 0xe6, 0x86, 0xc7, 0x68, 0xe4, 0x76, 0x8a,
    0x7, 0x60,

    /* U+041A "К" */
    0x76, 0x1d, 0x17, 0x6b, 0x50, 0x7e, 0xd0, 0x7,
    0x69, 0x70, 0x76, 0xc, 0x30,

    /* U+041B "Л" */
    0xc, 0xbd, 0x60, 0xd0, 0x76, 0xd, 0x7, 0x60,
    0xc0, 0x76, 0xa6, 0x7, 0x60,

    /* U+041C "М" */
    0x8d, 0x0, 0xb9, 0x8d, 0x21, 0xd9, 0x88, 0x96,
    0x89, 0x85, 0xbb, 0x49, 0x85, 0x78, 0x49,

    /* U+041D "Н" */
    0x85, 0x6, 0x78, 0x50, 0x67, 0x8d, 0xbd, 0x78,
    0x50, 0x67, 0x85, 0x6, 0x70,

    /* U+041E "О" */
    0x1b, 0xca, 0x8, 0x60, 0x85, 0xa3, 0x6, 0x78,
    0x60, 0x95, 0x1b, 0xca, 0x0,

    /* U+041F "П" */
    0x7d, 0xbd, 0x77, 0x60, 0x67, 0x76, 0x6, 0x77,
    0x60, 0x67, 0x76, 0x6, 0x70,

    /* U+0420 "Р" */
    0x8d, 0xbb, 0x8, 0x50, 0xb2, 0x8d, 0xb9, 0x8,
    0x50, 0x0, 0x85, 0x0, 0x0,

    /* U+0421 "С" */
    0x1b, 0xba, 0x8, 0x50, 0x72, 0xa3, 0x0, 0x8,
    0x50, 0x72, 0x1b, 0xba, 0x0,

    /* U+0422 "Т" */
    0x9c, 0xeb, 0x10, 0x48, 0x0, 0x4, 0x80, 0x0,
    0x48, 0x0, 0x4, 0x80, 0x0,

    /* U+0423 "У" */
    0x95, 0xd, 0x11, 0xc4, 0xa0, 0x9, 0xd3, 0x0,
    0x3c, 0x0, 0x1c, 0x40, 0x0,

    /* U+0424 "Ф" */
    0x0, 0x52, 0x0, 0x1a, 0xec, 0x60, 0x85, 0x93,
    0xb2, 0xb1, 0x93, 0x84, 0x85, 0x93, 0xb2, 0x1a,
    0xec, 0x60, 0x0, 0x52, 0x0,

    /* U+0425 "Х" */
    0x87, 0x1d, 0x0, 0xda, 0x50, 0x7, 0xe0, 0x0,
    0xda, 0x60, 0x96, 0x1d, 0x10,

    /* U+0426 "Ц" */
    0x85, 0x7, 0x60, 0x85, 0x7, 0x60, 0x85, 0x7,
    0x60, 0x85, 0x7, 0x60, 0x8d, 0xbd, 0xd0, 0x0,
    0x0, 0xc0, 0x0, 0x0, 0x60,

    /* U+0427 "Ч" */
    0x85, 0x8, 0x57, 0x60, 0x85, 0x1b, 0xbe, 0x50,
    0x0, 0x85, 0x0, 0x8, 0x50,

    /* U+0428 "Ш" */
    0x76, 0x1c, 0xb, 0x27, 0x61, 0xc0, 0xb2, 0x76,
    0x1c, 0xb, 0x27, 0x61, 0xc0, 0xb2, 0x7d, 0xbe,
    0xbe, 0x20,

    /* U+0429 "Щ" */
    0x76, 0x1c, 0xb, 0x27, 0x61, 0xc0, 0xb2, 0x76,
    0x1c, 0xb, 0x27, 0x61, 0xc0, 0xb2, 0x7c, 0xbe,
    0xbe, 0x80, 0x0, 0x0, 0x1b, 0x0, 0x0, 0x0,
    0x50,

    /* U+042A "Ъ" */
    0xad, 0x50, 0x0, 0x8, 0x50, 0x0, 0x8, 0xdb,
    0x80, 0x8, 0x50, 0xd1, 0x8, 0xdb, 0xa0,

    /* U+042B "Ы" */
    0x76, 0x0, 0x58, 0x76, 0x0, 0x58, 0x7d, 0xb8,
    0x58, 0x76, 0xc, 0x78, 0x7d, 0xba, 0x58,

    /* U+042C "Ь" */
    0x85, 0x0, 0x8, 0x50, 0x0, 0x8d, 0xb8, 0x8,
    0x50, 0xc1, 0x8d, 0xba, 0x0,

    /* U+042D "Э" */
    0x2c, 0xc8, 0x6, 0x30, 0xb2, 0x3, 0xbe, 0x46,
    0x30, 0xb2, 0x2c, 0xb8, 0x0,

    /* U+042E "Ю" */
    0x76, 0x4b, 0xb6, 0x7, 0x6c, 0x10, 0xd0, 0x7d,
    0xe0, 0xb, 0x27, 0x6c, 0x10, 0xd0, 0x76, 0x4b,
    0xb6, 0x0,

    /* U+042F "Я" */
    0x4c, 0xbc, 0xa4, 0x1c, 0x2e, 0xbc, 0x3a, 0x1c,
    0xb3, 0x1c,

    /* U+0430 "а" */
    0x5b, 0xc1, 0x39, 0xc5, 0xa2, 0x85, 0x6b, 0xc6,

    /* U+0431 "б" */
    0x0, 0x33, 0x1b, 0x81, 0x8b, 0xb2, 0xb3, 0x3a,
    0x93, 0x2a, 0x3c, 0xc3,

    /* U+0432 "в" */
    0x8c, 0xc3, 0x85, 0x58, 0x8c, 0xc5, 0x8c, 0xb7,

    /* U+0433 "г" */
    0x8c, 0x98, 0x40, 0x84, 0x8, 0x40,

    /* U+0434 "д" */
    0xe, 0xca, 0x0, 0xd2, 0xa0, 0x1b, 0x2a, 0xb,
    0xdb, 0xe1, 0xd0, 0xa, 0x22, 0x0, 0x10,

    /* U+0435 "е" */
    0x3c, 0xc3, 0xab, 0xa6, 0xa3, 0x0, 0x3c, 0xc3,

    /* U+0436 "ж" */
    0x96, 0x94, 0xa4, 0x1c, 0x95, 0xc0, 0xd, 0xee,
    0x80, 0x87, 0x94, 0xc3,

    /* U+0437 "з" */
    0x6b, 0xc1, 0x8, 0xd1, 0x40, 0x94, 0x5c, 0xb1,

    /* U+0438 "и" */
    0x84, 0x88, 0x86, 0xd8, 0x8d, 0x68, 0x88, 0x48,

    /* U+0439 "й" */
    0x8, 0x80, 0x84, 0x88, 0x86, 0xd8, 0x8d, 0x68,
    0x88, 0x48,

    /* U+043A "к" */
    0x85, 0x86, 0x86, 0xc0, 0x8e, 0x90, 0x85, 0xa6,

    /* U+043B "л" */
    0x1e, 0xc9, 0x2a, 0x49, 0x49, 0x49, 0xc4, 0x49,

    /* U+043C "м" */
    0x8b, 0x7, 0xc8, 0xe1, 0xcc, 0x88, 0xb8, 0xc8,
    0x5d, 0x2c,

    /* U+043D "н" */
    0x84, 0x48, 0x84, 0x48, 0x8c, 0xc8, 0x84, 0x48,

    /* U+043E "о" */
    0x3c, 0xc3, 0xa2, 0x3a, 0xa2, 0x3a, 0x3c, 0xc3,

    /* U+043F "п" */
    0x8c, 0xc8, 0x84, 0x48, 0x84, 0x48, 0x84, 0x48,

    /* U+0440 "р" */
    0x9a, 0xc3, 0x94, 0x39, 0x94, 0x39, 0x9b, 0xd3,
    0x94, 0x0, 0x94, 0x0,

    /* U+0441 "с" */
    0x3c, 0xc2, 0xa2, 0x23, 0xa2, 0x22, 0x3c, 0xc2,

    /* U+0442 "т" */
    0x9e, 0xb3, 0xb, 0x20, 0xb, 0x20, 0xb, 0x20,

    /* U+0443 "у" */
    0xc1, 0xa3, 0x75, 0xd0, 0x3c, 0xa0, 0xe, 0x50,
    0xb, 0x0, 0x78, 0x0,

    /* U+0444 "ф" */
    0x0, 0x10, 0x0, 0xd, 0x0, 0x4c, 0xfc, 0x5a,
    0x2e, 0x1c, 0xa2, 0xe1, 0xc4, 0xcf, 0xc6, 0x0,
    0xd0, 0x0, 0xd, 0x0,

    /* U+0445 "х" */
    0x95, 0xc2, 0x1e, 0x80, 0x1d, 0x90, 0x94, 0xb2,

    /* U+0446 "ц" */
    0x84, 0x48, 0x8, 0x44, 0x80, 0x84, 0x48, 0x8,
    0xcc, 0xe0, 0x0, 0xc, 0x0, 0x0, 0x20,

    /* U+0447 "ч" */
    0xa2, 0x66, 0xa3, 0x66, 0x3b, 0xd6, 0x0, 0x66,

    /* U+0448 "ш" */
    0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
    0x84, 0x8c, 0xdc, 0xd4,

    /* U+0449 "щ" */
    0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
    0x94, 0x9c, 0xdc, 0xda, 0x0, 0x0, 0xc, 0x0,
    0x0, 0x2,

    /* U+044A "ъ" */
    0x9f, 0x0, 0x0, 0xdb, 0xa0, 0xd, 0x8, 0x40,
    0xdb, 0xc1,

    /* U+044B "ы" */
    0x85, 0x0, 0xb1, 0x8c, 0xb3, 0xb1, 0x85, 0x49,
    0xb1, 0x8c, 0xc4, 0xb1,

    /* U+044C "ь" */
    0x85, 0x0, 0x8c, 0xb3, 0x85, 0x49, 0x8c, 0xc4,

    /* U+044D "э" */
    0x6b, 0xb0, 0x5, 0xc5, 0x81, 0x75, 0x4c, 0xb0,

    /* U+044E "ю" */
    0x85, 0x6a, 0xb0, 0x85, 0xd0, 0x76, 0x8c, 0xe0,
    0x76, 0x85, 0x6a, 0xb0,

    /* U+044F "я" */
    0x5c, 0xd6, 0xa2, 0x66, 0x4d, 0xc6, 0x85, 0x66,

    /* U+0451 "ё" */
    0x35, 0x80, 0x3c, 0xc3, 0xab, 0xa6, 0xa3, 0x0,
    0x3c, 0xc3,

    /* U+2013 "–" */
    0x5b, 0xbb, 0x0,

    /* U+2014 "—" */
    0x6b, 0xbb, 0xb0,

    /* U+201C "“" */
    0x45, 0x49, 0x93,

    /* U+201E "„" */
    0x8a, 0x49, 0x91, 0x0, 0x0,

    /* U+2026 "…" */
    0x65, 0xb5, 0x60,

    /* U+2033 "″" */
    0x99, 0x77,

    /* U+2212 "−" */
    0x7a, 0xa2
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 28, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 30, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 36, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 7, .adv_w = 68, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 64, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 34, .adv_w = 82, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 72, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 19, .box_w = 1, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 61, .adv_w = 39, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 73, .adv_w = 39, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 85, .adv_w = 50, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 93, .adv_w = 62, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 25, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 106, .adv_w = 38, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 108, .adv_w = 31, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 109, .adv_w = 44, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 118, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 128, .adv_w = 64, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 136, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 146, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 206, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 216, .adv_w = 30, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 27, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 225, .adv_w = 57, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 63, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 239, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 247, .adv_w = 54, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 257, .adv_w = 100, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 278, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 63, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 340, .adv_w = 62, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 350, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 363, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 376, .adv_w = 32, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 62, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 391, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 404, .adv_w = 60, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 414, .adv_w = 98, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 429, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 455, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 77, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 483, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 68, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 506, .adv_w = 68, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 519, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 532, .adv_w = 72, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 545, .adv_w = 99, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 563, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 576, .adv_w = 68, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 589, .adv_w = 68, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 599, .adv_w = 30, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 607, .adv_w = 47, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 616, .adv_w = 30, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 624, .adv_w = 48, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 629, .adv_w = 50, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 631, .adv_w = 36, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 632, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 640, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 652, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 660, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 672, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 680, .adv_w = 40, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 689, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 701, .adv_w = 62, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 713, .adv_w = 29, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 718, .adv_w = 28, .box_w = 3, .box_h = 7, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 729, .adv_w = 58, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 741, .adv_w = 29, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 747, .adv_w = 98, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 759, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 767, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 775, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 787, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 799, .adv_w = 40, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 805, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 813, .adv_w = 37, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 821, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 829, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 837, .adv_w = 83, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 849, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 857, .adv_w = 55, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 869, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 877, .adv_w = 37, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 889, .adv_w = 28, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 896, .adv_w = 37, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 910, .adv_w = 74, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 915, .adv_w = 43, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 918, .adv_w = 60, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 928, .adv_w = 41, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 933, .adv_w = 41, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 938, .adv_w = 67, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 950, .adv_w = 32, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 951, .adv_w = 60, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 961, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 979, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 991, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1005, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1018, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1031, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1044, .adv_w = 62, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1054, .adv_w = 84, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1075, .adv_w = 63, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1085, .adv_w = 107, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1103, .adv_w = 68, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1113, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1126, .adv_w = 79, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1144, .adv_w = 72, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1157, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1170, .adv_w = 98, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1185, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1198, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1211, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1224, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1237, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1250, .adv_w = 68, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1263, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1276, .adv_w = 89, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1297, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1310, .adv_w = 84, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1331, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1344, .adv_w = 106, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1362, .adv_w = 112, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1387, .adv_w = 86, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1402, .adv_w = 97, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1417, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1430, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1443, .adv_w = 103, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1461, .adv_w = 69, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1471, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1479, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1491, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1499, .adv_w = 47, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1505, .adv_w = 69, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1520, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1528, .adv_w = 91, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1540, .adv_w = 57, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1548, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1556, .adv_w = 64, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1566, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1574, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1582, .adv_w = 84, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1592, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1600, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1608, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1616, .adv_w = 63, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1628, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1636, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1644, .adv_w = 55, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1656, .adv_w = 82, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1676, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1684, .adv_w = 67, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1699, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1707, .adv_w = 92, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1719, .adv_w = 97, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1737, .adv_w = 72, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1747, .adv_w = 90, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1759, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1767, .adv_w = 59, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1775, .adv_w = 91, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1787, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1795, .adv_w = 60, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1805, .adv_w = 72, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 1808, .adv_w = 86, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 1811, .adv_w = 43, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 1814, .adv_w = 42, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1819, .adv_w = 79, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1822, .adv_w = 36, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 1824, .adv_w = 63, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 2}
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

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 1, 0, 2, 0, 0, 0, 0,
    2, 3, 0, 0, 0, 4, 0, 4,
    5, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 6, 7, 8, 9, 10, 11,
    0, 12, 12, 13, 14, 15, 12, 12,
    9, 16, 17, 18, 0, 19, 13, 20,
    21, 22, 23, 24, 25, 0, 0, 0,
    0, 0, 26, 27, 28, 0, 29, 30,
    0, 31, 0, 0, 32, 0, 31, 31,
    33, 27, 0, 34, 0, 35, 0, 36,
    37, 38, 36, 39, 40, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 10, 6, 41, 7, 42, 43, 10,
    22, 44, 12, 0, 14, 12, 12, 12,
    9, 0, 16, 8, 19, 45, 0, 22,
    12, 12, 12, 46, 47, 12, 48, 49,
    50, 51, 26, 52, 53, 54, 55, 29,
    38, 28, 0, 0, 56, 0, 0, 0,
    33, 0, 27, 28, 57, 36, 27, 38,
    58, 0, 0, 59, 60, 0, 61, 27,
    62, 0, 29, 0, 0, 2, 4, 4,
    2, 0
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 1, 0, 2, 0, 0, 0, 3,
    2, 0, 4, 5, 0, 6, 7, 6,
    8, 0, 0, 0, 0, 0, 0, 0,
    9, 0, 0, 0, 0, 0, 0, 0,
    10, 0, 11, 0, 12, 0, 0, 0,
    12, 0, 0, 13, 0, 0, 0, 0,
    12, 0, 12, 0, 14, 15, 16, 17,
    18, 19, 20, 21, 0, 0, 22, 0,
    0, 0, 23, 0, 24, 24, 24, 25,
    24, 0, 0, 0, 0, 0, 26, 26,
    27, 26, 24, 28, 29, 30, 31, 32,
    33, 34, 32, 35, 0, 0, 36, 0,
    0, 0, 0, 0, 0, 0, 0, 12,
    37, 0, 11, 0, 0, 0, 38, 0,
    19, 0, 0, 0, 0, 39, 0, 0,
    12, 0, 0, 12, 15, 40, 0, 19,
    0, 41, 0, 0, 0, 0, 42, 43,
    0, 0, 23, 44, 45, 26, 46, 24,
    34, 47, 26, 26, 26, 48, 26, 26,
    27, 26, 26, 24, 49, 32, 24, 34,
    26, 50, 26, 26, 51, 52, 53, 54,
    26, 55, 24, 7, 7, 2, 6, 6,
    2, 0
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -3, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1,
    0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -12, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -13, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -7, 0, 0, 0,
    0, 0, 0, 0, -4, 0, -1, 0,
    0, -7, -1, -5, -3, 0, -6, 0,
    0, 0, 0, 0, -1, -1, 0, 0,
    -1, -1, -3, -2, 0, 1, 0, 0,
    1, 1, 0, -3, -3, 0, 0, 0,
    0, 0, 1, -3, -5, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, -1, 0, 0, -3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -2,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -6, 0, 0,
    0, 0, -1, 0, 0, 0, -3, 0,
    -1, 0, -1, -2, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, -2, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, -1, -1,
    0, -1, 0, 0, 0, -1, -1, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -14, 0, 0, 0, 0,
    -10, 0, -13, 0, 1, 0, 0, 0,
    0, 0, 0, 0, -2, -1, 0, 0,
    -1, -1, 0, 0, -1, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, -2, 0, 0, 0, 1,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 0, -2, 0, 0,
    0, 0, 1, 0, 1, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -6, 0, 0, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, -1, -1, 0, 0,
    0, -1, -2, -3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, 0, 0, -4, -5, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, -4, 0, 0,
    -14, -2, -10, -7, 0, -14, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, -7, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -19, 0,
    0, 0, 0, -8, 0, -11, 0, 0,
    0, 0, 0, -2, 0, -2, 0, -1,
    -1, 0, 0, -1, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, -8, -5,
    0, 0, 0, 0, 0, 0, -5, 0,
    -3, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    -2, -1, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, -1,
    0, 0, -3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -3, 0,
    0, 0, 0, -13, -14, 0, 0, 0,
    -5, -2, -13, -1, 1, 0, 1, 1,
    0, 1, 0, 0, -7, -5, 0, -6,
    -8, -4, -6, 0, -5, -4, -3, -4,
    -3, 0, -5, -5, -3, 0, 0, 1,
    0, -2, -5, -8, -7, -7, -4, -7,
    0, -7, -5, -7, -7, 0, 0, 0,
    1, 0, -12, -4, 0, 0, 0, -4,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, -3, -2, 0, 0, -3,
    -2, 0, 0, -2, -1, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, -7, -3, 0, 0, 0, -2, 0,
    0, 0, 1, 0, 0, 0, 0, 0,
    0, 1, -2, -2, 0, 0, -2, -1,
    0, 0, -1, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -5, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, -1, 0, 0, -1, 0, 0,
    0, -1, -2, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, -1, 0,
    0, 0, 1, -2, -3, 0, 0, 0,
    0, 0, 0, 0, -2, 1, -3, -12,
    -5, 0, 0, 0, -6, -2, -5, -1,
    1, -5, 1, 1, 1, 1, 0, 1,
    -4, -4, -1, -2, -4, -2, -3, -1,
    -2, -1, 0, -1, -2, 1, -3, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 0, -1, 0, 0, 0, -1,
    -1, -1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -3, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, -2,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -1, 0, -1,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -1, 0, -1, -4, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, -1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -5, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -1,
    0, -1, -1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, -1,
    -1, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, -8, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, -1, 1,
    0, -1, 0, 0, 3, 0, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, -7, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -1, -1, 1, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -3, 0, -2, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -7, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -1, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -1, 0, 0,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    -1, 0, -1, -1, 0, 0, 0, 0,
    0, -1, 0, 0, 0, 0, 0, 0,
    0, -1, 0, 0, 0, 0, 0, -1,
    -1, -1, 0, 0, 0, 0, 0, 0,
    -3, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, -23, -24, 0, 0,
    0, -11, -4, 0, -3, 0, 0, 0,
    0, 0, 0, 0, 0, -11, -10, 0,
    -11, -10, -8, -10, 0, -9, -8, -6,
    -8, -7, 0, -10, -9, -6, 0, 0,
    0, 0, -4, -10, -14, -13, -13, -8,
    -13, 0, -13, -10, -13, -14, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, -1, 0, 0, -2, 0, -2, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, -2, -2,
    0, 0, 0, 1, 0, 1, 0, -2,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -1, 0, 0,
    0, 0, 0, -1, 0, -1, 0, -1,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -1, -1, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -22, -8, 0, 0, 0, -5, -1,
    0, 0, 1, 0, 0, 0, 0, 1,
    0, 0, -5, -4, 0, -5, -4, 0,
    -3, 0, 0, 0, 0, 0, 0, 0,
    0, -4, -3, 0, 0, 1, 0, 0,
    -2, -5, -2, -4, 0, -1, 0, -2,
    -5, 0, -3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 0,
    0, -1, 0, -1, 0, 1, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, -1, -1, -1, 0, 0,
    1, 0, 1, -1, -1, 0, 0, 0,
    0, 0, 0, -3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -11, 0, -3, 0, -1, -5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -3, 0, 0, 0, 0,
    0, -1, -1, -3, 0, 0, 0, 0,
    0, 0, -3, 0, 0, 0, -1, 0,
    0, 0, -3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -11,
    0, -3, 0, -1, -5, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -3, 0, 0, 0, 0, 0,
    -1, -1, -3, 0, 0, 0, 0, 0,
    0, -3, 0, 0, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, 0, -1, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, -1, -1,
    0, 0, 0, 0, 0, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, 0, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, -2, -1, 0,
    0, 0, 0, 0, -2, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, 0,
    0, -1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, 0, -1, 0, 0,
    0, -1, 0, 0, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -5, 0, -2, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, -1, -1, -2, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, -4,
    1, 0, -2, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 1, -1,
    -1, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 2, 0, 1, -2, -1,
    -1, 0, -1, 0, 0, 0, -7, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -3, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -5, -2, 0,
    0, 0, 0, 0, 0, -7, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -3, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -5, -2, 0, 0,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, -1, 0, 0, 0, 0, 0,
    0, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 62,
    .right_class_cnt     = 55,
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
    .cmap_num = 4,
    .bpp = 4,
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
const lv_font_t font_txt_sm_t10 = {
#else
lv_font_t font_txt_sm_t10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 9,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if FONT_TXT_SM_T10*/

