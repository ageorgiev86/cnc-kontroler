#include "raster.h"

#include <stdlib.h>

static inline void plot(uint8_t *row, int32_t x, uint32_t c, uint8_t opa)
{
    uint8_t *p = row + x * 4;
    if (opa >= 250) { *(uint32_t *)p = c; return; }
    uint32_t a = opa, na = 255 - a;
    p[0] = (uint8_t)(((c & 0xff) * a + p[0] * na) / 255);
    p[1] = (uint8_t)((((c >> 8) & 0xff) * a + p[1] * na) / 255);
    p[2] = (uint8_t)((((c >> 16) & 0xff) * a + p[2] * na) / 255);
}

void raster_line(lv_draw_buf_t *b, int32_t x0, int32_t y0, int32_t x1, int32_t y1, lv_color_t col, int w, uint8_t opa, int dash)
{
    int32_t W = (int32_t)b->header.w, H = (int32_t)b->header.h, stride = (int32_t)b->header.stride;
    /* изцяло извън буфера */
    if ((x0 < -8 && x1 < -8) || (y0 < -8 && y1 < -8) || (x0 > W + 8 && x1 > W + 8) || (y0 > H + 8 && y1 > H + 8)) return;
    uint32_t c = 0xff000000u | ((uint32_t)col.red << 16) | ((uint32_t)col.green << 8) | col.blue;
    int32_t dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    int lo = -(w - 1) / 2, hi = w / 2;
    bool steep = -dy > dx;   /* дебелината е напречно на посоката */
    long n = 0, lim = 4L * (W + H) + 16;
    for (;;) {
        bool on = !dash || (n / dash) % 2 == 0;
        if (on)
            for (int k = lo; k <= hi; k++) {
                int32_t x = steep ? x0 + k : x0, y = steep ? y0 : y0 + k;
                if (x >= 0 && y >= 0 && x < W && y < H) plot(b->data + y * stride, x, c, opa);
            }
        if ((x0 == x1 && y0 == y1) || ++n > lim) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
