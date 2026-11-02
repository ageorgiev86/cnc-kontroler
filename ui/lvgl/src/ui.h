/* Операторският интерфейс: създаване на екраните и опресняване от модела. */
#ifndef CNC_UI_H
#define CNC_UI_H

#include "lvgl.h"
#include "machine.h"

void ui_create(lv_display_t *disp);
/* Записва текущия екран като BMP (за снимки от симулатора). 0 при успех. */
int ui_save_bmp(const char *path);

/* --- вътрешно между модулите на интерфейса --- */
typedef struct {
    lv_obj_t *bar, *left, *right;
    lv_obj_t *reset, *pill, *pill_lbl, *mode, *mach, *gmodes_wcs, *tool, *mcu_led, *mcu;
    lv_obj_t *msg, *clock, *estop;
} topbar_t;

typedef struct {
    lv_obj_t *row, *val, *unit, *sm, *sd, *fbar, *ftxt, *dot;
} dro_row_t;

typedef struct {
    lv_obj_t *card, *home_btn, *seg_work, *seg_mach;
    dro_row_t rows[AX_N];
} dro_t;

void topbar_create(topbar_t *tb, lv_obj_t *parent);
void topbar_update(topbar_t *tb, uint32_t now);
void dro_create(dro_t *d, lv_obj_t *parent);
void dro_update(dro_t *d, uint32_t now);

/* Мигане като steps(2) в макета: първата половина на периода е видимо */
static inline bool ui_blink_on(uint32_t now, uint32_t period) { return now % period < period / 2; }

#endif
