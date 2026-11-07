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

/* 3D траектория (lv_canvas) */
typedef struct {
    lv_obj_t *card, *body, *canvas, *base_cv, *leg, *info, *seg[5];
    lv_draw_buf_t *buf, *base;
    int32_t w, h;
    float yaw, pitch, zoom;
    int view;              /* 0 3D, 1 XY, 2 XZ, 3 YZ, -1 свободен */
    bool base_dirty;
    int drawn_op;
    float drawn_s, drawn_x, drawn_y, drawn_z;
    uint32_t drawn_at;
} traj_t;

/* G-code списък: виртуален (само видимите редове са обекти) */
#define GC_MAX_ROWS 64
typedef struct {
    lv_obj_t *card, *name, *pos, *list, *row[GC_MAX_ROWS], *num[GC_MAX_ROWS], *code[GC_MAX_ROWS];
    int nrows, row_h, first, cur, scroll;
} gcode_t;

/* оверрайди, напредък и бутоните на цикъла */
typedef struct {
    lv_obj_t *f_prog, *f_act, *f_ov, *s_prog, *s_act, *s_ov, *r_act, *r_btn[3];
    lv_obj_t *bar, *pct, *elapsed, *remain, *end;
    lv_obj_t *btn_single, *btn_cool;
} ctrl_t;

void traj_create(traj_t *t, lv_obj_t *parent);
void traj_update(traj_t *t, uint32_t now);
void gcode_create(gcode_t *g, lv_obj_t *parent);
void gcode_update(gcode_t *g, uint32_t now);
lv_obj_t *ctrl_create(ctrl_t *c, lv_obj_t *parent);
lv_obj_t *cbtns_create(lv_obj_t *parent);
void ctrl_update(ctrl_t *c, uint32_t now);

void topbar_create(topbar_t *tb, lv_obj_t *parent);
void topbar_update(topbar_t *tb, uint32_t now);
void dro_create(dro_t *d, lv_obj_t *parent);
void dro_update(dro_t *d, uint32_t now);

/* Мигане като steps(2) в макета: първата половина на периода е видимо */
static inline bool ui_blink_on(uint32_t now, uint32_t period) { return now % period < period / 2; }

#endif
