/* Операторският интерфейс: създаване на екраните и опресняване от модела. */
#ifndef CNC_UI_H
#define CNC_UI_H

#include "lvgl.h"
#include "machine.h"

void ui_create(lv_display_t *disp);
void ui_load_config(const char *path);
const char *ui_config_path(void);   /* NULL = ~/.config/cnc-ui.conf */
/* Записва текущия екран като BMP (за снимки от симулатора). 0 при успех. */
int ui_save_bmp(const char *path);
/* За симулатора: екран по име и тестови действия */
int ui_go_name(const char *name);
void ui_test(const char *act);

/* --- вътрешно между модулите на интерфейса --- */
typedef struct {
    lv_obj_t *bar, *left, *right;
    lv_obj_t *reset, *pill, *pill_lbl, *mode, *mach, *gmodes_wcs, *tool, *mcu_led, *mcu;
    lv_obj_t *msg, *clock, *estop;
} topbar_t;

typedef struct {
    lv_obj_t *row, *val, *unit, *sm, *sd, *fbar, *ftxt, *dot;
} dro_row_t;

/* Видове на картата с позициите (data-dro в макета) */
typedef enum { DRO_FULL, DRO_ZERO, DRO_COMPACT } dro_kind_t;

typedef struct {
    dro_kind_t kind;
    lv_obj_t *card, *title, *home_btn, *seg_work, *seg_mach, *single, *cool;
    dro_row_t rows[AX_N];
    int need_was, single_was, cool_was, wcs_was;
} dro_t;

/* 3D траектория (lv_canvas) */
typedef struct {
    lv_obj_t *card, *body, *canvas, *base_cv, *leg, *info, *seg[5];
    lv_draw_buf_t *buf, *base, *done;   /* екран, основа, основа + изминатият път */
    int done_op;                         /* докъде е нарисуван изминатият път в done */
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
void traj_legend(traj_t *t);
void traj_reload(traj_t *t);
void gcode_reload(gcode_t *g);
void ui_program_changed(void);
void ui_run_from_line(void);     /* „Изпълни от ред“: избор на ред, старт с ЦИКЪЛ СТАРТ */   /* след зареждане на нова програма */
void gcode_create(gcode_t *g, lv_obj_t *parent);
void gcode_update(gcode_t *g, uint32_t now);
lv_obj_t *ctrl_create(ctrl_t *c, lv_obj_t *parent);
lv_obj_t *cbtns_create(lv_obj_t *parent);
void ctrl_update(ctrl_t *c, uint32_t now);

void topbar_create(topbar_t *tb, lv_obj_t *parent);
void topbar_update(topbar_t *tb, uint32_t now);
void dro_create(dro_t *d, lv_obj_t *parent, dro_kind_t kind);
void dro_update(dro_t *d, uint32_t now);

/* Екраните (навигацията .navbar) */
enum { SCR_RUN, SCR_JOG, SCR_MDI, SCR_TOOLS, SCR_PROBE, SCR_DIAG, SCR_SET, SCR_FILES, SCR_N };
extern int g_screen;
void ui_go(int scr);
lv_obj_t *ui_traj_card(void);            /* картата „Траектория“ се мести между Работа, Ръчно и MDI */
void ui_traj_slot(int scr, int col, int cspan, int row, int rspan);

void jog_create(lv_obj_t *scr);
void jog_update(uint32_t now);
void mdi_create(lv_obj_t *scr);
void mdi_update(uint32_t now);
void tools_create(lv_obj_t *scr);
void tools_update(uint32_t now);
void probe_create(lv_obj_t *scr);
void probe_update(uint32_t now);
void diag_create(lv_obj_t *scr);
void diag_update(uint32_t now);
void set_create(lv_obj_t *scr);
void set_update(uint32_t now);
void set_show(void);
void set_test(int level, const char *sec);
void set_test_edit(const char *key);
void diag_test_scroll(int y);
void files_create(lv_obj_t *scr);
void files_update(uint32_t now);
void files_show(void);
void files_test(int src, int sel);
void files_load(void);
void files_hide(void);
void hmap_open(void);
void hmap_label(char *b, size_t n);
void hmap_result(lv_obj_t *parent);
void hmap_tick(void);   /* тестови действия на отделните екрани */

/* Диалози (ui_dlg.c) */
typedef void (*dlg_num_cb)(double v, void *ud);
typedef int (*dlg_text_cb)(const char *txt, void *ud);   /* 0 = прието */
typedef void (*dlg_pick_cb)(int idx, void *ud);
void dlg_setpos(int ax);
void dlg_num(const char *title, const char *unit, const char *hint, double cur, double min, double max, int dec, dlg_num_cb cb, void *ud);
void dlg_text(const char *title, const char *cur, int maxlen, dlg_text_cb cb, void *ud);
void dlg_pick(const char *title, const char *const *opts, int n, int cur, dlg_pick_cb cb, void *ud);
void dlg_confirm(const char *title, const char *text, const char *ok, dlg_pick_cb cb, void *ud);
void dlg_close(void);
bool dlg_open(void);
bool dlg_key(uint32_t key);     /* физическа клавиатура; true = обработено */
bool mdi_key(uint32_t key);
void ui_key(uint32_t key);      /* клавиш: Unicode знак или LV_KEY_* */
void ui_keys_attach(lv_indev_t *kb);   /* клавиатурата праща към ui_key */

/* Мигане като steps(2) в макета: първата половина на периода е видимо */
static inline bool ui_blink_on(uint32_t now, uint32_t period) { return now % period < period / 2; }

#endif
