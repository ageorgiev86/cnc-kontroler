/* Бързо рисуване на линии направо в буфер XRGB8888, без задачите за рисуване на LVGL
 * (те са бавни при десетки хиляди линии). За траекторията и прегледа на програми. */
#ifndef CNC_RASTER_H
#define CNC_RASTER_H

#include "lvgl.h"

/* дебелина 1–4 px, opa 0–255, dash > 0 = пунктир (дължина на тирето и празното в px) */
void raster_line(lv_draw_buf_t *b, int32_t x0, int32_t y0, int32_t x1, int32_t y1, lv_color_t c, int w, uint8_t opa, int dash);

#endif
