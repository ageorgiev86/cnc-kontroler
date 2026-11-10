/* Екранът „Настройки“ (.sets в макета).
 * Вход с PIN по нива (Настройчик, Производител), разделите вляво, формулярите от config_defs.c,
 * таблица по оси (оси, реферирване, енкодери), входове/изходи, връзка с MCU, макроси, достъп.
 * Промените са в черновата; „Запиши“ ги прилага, „Отмени промените“ ги връща. */
#include <math.h>
#include <stdio.h>
#include "net_apply.h"
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "io.h"
#include "ui.h"
#include "ui_style.h"

enum { S_MACH, S_IO, S_LUBE, S_AX, S_HOME, S_ENC, S_SP, S_MAG, S_PRB, S_TS, S_MCU, S_NET, S_DISP, S_LOGIC, S_ACC, S_N };
static const char *const SEC_K[S_N] = {"mach", "io", "lube", "ax", "home", "enc", "sp", "mag", "prb", "ts", "mcu", "net", "disp", "logic", "acc"};
static const char *const SEC_N[S_N] = {"Машина", "Входове/изходи", "Смазване", "Оси и мотори", "Реферирване", "Енкодери", "Шпиндел", "Магазин",
                                       "3D пробник", "Toolsetter", "Връзка с MCU", "Мрежа", "Дисплей", "Макроси и логика", "Достъп"};
static const int SEC_LVL[S_N] = {2, 2, 1, 2, 2, 2, 2, 2, 1, 1, 2, 1, 1, 2, 2};
static const char *const LVN[3] = {"Оператор", "Настройчик", "Производител"};

static lv_obj_t *root, *login, *main_grid, *nav, *nav_btn[S_N], *sec_title, *dirty_l, *who, *body, *help, *foot, *note;
static lv_obj_t *pin_l, *lv_btn[2], *login_btn;
static int sec = S_MACH, pick = 2, io_tab = 0, bad_pin, rebuild = 1, lg_sel = 0;
static char pin[12], help_key[40];
static uint32_t lock_until, last_act;

/* дневник на промените (в паметта) */
#define LOG_N 60
static struct { char ts[12], who[16], what[48], from[48], to[48]; } LOG[LOG_N];
static int log_n;

static const pdef_t *sec_form(int s, int *n)
{
    switch (s) {
    case S_MACH: *n = P_MACH_N; return P_MACH;
    case S_LUBE: *n = P_LUBE_N; return P_LUBE;
    case S_SP: *n = P_SP_N; return P_SP;
    case S_MAG: *n = P_MAG_N; return P_MAG;
    case S_PRB: *n = P_PRB_N; return P_PRB;
    case S_TS: *n = P_TS_N; return P_TS;
    case S_NET: *n = P_NET_N; return P_NET;
    case S_DISP: *n = P_DISP_N; return P_DISP;
    case S_AX: *n = P_AX_N; return P_AX;
    case S_HOME: *n = P_HOME_N; return P_HOME;
    case S_ENC: *n = P_ENC_N; return P_ENC;
    default: *n = 0; return NULL;
    }
}
static bool sec_axis(int s) { return s == S_AX || s == S_HOME || s == S_ENC; }

/* режимите на дисплея: „Автоматично“ и обичайните (в DRM идват от монитора) */
static const char *const DISP_OPTS[] = {"auto", "Автоматично", "1920x1200", "1920×1200", "1920x1080", "1920×1080", "1680x1050", "1680×1050",
    "1600x900", "1600×900", "1280x1024", "1280×1024", "1280x800", "1280×800", "1280x720", "1280×720", "1080x1920", "1080×1920",
    "1024x768", "1024×768", "1024x600", "1024×600", "800x1280", "800×1280", "800x600", "800×600", "800x480", "800×480", "600x1024", "600×1024", NULL};
static const char *const *opts_of(const pdef_t *p) { return p->opts ? p->opts : !strcmp(p->k, "dRes") ? DISP_OPTS : NULL; }

static void fmt_val(const pdef_t *p, const char *v, char *b, size_t n)
{
    const char *const *o;
    switch (p->t) {
    case PT_BOOL: snprintf(b, n, "%s", atof(v) ? p->on : p->off); break;
    case PT_DIR: snprintf(b, n, "%s", atof(v) > 0 ? "+" : "−"); break;
    case PT_ENUM: o = opts_of(p); snprintf(b, n, "%s", o ? opt_label(o, v) : v); break;
    case PT_IP: case PT_TEXT: snprintf(b, n, "%s", v[0] ? v : "—"); break;
    case PT_PASS: snprintf(b, n, "%s", v[0] ? "••••••" : "не е зададена"); break;
    default: snprintf(b, n, "%.*f", p->d, atof(v)); break;
    }
}

static void key_of(int s, const pdef_t *p, int ax, char *k, size_t n)
{
    if (ax >= 0) snprintf(k, n, "%s.%s", AX_NAME[ax], p->k);
    else snprintf(k, n, "%s.%s", SEC_K[s], p->k);
}

static void dirty(void) { rebuild = 1; }

/* ---------- редакция на стойност ---------- */
static char ed_key[40];
static const pdef_t *ed_p;

static void on_num_set(double v, void *ud) { drf_set_num(ed_key, v, ed_p->d); dirty(); }
static int on_text_set(const char *t, void *ud)
{
    if (ed_p->t == PT_IP) {
        int a, b, c, d;
        char x;
        if (t[0] && (sscanf(t, "%d.%d.%d.%d%c", &a, &b, &c, &d, &x) != 4 || a > 255 || b > 255 || c > 255 || d > 255 || a < 0 || b < 0 || c < 0 || d < 0)) {
            machine_toast("Невалиден адрес. Пример: 192.168.1.50", lv_tick_get(), 3500);
            return -1;
        }
    }
    if (!strcmp(ed_p->k, "host")) {
        for (const char *q = t; *q; q++)
            if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9') || *q == '-') || strlen(t) > 32) {
                machine_toast("Името може да съдържа само латински букви, цифри и тире", lv_tick_get(), 3500);
                return -1;
            }
    }
    if (!t[0] && ed_p->t != PT_PASS && strcmp(ed_p->k, "shUser") && strcmp(ed_p->k, "shPath")) {
        machine_toast("Полето не може да е празно", lv_tick_get(), 3000);
        return -1;
    }
    drf_set(ed_key, t);
    dirty();
    return 0;
}
static void on_pick_set(int i, void *ud) { drf_set(ed_key, opts_of(ed_p)[i * 2]); dirty(); }

static void edit(const pdef_t *p, const char *key)
{
    snprintf(ed_key, sizeof ed_key, "%s", key);
    snprintf(help_key, sizeof help_key, "%s", p->k);
    ed_p = p;
    const char *v = drf_str(key);
    if (p->t == PT_BOOL) {
        if (!strcmp(p->k, "active") && atof(v)) {   /* поне една активна ос */
            int n = 0;
            for (int a = 0; a < AX_N; a++) { char k[24]; snprintf(k, sizeof k, "%s.active", AX_NAME[a]); n += drf_num(k) != 0; }
            if (n <= 1) { machine_toast("Поне една ос трябва да е активна", lv_tick_get(), 3000); dirty(); return; }
        }
        drf_set(key, atof(v) ? "0" : "1");
        dirty();
    } else if (p->t == PT_DIR) {
        drf_set(key, atof(v) > 0 ? "-1" : "1");
        dirty();
    } else if (p->t == PT_ENUM) {
        const char *const *o = opts_of(p);
        const char *lab[32];
        int n = 0;
        for (; o[n * 2] && n < 32; n++) lab[n] = o[n * 2 + 1];
        dlg_pick(p->n, lab, n, opt_index(o, v), on_pick_set, NULL);
        dirty();
    } else if (p->t == PT_TEXT || p->t == PT_IP || p->t == PT_PASS) {
        dlg_text(p->n, p->t == PT_PASS ? "" : v, p->t == PT_IP ? 15 : 63, on_text_set, NULL);
        dirty();
    } else {
        char h[400];
        snprintf(h, sizeof h, "%s", p->h ? p->h : "");
        dlg_num(p->n, p->u, h, atof(v), p->min, cfg_max(p), p->d, on_num_set, NULL);
        dirty();
    }
}

/* бутоните носят индекс в таблицата на раздела и оста */
static void on_cell(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e), pi = k / 8, ax = k % 8 - 1, n;
    const pdef_t *P = sec_form(sec, &n);
    char key[40];
    key_of(sec, &P[pi], ax, key, sizeof key);
    edit(&P[pi], key);
}
static void on_seg(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e), pi = k / 64, oi = k % 64, n;
    const pdef_t *P = sec_form(sec, &n);
    char key[40];
    key_of(sec, &P[pi], -1, key, sizeof key);
    drf_set(key, opts_of(&P[pi])[oi * 2]);
    snprintf(help_key, sizeof help_key, "%s", P[pi].k);
    dirty();
}
static void on_help(lv_event_t *e)
{
    int pi = (int)(intptr_t)lv_event_get_user_data(e), n;
    const pdef_t *P = sec_form(sec, &n);
    snprintf(help_key, sizeof help_key, "%s", P[pi].k);
    dirty();
}

/* ---------- изграждане на редове ---------- */
static lv_obj_t *name_cell(lv_obj_t *g, const pdef_t *p, int pi, int row)
{
    lv_obj_t *c = ui_box(g);
    ui_flex(c, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_clickable(c, true);
    lv_obj_add_event_cb(c, on_help, LV_EVENT_CLICKED, (void *)(intptr_t)pi);
    bool h = !strcmp(help_key, p->k);
    lv_obj_t *n = ui_label(c, F.txt, h ? C_ACC : C_FG, p->n);
    lv_obj_set_width(n, LV_PCT(100));
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_WRAP);
    if (p->u) ui_label(c, F.txt_sm, C_DIM, p->u);
    lv_obj_set_grid_cell(c, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
    return c;
}

static lv_obj_t *val_btn(lv_obj_t *parent, const pdef_t *p, const char *key, void *ud)
{
    char b[80];
    fmt_val(p, drf_str(key), b, sizeof b);
    lv_obj_t *bt = ui_btn(parent, b, BTN_NORMAL, false);
    lv_obj_set_style_text_font(bt, p->t == PT_NUM || p->t == PT_IP ? F.num_md : F.txt, 0);
    lv_obj_set_style_text_letter_space(bt, 0, 0);
    lv_obj_set_height(bt, EM(2.6f));
    if (cfg_changed(key)) { lv_obj_set_style_border_color(bt, C_INFO, 0); lv_obj_set_style_border_width(bt, EM(0.14f), 0); }
    if (p->t == PT_BOOL && atof(drf_str(key)) && !p->rev) lv_obj_set_style_text_color(bt, C_OK, 0);
    if (p->rev && atof(drf_str(key))) lv_obj_set_style_text_color(bt, C_WARN, 0);
    lv_obj_add_event_cb(bt, on_cell, LV_EVENT_CLICKED, ud);
    return bt;
}

static void build_form(void)
{
    int n;
    const pdef_t *P = sec_form(sec, &n);
    static int32_t cols[3], rows[80];
    cols[0] = LV_GRID_FR(45); cols[1] = LV_GRID_FR(55); cols[2] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(body);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_style_max_width(g, EM(42), 0);
    int row = 0;
    for (int i = 0; i < n && row < 78; i++) {
        const pdef_t *p = &P[i];
        if (p->show && !p->show()) continue;
        rows[row] = LV_GRID_CONTENT;
        if (p->t == PT_HDR) {
            char up[64];
            ui_upper(up, sizeof up, p->n);
            lv_obj_t *h = ui_label(g, F.ui_xs, C_ACC, up);
            lv_obj_set_style_text_letter_space(h, EM(0.12f), 0);
            lv_obj_set_style_pad_top(h, EM(0.9f), 0);
            lv_obj_set_style_pad_bottom(h, EM(0.15f), 0);
            lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_color(h, C_LINE, 0);
            lv_obj_set_style_border_width(h, 1, 0);
            lv_obj_set_grid_cell(h, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_END, row, 1);
            row++;
            continue;
        }
        name_cell(g, p, i, row);
        char key[40];
        key_of(sec, p, -1, key, sizeof key);
        const char *const *o = opts_of(p);
        if (p->t == PT_ENUM && o && opt_count(o) <= 3) {   /* малко варианти: сегменти */
            lv_obj_t *s = ui_box(g);
            ui_flex(s, LV_FLEX_FLOW_ROW, 0.25f);
            for (int k = 0; o[k * 2]; k++) {
                bool on = !strcmp(drf_str(key), o[k * 2]);
                lv_obj_t *b = ui_btn(s, o[k * 2 + 1], on ? BTN_ON : BTN_NORMAL, true);
                lv_obj_set_flex_grow(b, 1);
                lv_obj_set_height(b, EM(2.6f));
                if (on && cfg_changed(key)) { lv_obj_set_style_border_color(b, C_INFO, 0); lv_obj_set_style_border_width(b, EM(0.14f), 0); }
                lv_obj_add_event_cb(b, on_seg, LV_EVENT_CLICKED, (void *)(intptr_t)(i * 64 + k));
            }
            lv_obj_set_grid_cell(s, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, row, 1);
        } else {
            lv_obj_t *b = val_btn(g, p, key, (void *)(intptr_t)(i * 8));
            lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, row, 1);
        }
        row++;
    }
    rows[row] = LV_GRID_TEMPLATE_LAST;
    ui_grid(g, cols, rows, 0.3f);
    lv_obj_set_style_pad_column(g, EM(0.8f), 0);
}

static void build_axis(void)
{
    int n;
    const pdef_t *P = sec_form(sec, &n);
    static int32_t cols[8], rows[16];
    cols[0] = LV_GRID_FR(24);
    for (int a = 0; a < AX_N; a++) cols[a + 1] = LV_GRID_FR(14);
    cols[7] = LV_GRID_TEMPLATE_LAST;
    for (int i = 0; i <= n; i++) rows[i] = LV_GRID_CONTENT;
    rows[n + 1] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(body);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, cols, rows, 0.3f);
    char key[40];
    for (int a = 0; a < AX_N; a++) {
        key_of(sec, &P_AX[0], a, key, sizeof key);
        lv_obj_t *l = ui_label(g, F.ui_md, drf_num(key) ? C_ACC : C_DIM, AX_NAME[a]);
        lv_obj_set_grid_cell(l, LV_GRID_ALIGN_CENTER, a + 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
    for (int i = 0; i < n; i++) {
        name_cell(g, &P[i], i, i + 1);
        for (int a = 0; a < AX_N; a++) {
            key_of(sec, &P[i], a, key, sizeof key);
            lv_obj_t *b = val_btn(g, &P[i], key, (void *)(intptr_t)(i * 8 + a + 1));
            lv_obj_set_style_pad_hor(b, EM(0.2f), 0);
            char ak[24], ek[24];
            snprintf(ak, sizeof ak, "%s.active", AX_NAME[a]);
            snprintf(ek, sizeof ek, "%s.encOn", AX_NAME[a]);
            bool off = (!drf_num(ak) && strcmp(P[i].k, "active")) || (sec == S_ENC && strcmp(P[i].k, "encOn") && !drf_num(ek));
            if (off) lv_obj_set_style_opa(b, 110, 0);
            lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, a + 1, 1, LV_GRID_ALIGN_CENTER, i + 1, 1);
        }
    }
}

/* ---------- входове и изходи ---------- */
static char io_key[40];
static void on_io_tab(lv_event_t *e) { io_tab = (int)(intptr_t)lv_event_get_user_data(e); dirty(); }
static int on_io_name(const char *t, void *ud) { drf_set(io_key, t); dirty(); return 0; }
static const char *const *io_pick_opts;
static void on_io_pick(int i, void *ud) { drf_set(io_key, io_pick_opts[i * 2]); dirty(); }
static void io_pick(const char *ttl, const char *const *o)
{
    io_pick_opts = o;
    const char *lab[32];
    int n = 0;
    for (; o[n * 2]; n++) lab[n] = o[n * 2 + 1];
    dlg_pick(ttl, lab, n, opt_index(o, drf_str(io_key)), on_io_pick, NULL);
}
static void on_io_num_set(double v, void *ud) { drf_set_num(io_key, v, 2); dirty(); }

/* user_data: тип (0 вход, 1 изход, 2 ан. вход, 3 ан. изход) * 10000 + канал * 100 + поле */
static void on_io(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e), d = k / 10000, ch = k / 100 % 100, f = k % 100;
    static const char *const DN[4] = {"in", "out", "ain", "aout"};
    static const char *const FLD[] = {"n", "f", "inv", "flt", "safe", "sig", "lo", "hi", "u", "amin", "amax", "act"};
    snprintf(io_key, sizeof io_key, "io.%s%d.%s", DN[d], ch + 1, FLD[f]);
    char t[64];
    snprintf(t, sizeof t, "%s%d", d == 0 ? "IN" : d == 1 ? "OUT" : d == 2 ? "AIN" : "AOUT", ch + 1);
    switch (f) {
    case 0: case 8: { char tt[120]; snprintf(tt, sizeof tt, "%s · %s", t, f ? "мерна единица" : "име"); dlg_text(tt, drf_str(io_key), 39, on_io_name, NULL); break; }
    case 1: io_pick(t, d == 0 ? IN_F : d == 1 ? OUT_F : d == 2 ? AIN_F : AOUT_F); break;
    case 2: drf_set(io_key, drf_num(io_key) ? "0" : "1"); dirty(); break;
    case 3: dlg_num("Филтър", "ms", "Потиска смущения по входа.", drf_num(io_key), 0, 1000, 0, on_io_num_set, NULL); break;
    case 4: drf_set(io_key, !strcmp(drf_str(io_key), "off") ? "keep" : "off"); dirty(); break;
    case 5: io_pick("Сигнал", SIG_T); break;
    case 11: io_pick("При нарушение", AACT_T); break;
    default: dlg_num(t, drf_str("io.ain1.u"), "Стойността в мерни единици.", drf_num(io_key), -100000, 100000, 2, on_io_num_set, NULL); break;
    }
}

static lv_obj_t *io_btn(lv_obj_t *g, const char *txt, int ud, const char *key, int col, int row)
{
    lv_obj_t *b = ui_btn(g, txt, BTN_NORMAL, true);
    lv_obj_set_style_text_font(b, F.txt_sm, 0);
    lv_obj_set_style_text_letter_space(b, 0, 0);
    lv_obj_set_style_pad_hor(b, EM(0.3f), 0);
    lv_label_set_long_mode(ui_btn_label(b), LV_LABEL_LONG_MODE_DOTS);
    if (cfg_changed(key)) { lv_obj_set_style_border_color(b, C_INFO, 0); lv_obj_set_style_border_width(b, EM(0.14f), 0); }
    lv_obj_add_event_cb(b, on_io, LV_EVENT_CLICKED, (void *)(intptr_t)ud);
    lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_CENTER, row, 1);
    return b;
}

static void build_io(void)
{
    lv_obj_t *s = ui_box(body);
    ui_flex(s, LV_FLEX_FLOW_ROW, 0.25f);
    static const char *const T[4] = {"ВХОДОВЕ (32)", "ИЗХОДИ (32)", "АНАЛОГОВИ ВХОДОВЕ (4)", "АНАЛОГОВИ ИЗХОДИ (2)"};
    for (int i = 0; i < 4; i++) lv_obj_add_event_cb(ui_btn(s, T[i], i == io_tab ? BTN_ON : BTN_NORMAL, true), on_io_tab, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    static int32_t cols[13], rows[40];
    lv_obj_t *g = ui_box(body);
    lv_obj_set_width(g, LV_PCT(100));
    char k[40], b[64];
    if (io_tab < 2) {
        cols[0] = EM(3.4f); cols[1] = LV_GRID_FR(30); cols[2] = LV_GRID_FR(30); cols[3] = LV_GRID_FR(12); cols[4] = LV_GRID_FR(12); cols[5] = LV_GRID_TEMPLATE_LAST;
        for (int i = 0; i <= IO_N; i++) rows[i] = LV_GRID_CONTENT;
        rows[IO_N + 1] = LV_GRID_TEMPLATE_LAST;
        ui_grid(g, cols, rows, 0.25f);
        static const char *const H[5] = {"", "ИМЕ", "ФУНКЦИЯ", "ЛОГИКА", ""};
        for (int c = 0; c < 5; c++) {
            lv_obj_t *l = ui_label(g, F.ui_xs, C_DIM, c == 4 ? (io_tab ? "ПРИ АВАРИЯ" : "ФИЛТЪР") : H[c]);
            lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, c, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        }
        const char *d = io_tab ? "out" : "in";
        for (int i = 0; i < IO_N; i++) {
            snprintf(b, sizeof b, "%s%d", io_tab ? "OUT" : "IN", i + 1);
            lv_obj_t *l = ui_label(g, F.num_sm, C_HI, b);
            lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, i + 1, 1);
            snprintf(k, sizeof k, "io.%s%d.n", d, i + 1);
            io_btn(g, drf_str(k)[0] ? drf_str(k) : "—", io_tab * 10000 + i * 100 + 0, k, 1, i + 1);
            snprintf(k, sizeof k, "io.%s%d.f", d, i + 1);
            io_btn(g, opt_label(io_tab ? OUT_F : IN_F, drf_str(k)), io_tab * 10000 + i * 100 + 1, k, 2, i + 1);
            snprintf(k, sizeof k, "io.%s%d.inv", d, i + 1);
            io_btn(g, drf_num(k) ? "обърната" : "нормална", io_tab * 10000 + i * 100 + 2, k, 3, i + 1);
            if (io_tab) {
                snprintf(k, sizeof k, "io.out%d.safe", i + 1);
                io_btn(g, !strcmp(drf_str(k), "off") ? "изключва се" : "остава", 10000 + i * 100 + 4, k, 4, i + 1);
            } else {
                snprintf(k, sizeof k, "io.in%d.flt", i + 1);
                snprintf(b, sizeof b, "%.0f ms", drf_num(k));
                io_btn(g, b, i * 100 + 3, k, 4, i + 1);
            }
        }
    } else {
        bool out = io_tab == 3;
        int n = out ? AOUT_N : AIN_N, nc = out ? 6 : 10;
        static const char *const H[10] = {"ИМЕ", "ФУНКЦИЯ", "СИГНАЛ", "НАЧАЛО", "КРАЙ", "МЯРКА", "МИН", "МАКС", "ДЕЙСТВИЕ", "ФИЛТЪР"};
        static const int FI[10] = {0, 1, 5, 6, 7, 8, 9, 10, 11, 3};
        static const char *const FK[10] = {"n", "f", "sig", "lo", "hi", "u", "amin", "amax", "act", "flt"};
        cols[0] = EM(3.4f);
        for (int c = 0; c < nc; c++) cols[c + 1] = LV_GRID_FR(c < 2 ? 20 : 10);
        cols[nc + 1] = LV_GRID_TEMPLATE_LAST;
        for (int i = 0; i <= n; i++) rows[i] = LV_GRID_CONTENT;
        rows[n + 1] = LV_GRID_TEMPLATE_LAST;
        ui_grid(g, cols, rows, 0.25f);
        for (int c = 0; c < nc; c++) {
            lv_obj_t *l = ui_label(g, F.ui_xs, C_DIM, H[c]);
            lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, c + 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        }
        for (int i = 0; i < n; i++) {
            snprintf(b, sizeof b, "%s%d", out ? "AOUT" : "AIN", i + 1);
            lv_obj_t *l = ui_label(g, F.num_sm, C_HI, b);
            lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, i + 1, 1);
            for (int c = 0; c < nc; c++) {
                snprintf(k, sizeof k, "io.%s%d.%s", out ? "aout" : "ain", i + 1, FK[c]);
                const char *v = drf_str(k);
                if (c == 1) snprintf(b, sizeof b, "%s", opt_label(out ? AOUT_F : AIN_F, v));
                else if (c == 2) snprintf(b, sizeof b, "%s", opt_label(SIG_T, v));
                else if (c == 8) snprintf(b, sizeof b, "%s", opt_label(AACT_T, v));
                else if (c == 0 || c == 5) snprintf(b, sizeof b, "%s", v[0] ? v : "—");
                else snprintf(b, sizeof b, "%g", atof(v));
                io_btn(g, b, (out ? 3 : 2) * 10000 + i * 100 + FI[c], k, c + 1, i + 1);
            }
        }
    }
}

/* ---------- макроси и логика (преглед; изпълнението идва с интерпретатора) ---------- */
static const struct { int m; const char *n, *code; } MAC[] = {
    {100, "Затягане на детайла", "O100 (M100: ЗАТЯГАНЕ НА ДЕТАЙЛА)\nM64 P13 (OUT13: втора стяга)\nM66 P11 L3 Q2 (чакай IN11 „стегнато“ до 2 s)\nIF [#5399 LT 0] THEN #3000=1 (СТЯГАТА НЕ ЗАТЯГА)\nM99"},
    {101, "Разтягане", "O101 (M101: РАЗТЯГАНЕ)\nM65 P13\nG4 P0.5\nM99"},
    {110, "Вакуумна маса, двете зони", "O110 (M110: ВАКУУМ ЗОНИ 1 И 2)\nM64 P10\nM64 P11\nM66 P7 L3 Q5 (чакай „Вакуум ОК“)\nM99"},
};
static void on_mac(lv_event_t *e) { lg_sel = (int)(intptr_t)lv_event_get_user_data(e); dirty(); }

static void build_logic(void)
{
    ui_lab(body, "МАКРОСИ M100–M199");
    lv_obj_t *s = ui_box(body);
    ui_flex(s, LV_FLEX_FLOW_ROW_WRAP, 0.3f);
    lv_obj_set_width(s, LV_PCT(100));
    for (int i = 0; i < 3; i++) {
        char t[64];
        snprintf(t, sizeof t, "M%d · %s", MAC[i].m, MAC[i].n);
        lv_obj_t *b = ui_btn(s, t, i == lg_sel ? BTN_ON : BTN_NORMAL, true);
        lv_obj_add_event_cb(b, on_mac, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_t *c = ui_label(body, F.num_gc, C_FG, MAC[lg_sel].code);
    lv_obj_set_width(c, LV_PCT(100));
    lv_obj_set_style_bg_color(c, C_PANEL, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, C_LINE, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, EM(0.35f), 0);
    lv_obj_set_style_pad_all(c, EM(0.6f), 0);
    ui_lab(body, "ПРАВИЛА (PLC)");
    static const char *const R[4] = {"IN2 Налягане въздух = 0 за 1 s → Аларма", "IN1 Врата затворена = 0 → Пауза на подаването",
                                     "IN4 Бутон Старт (пулт) = 1 → Цикъл старт", "IN9 Контейнер за стружки пълен = 1 за 2 s → Предупреждение (изкл.)"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *l = ui_label(body, F.txt, i == 3 ? C_FAINT : C_FG, R[i]);
        lv_obj_set_width(l, LV_PCT(100));
    }
}

/* ---------- връзка с MCU и достъп ---------- */
static void kv_list(const char *const kv[][2], int n)
{
    for (int i = 0; i < n; i++) {
        lv_obj_t *v;
        lv_obj_t *r = ui_kv(body, kv[i][0], &v);
        lv_obj_set_style_max_width(r, EM(30), 0);
        lv_obj_set_style_text_font(lv_obj_get_child(r, 0), F.txt, 0);
        lv_obj_set_style_text_font(v, F.num_md, 0);
        ui_set_text(v, kv[i][1]);
    }
}

static int pin_lvl;
static void on_pin_set(double v, void *ud)
{
    char t[16];
    snprintf(t, sizeof t, "%.0f", v);
    if (strlen(t) < 4) { machine_toast("PIN трябва да е поне 4 цифри", lv_tick_get(), 3000); return; }
    drf_set(pin_lvl == 1 ? "acc.pin1" : "acc.pin2", t);
    cfg_commit_key(pin_lvl == 1 ? "acc.pin1" : "acc.pin2");
    if (log_n < LOG_N) { memmove(&LOG[1], &LOG[0], sizeof LOG[0] * (size_t)log_n); log_n++; }
    snprintf(LOG[0].ts, sizeof LOG[0].ts, "сега");
    snprintf(LOG[0].who, sizeof LOG[0].who, "%s", LVN[M.acc_level]);
    snprintf(LOG[0].what, sizeof LOG[0].what, "PIN %s", LVN[pin_lvl]);
    snprintf(LOG[0].from, sizeof LOG[0].from, "••••");
    snprintf(LOG[0].to, sizeof LOG[0].to, "••••");
    char m[64];
    snprintf(m, sizeof m, "PIN за %s е сменен", LVN[pin_lvl]);
    machine_toast(m, lv_tick_get(), 3500);
    dirty();
}
static void on_pin(lv_event_t *e)
{
    pin_lvl = (int)(intptr_t)lv_event_get_user_data(e);
    dlg_num("Нов PIN", "", "От 4 до 8 цифри. Не използвайте 0000, 1234 и рождени дати.", 0, 0, 99999999, 0, on_pin_set, NULL);
}
static void on_tmo_set(double v, void *ud) { drf_set_num("acc.tmo", v, 0); cfg_commit_key("acc.tmo"); dirty(); }
static void on_tmo(lv_event_t *e) { dlg_num("Автоматичен изход", "мин", "След толкова минути без действие настройките се затварят сами.", cfg_num("acc.tmo"), 1, 120, 0, on_tmo_set, NULL); }

static void build_acc(void)
{
    static int32_t cols[] = {LV_GRID_FR(45), LV_GRID_FR(55), LV_GRID_TEMPLATE_LAST}, rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *g = ui_box(body);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_style_max_width(g, EM(42), 0);
    ui_grid(g, cols, rows, 0.3f);
    static const char *const N[4] = {"PIN Настройчик", "PIN Производител", "Автоматичен изход, мин без действие", "Сериен номер"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *l = ui_label(g, F.txt, C_FG, N[i]);
        lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, i, 1);
        char t[16];
        snprintf(t, sizeof t, "%.0f", cfg_num("acc.tmo"));
        lv_obj_t *b = ui_btn(g, i < 2 ? "СМЕНИ" : i == 2 ? t : "CNC-2026-0001", BTN_NORMAL, false);
        lv_obj_set_height(b, EM(2.6f));
        if (i < 2) lv_obj_add_event_cb(b, on_pin, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
        else if (i == 2) lv_obj_add_event_cb(b, on_tmo, LV_EVENT_CLICKED, NULL);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, i, 1);
    }
    ui_lab(body, "ДНЕВНИК НА ПРОМЕНИТЕ");
    if (!log_n) ui_label(body, F.txt, C_DIM, "Още няма записани промени.");
    for (int i = 0; i < log_n; i++) {
        char t[300];
        snprintf(t, sizeof t, "%.11s · %.15s · %.47s: %.47s → %.47s", LOG[i].ts, LOG[i].who, LOG[i].what, LOG[i].from, LOG[i].to);
        lv_obj_t *l = ui_label(body, F.num_sm, C_FG, t);
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    }
}

/* ---------- бутони „вземи текущата позиция“ ---------- */
static void on_teach(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    uint32_t now = lv_tick_get();
    bool needz = k == 1 || k == 4, needxy = k != 1 && k != 4;
    if ((needz && !M.homed[AX_Z]) || (needxy && (!M.homed[AX_X] || !M.homed[AX_Y]))) {
        machine_toast("Осите не са реферирани. Позицията няма да е вярна. Натиснете HOME.", now, 4000);
        return;
    }
    switch (k) {
    case 0: drf_set_num("mag.mcX", M.mach[AX_X], 3); drf_set_num("mag.mcY", M.mach[AX_Y], 3); drf_set_num("mag.mcZ", M.mach[AX_Z], 3); break;
    case 1: drf_set_num("mag.mz", M.mach[AX_Z], 3); break;
    case 2: drf_set_num("mag.m1x", M.mach[AX_X], 3); drf_set_num("mag.m1y", M.mach[AX_Y], 3); break;
    case 3: drf_set_num("ts.tsx", M.mach[AX_X], 3); drf_set_num("ts.tsy", M.mach[AX_Y], 3); break;
    case 4: drf_set_num("ts.tsz", M.mach[AX_Z], 3); break;
    case 5: drf_set_num("prb.prRingX", M.mach[AX_X], 3); drf_set_num("prb.prRingY", M.mach[AX_Y], 3); break;
    case 6:   /* позиция за смяна (карусел, верига): и двете отметки се включват */
        drf_set_num("mag.mX", M.mach[AX_X], 3); drf_set_num("mag.mY", M.mach[AX_Y], 3);
        drf_set("mag.mXOn", "1"); drf_set("mag.mYOn", "1");
        break;
    }
    machine_toast("Взета е текущата позиция. Натиснете „Запиши“.", now, 3500);
    dirty();
}
static void on_cal(lv_event_t *e) { ui_go(SCR_PROBE); machine_probe_cycle("cal", 0, lv_tick_get()); }

static void teach_row(void)
{
    lv_obj_t *r = ui_box(body);
    ui_flex(r, LV_FLEX_FLOW_ROW_WRAP, 0.4f);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_style_pad_top(r, EM(0.6f), 0);
#define T(t, k) lv_obj_add_event_cb(ui_btn(r, t, BTN_NORMAL, false), on_teach, LV_EVENT_CLICKED, (void *)(intptr_t)(k))
    if (sec == S_MAG) {
        bool mon = drf_num("mag.mOn"), atc = drf_num("mag.atcOn");
        if (!mon || !atc) T("РЪЧНА СМЯНА = ТЕКУЩАТА X Y Z", 0);
        if (mon && DS_IS("mag.mtype", "linear")) T("ГНЕЗДО 1 = ТЕКУЩАТА X Y", 2);
        if (mon && !DS_IS("mag.mtype", "linear")) T("ПОЗИЦИЯ ЗА СМЯНА = ТЕКУЩАТА X Y", 6);
        if (mon) T("Z ЗА СМЯНА = ТЕКУЩАТА Z", 1);
    } else if (sec == S_TS) {
        T("ВЗЕМИ ТЕКУЩАТА X Y", 3);
        T("Z ЗА НАЧАЛО = ТЕКУЩАТА Z", 4);
    } else if (sec == S_PRB) {
        T("ЦЕНТЪР НА ПРЪСТЕНА = ТЕКУЩАТА X Y", 5);
        lv_obj_add_event_cb(ui_btn(r, "КАЛИБРИРАЙ В ПРЪСТЕНА", BTN_ON, false), on_cal, LV_EVENT_CLICKED, NULL);
    }
#undef T
}

static const char *sec_help(void)
{
    switch (sec) {
    case S_AX: return "Натиснете клетка, за да я промените. Числата се въвеждат с клавиатура, а Да/Не и посоките се превключват с едно натискане. Оранжевото означава обърната посока.";
    case S_HOME: return "Натиснете име на параметър, за да видите какво прави. Промени в посоката или позициите изискват ново реферирване.";
    case S_ENC: return "Изключете енкодера на оста, на която няма закачен. Тогава тя работи в отворен контур, а параметрите ѝ за енкодера не се използват. Входът е RS422 A/B/Z.";
    case S_IO: return "Името и функцията се виждат в „Диагностика“, в макросите и в правилата. „Потребителски“ вход или изход се ползва от G-code (M62–M66), от макрос или от правило. Логиката обръща състоянието, филтърът потиска смущения, „При авария“ определя какво прави изходът при авариен стоп.";
    case S_LUBE: return "Помпата се управлява от изхода с функция „Мазане“, а нивото на маслото и налягането се четат от входовете „Ниво масло“ и „Налягане на мазането“ (Настройки → Входове/изходи).";
    case S_PRB: return "Калибриране: центрирайте шпиндела над пръстена с индикатор и натиснете „Център на пръстена = текущата X Y“. После сложете пробника в пръстена и натиснете „Калибрирай“.";
    case S_MAG: return "Изберете типа и броя гнезда. При линеен магазин задайте и позицията на гнездо 1 и стъпката.";
    case S_DISP: return "Режимът на екрана и завъртането. Прилага се при следващото пускане на интерфейса (собственият DRM драйвер е следваща стъпка).";
    case S_MCU: return "Само за преглед: параметрите на връзката се задават във фърмуера на STM32.";
    case S_LOGIC: return "Макросите M100–M199 и правилата се изпълняват от интерпретатора и PLC слоя, които идват след стабилната основа (docs/makrosi-logika.md). Тук е прегледът им.";
    case S_ACC: return "PIN кодовете се пазят на контролера. Дневникът се пази и след изключване и не може да се изтрие от екрана.";
    default: return "Натиснете име на параметър, за да видите какво прави.";
    }
}

static void build(void)
{
    rebuild = 0;
    lv_obj_clean(body);
    lv_obj_scroll_to_y(body, 0, LV_ANIM_OFF);
    int n;
    const pdef_t *P = sec_form(sec, &n);
    if (sec_axis(sec)) build_axis();
    else if (P) { build_form(); teach_row(); }
    else if (sec == S_IO) build_io();
    else if (sec == S_LOGIC) build_logic();
    else if (sec == S_ACC) build_acc();
    else if (sec == S_MCU) {
        static const char *const KV[][2] = {{"Интерфейс", "SPI0 · 16 MHz"}, {"Цикъл", "1 kHz"}, {"Пакет", "128 B + CRC"}, {"Watchdog", "20 ms"}, {"MCU", "STM32H723"}};
        kv_list(KV, 5);
    }
    /* помощ: параметърът, който е натиснат, или общият текст на раздела */
    char h[900] = "";
    for (int i = 0; P && i < n; i++)
        if (P[i].k && !strcmp(P[i].k, help_key)) {
            snprintf(h, sizeof h, "%s. %s%s%s", P[i].n, P[i].h ? P[i].h : "", P[i].ref ? "\n" : "", P[i].ref ? P[i].ref : "");
            break;
        }
    ui_set_text(help, h[0] ? h : sec_help());
    lv_obj_set_hidden(foot, !(P || sec == S_IO));
    for (int i = 0; i < S_N; i++) {
        bool ok = M.acc_level >= SEC_LVL[i];
        ui_btn_set_kind(nav_btn[i], i == sec ? BTN_ON : BTN_NORMAL);
        lv_obj_set_style_opa(nav_btn[i], ok ? LV_OPA_COVER : 102, 0);
    }
    char up[64];
    ui_upper(up, sizeof up, SEC_N[sec]);
    ui_set_text(sec_title, up);
}

/* ---------- запис ---------- */
static void log_change(const char *what, const char *from, const char *to)
{
    if (log_n < LOG_N) log_n++;
    memmove(&LOG[1], &LOG[0], sizeof LOG[0] * (size_t)(log_n - 1));
    snprintf(LOG[0].ts, sizeof LOG[0].ts, "сега");
    snprintf(LOG[0].who, sizeof LOG[0].who, "%s", LVN[M.acc_level]);
    snprintf(LOG[0].what, sizeof LOG[0].what, "%s", what);
    snprintf(LOG[0].from, sizeof LOG[0].from, "%s", from);
    snprintf(LOG[0].to, sizeof LOG[0].to, "%s", to);
}

static void on_save(lv_event_t *e)
{
    uint32_t now = lv_tick_get();
    if (M.state == MS_RUN || M.state == MS_HOLD) { machine_toast("Спрете машината, преди да запишете настройките", now, 3500); return; }
    if (!cfg_dirty()) { machine_toast("Няма промени за запис", now, 2500); return; }
    if (drf_num("mag.mn") > cfg_max(&P_MAG[4])) {
        char t[96];
        snprintf(t, sizeof t, "Магазинът има най-много %.0f гнезда. Намалете броя.", cfg_max(&P_MAG[4]));
        machine_toast(t, now, 4000);
        return;
    }
    /* статичен адрес: трите полета са задължителни (DNS по избор, тогава е шлюзът) */
    if (!strcmp(drf_str("net.mode"), "static") && (!drf_str("net.ip")[0] || !drf_str("net.mask")[0] || !drf_str("net.gw")[0])) {
        machine_toast("Попълнете IP, маска и шлюз", now, 4000);
        return;
    }
    bool net_addr = cfg_changed("net.mode") || cfg_changed("net.ip") || cfg_changed("net.mask") || cfg_changed("net.gw") || cfg_changed("net.dns");
    bool net_host = cfg_changed("net.host");
    /* осите според вида и профила (axProf в макета) */
    bool type_ch = cfg_changed("mach.type"), ax_ch = type_ch || cfg_changed("mach.miAx") || cfg_changed("mach.lzAx") || cfg_changed("mach.plAx") || cfg_changed("mach.ltY");
    if (ax_ch) {
        const char *t = drf_str("mach.type"), *pr = !strcmp(t, "mill") ? drf_str("mach.miAx") : !strcmp(t, "laser") ? drf_str("mach.lzAx") : drf_str("mach.plAx");
        for (int a = 0; a < AX_N; a++) {
            bool on;
            if (!strcmp(t, "lathe")) on = a == AX_X || a == AX_Z || (a == AX_Y && drf_num("mach.ltY"));
            else on = a <= AX_Z || !strcmp(pr, "6") || (a == AX_A && strchr(pr, 'a')) || (a == AX_B && strchr(pr, 'b')) || (a == AX_C && strchr(pr, 'c'));
            char k[24];
            snprintf(k, sizeof k, "%s.active", AX_NAME[a]);
            drf_set(k, on ? "1" : "0");
        }
    }
    /* промени, които изискват ново реферирване (REHOME в макета) */
    static const char *const RE[] = {"mdir", "edir", "encRes", "spm", "hsw", "hdir", "hswp", "hpos", "hidx", "active"};
    char re[32] = "";
    for (int a = 0; a < AX_N; a++)
        for (unsigned i = 0; i < sizeof RE / sizeof RE[0]; i++) {
            char k[24];
            snprintf(k, sizeof k, "%s.%s", AX_NAME[a], RE[i]);
            if (cfg_changed(k)) {
                M.homed[a] = HOMED_NO;
                if (!strstr(re, AX_NAME[a])) { if (re[0]) strcat(re, ", "); strcat(re, AX_NAME[a]); }
                break;
            }
        }
    /* дневник: всички променени параметри от таблиците */
    const pdef_t *L[11] = {P_MACH, P_LUBE, P_SP, P_MAG, P_PRB, P_TS, P_NET, P_DISP, P_AX, P_HOME, P_ENC};
    const int LN[11] = {P_MACH_N, P_LUBE_N, P_SP_N, P_MAG_N, P_PRB_N, P_TS_N, P_NET_N, P_DISP_N, P_AX_N, P_HOME_N, P_ENC_N};
    const char *LS[8] = {"mach", "lube", "sp", "mag", "prb", "ts", "net", "disp"};
    for (int s = 0; s < 11; s++)
        for (int i = 0; i < LN[s]; i++) {
            if (!L[s][i].k) continue;
            for (int a = s < 8 ? -1 : 0; a < (s < 8 ? 0 : AX_N); a++) {
                char k[40], f[40], t[40], w[48];
                if (a < 0) snprintf(k, sizeof k, "%s.%s", LS[s], L[s][i].k); else snprintf(k, sizeof k, "%s.%s", AX_NAME[a], L[s][i].k);
                if (!cfg_changed(k)) continue;
                fmt_val(&L[s][i], cfg_str(k), f, sizeof f);
                fmt_val(&L[s][i], drf_str(k), t, sizeof t);
                snprintf(w, sizeof w, "%s%s%s", a >= 0 ? AX_NAME[a] : "", a >= 0 ? " · " : "", L[s][i].n);
                log_change(w, L[s][i].t == PT_PASS ? "••••" : f, L[s][i].t == PT_PASS ? "••••" : t);
            }
        }
    bool disp_ch = cfg_changed("disp.dRes") || cfg_changed("disp.dRot");
    cfg_commit();
    machine_apply_cfg();
    char t[160];
    if (type_ch) snprintf(t, sizeof t, "Машината е „%s“. Натиснете HOME.", opt_label(P_MACH[0].opts, cfg_str("mach.type")));
    else if (disp_ch) snprintf(t, sizeof t, "Записано. Новият режим на екрана важи след рестарт на интерфейса.");
    else if (re[0]) snprintf(t, sizeof t, "Записано. Осите %s трябва да се реферират отново.", re);
    else snprintf(t, sizeof t, "Настройките са записани");
    if (net_addr || net_host) {   /* мрежата на системата: отговорът на cnc-net (ОК … или ГРЕШКА …) */
        char nm[300];
        bool ok = net_apply(net_addr, net_host, nm, sizeof nm);
        dlg_confirm(ok ? "Мрежа" : "Мрежата не е сменена", nm, "ОК", NULL, NULL);   /* важно: новият адрес за SSH */
        dirty();
        return;
    }
    machine_toast(t, now, 6000);
    dirty();
}

static void on_revert(lv_event_t *e) { cfg_revert(); machine_toast("Промените са отменени", lv_tick_get(), 3000); dirty(); }

static void logout(const char *why)
{
    M.acc_level = 0;
    pin[0] = 0;
    char t[80];
    snprintf(t, sizeof t, "Изход от настройките%s", why);
    machine_toast(t, lv_tick_get(), 3500);
    set_show();
}
static void on_logout(lv_event_t *e) { logout(""); }
static void on_nav(lv_event_t *e)
{
    int s = (int)(intptr_t)lv_event_get_user_data(e);
    if (M.acc_level < SEC_LVL[s]) {
        char t[80];
        snprintf(t, sizeof t, "Този раздел изисква ниво „%s“", LVN[SEC_LVL[s]]);
        machine_toast(t, lv_tick_get(), 3000);
        return;
    }
    sec = s;
    help_key[0] = 0;
    dirty();
}

/* ---------- вход ---------- */
static void login_render(void)
{
    char b[16];
    size_t n = strlen(pin);
    for (size_t i = 0; i < n; i++) b[i] = '*';
    b[n] = 0;
    ui_set_text(pin_l, b);
    ui_seg_set(lv_btn, 2, pick - 1);
    char t[48];
    uint32_t now = lv_tick_get();
    if ((int32_t)(lock_until - now) > 0) snprintf(t, sizeof t, "ИЗЧАКАЙТЕ %u s", (unsigned)((lock_until - now) / 1000 + 1));
    else snprintf(t, sizeof t, "ВХОД КАТО %s", pick == 1 ? "НАСТРОЙЧИК" : "ПРОИЗВОДИТЕЛ");
    ui_set_text(ui_btn_label(login_btn), t);
}
static void on_lv(lv_event_t *e) { pick = (int)(intptr_t)lv_event_get_user_data(e); pin[0] = 0; login_render(); }
static void on_pk(lv_event_t *e)
{
    const char *k = lv_event_get_user_data(e);
    size_t n = strlen(pin);
    if (!strcmp(k, "C")) pin[0] = 0;
    else if (!strcmp(k, "<")) { if (n) pin[n - 1] = 0; }
    else if (n < 8) { pin[n] = k[0]; pin[n + 1] = 0; }
    login_render();
}
static void on_login(lv_event_t *e)
{
    uint32_t now = lv_tick_get();
    if ((int32_t)(lock_until - now) > 0) return;
    if (!strcmp(pin, cfg_str(pick == 1 ? "acc.pin1" : "acc.pin2"))) {
        M.acc_level = pick;
        bad_pin = 0;
        last_act = now;
        sec = pick == 1 ? S_LUBE : S_MACH;
        char t[64];
        snprintf(t, sizeof t, "Вход като %s", LVN[pick]);
        machine_toast(t, now, 3000);
        set_show();
    } else {
        if (++bad_pin >= 3) { lock_until = now + 30000; bad_pin = 0; machine_toast("3 грешни опита. Изчакайте 30 секунди.", now, 4000); }
        else machine_toast("Грешен PIN", now, 2500);
    }
    pin[0] = 0;
    login_render();
}

static void login_create(lv_obj_t *scr)
{
    lv_obj_t *c = login = ui_card(scr, "ВХОД В НАСТРОЙКИТЕ", NULL);
    lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
    lv_obj_t *b = ui_body(c, 0.6f);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *col = ui_box(b);
    lv_obj_set_width(col, EM(26));
    ui_flex(col, LV_FLEX_FLOW_COLUMN, 0.6f);
    lv_obj_t *lv = ui_box(col);
    lv_obj_set_width(lv, LV_PCT(100));
    ui_flex(lv, LV_FLEX_FLOW_ROW, 0.5f);
    static const char *const SUB[2] = {"смазване, пробник, toolsetter, мрежа, дисплей", "всички настройки"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *x = lv_btn[i] = ui_btn(lv, "", BTN_NORMAL, false);
        lv_obj_delete(ui_btn_label(x));
        lv_obj_set_flex_grow(x, i ? 1 : 2);
        lv_obj_set_height(x, EM(4.2f));
        ui_flex(x, LV_FLEX_FLOW_COLUMN, 0.1f);
        lv_obj_set_flex_align(x, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        ui_label(x, F.ui_md, C_FG, LVN[i + 1]);
        lv_obj_t *s = ui_label(x, F.txt_sm, C_DIM, SUB[i]);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_event_cb(x, on_lv, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
    }
    lv_obj_t *pb = ui_box(col);
    lv_obj_set_size(pb, LV_PCT(100), EM(3.2f));
    lv_obj_set_style_bg_color(pb, C_PANEL, 0);
    lv_obj_set_style_bg_opa(pb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pb, C_LINE, 0);
    lv_obj_set_style_border_width(pb, 1, 0);
    lv_obj_set_style_radius(pb, EM(0.35f), 0);
    ui_flex(pb, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(pb, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    pin_l = ui_label(pb, F.num_ov, C_FG, "");
    static int32_t c3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, r4[5];
    for (int i = 0; i < 4; i++) r4[i] = EM(g_portrait ? 3.2f : 4.2f);
    r4[4] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(col);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, c3, r4, 0.45f);
    static const char *const K[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", "<"};
    for (int i = 0; i < 12; i++) {
        lv_obj_t *x = ui_btn(g, i == 11 ? "DEL" : K[i], BTN_NORMAL, false);
        lv_obj_set_style_text_font(x, i == 11 ? F.ui_sm : F.num_pg, 0);
        lv_obj_add_event_cb(x, on_pk, LV_EVENT_CLICKED, (void *)K[i]);
        ui_cell(x, i % 3, 1, i / 3, 1);
    }
    login_btn = ui_btn(col, "", BTN_GO, false);
    lv_obj_set_width(login_btn, LV_PCT(100));
    lv_obj_add_event_cb(login_btn, on_login, LV_EVENT_CLICKED, NULL);
    lv_obj_t *h = ui_label(c, F.txt, C_DIM, "Операторът не влиза в настройките. Таблицата на инструментите, нулирането на G54 и пробингът са достъпни без парола.\n"
                                             "#4e5e7a PIN по подразбиране: 1111 за Настройчик и 2222 за Производител (сменят се в „Достъп“).#");
    lv_label_set_recolor(h, true);
    lv_obj_set_width(h, LV_PCT(100));
    lv_label_set_long_mode(h, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_pad_all(h, EM(0.7f), 0);
    lv_obj_set_style_border_side(h, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(h, C_LINE, 0);
    lv_obj_set_style_border_width(h, 1, 0);
    login_render();
}

void set_create(lv_obj_t *scr)
{
    root = scr;
    login_create(scr);
    lv_obj_t *g = main_grid = ui_box(scr);
    lv_obj_set_size(g, LV_PCT(100), LV_PCT(100));
    static int32_t cols[3], rows[3];
    if (g_portrait) {
        cols[0] = LV_GRID_FR(1); cols[1] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_CONTENT; rows[1] = LV_GRID_FR(1); rows[2] = LV_GRID_TEMPLATE_LAST;
    } else {
        cols[0] = EM(15); cols[1] = LV_GRID_FR(1); cols[2] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_FR(1); rows[1] = LV_GRID_TEMPLATE_LAST;
    }
    ui_grid(g, cols, rows, 0.6f);
    nav = ui_card(g, NULL, NULL);
    lv_obj_set_style_pad_all(nav, EM(0.4f), 0);
    lv_obj_set_style_pad_gap(nav, EM(0.3f), 0);
    if (g_portrait) {
        ui_flex(nav, LV_FLEX_FLOW_ROW_WRAP, 0.3f);
        lv_obj_set_width(nav, LV_PCT(100));
        lv_obj_set_grid_cell(nav, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    } else {
        ui_scroll_v(nav);
        ui_cell(nav, 0, 1, 0, 1);
    }
    for (int i = 0; i < S_N; i++) {
        lv_obj_t *b = nav_btn[i] = ui_btn(nav, SEC_N[i], BTN_NORMAL, false);
        lv_obj_set_style_text_font(b, g_portrait ? F.ui_sm : F.ui_md, 0);
        lv_obj_set_style_text_letter_space(b, EM(0.02f), 0);
        if (g_portrait) { lv_obj_set_width(b, LV_PCT(24)); lv_obj_set_height(b, EM(2.4f)); lv_obj_set_style_pad_hor(b, EM(0.2f), 0); }
        else { lv_obj_set_width(b, LV_PCT(100)); lv_obj_set_height(b, EM(2.8f)); lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); }
        lv_obj_add_event_cb(b, on_nav, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_t *hr;
    lv_obj_t *m = ui_card(g, "", &hr);
    if (g_portrait) ui_cell(m, 0, 1, 1, 1); else ui_cell(m, 1, 1, 0, 1);
    sec_title = lv_obj_get_child(lv_obj_get_child(m, 0), 0);
    dirty_l = ui_label(hr, F.txt_sm, C_WARN, "");
    who = ui_label(hr, F.ui_sm, C_ACC, "");
    lv_obj_add_event_cb(ui_btn(hr, "ИЗХОД", BTN_NORMAL, true), on_logout, LV_EVENT_CLICKED, NULL);
    body = ui_box(m);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_all(body, EM(0.7f), 0);
    ui_scroll_v(body);
    ui_flex(body, LV_FLEX_FLOW_COLUMN, 0.5f);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    help = ui_label(m, F.txt, C_DIM, "");
    lv_obj_set_width(help, LV_PCT(100));
    lv_label_set_long_mode(help, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_min_height(help, EM(3.4f), 0);
    lv_obj_set_style_pad_all(help, EM(0.7f), 0);
    lv_obj_set_style_border_side(help, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(help, C_LINE, 0);
    lv_obj_set_style_border_width(help, 1, 0);
    foot = ui_box(m);
    lv_obj_set_width(foot, LV_PCT(100));
    lv_obj_set_style_pad_all(foot, EM(0.6f), 0);
    lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(foot, C_LINE, 0);
    lv_obj_set_style_border_width(foot, 1, 0);
    ui_flex(foot, LV_FLEX_FLOW_ROW, 0.5f);
    lv_obj_set_flex_align(foot, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    note = ui_label(foot, F.txt_sm, C_DIM, "");
    lv_obj_set_flex_grow(note, 1);
    lv_obj_add_event_cb(ui_btn(foot, "ОТМЕНИ ПРОМЕНИТЕ", BTN_NORMAL, false), on_revert, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn(foot, "ЗАПИШИ", BTN_GO, false), on_save, LV_EVENT_CLICKED, NULL);
    set_show();
}

void set_show(void)
{
    lv_obj_set_hidden(login, M.acc_level > 0);
    lv_obj_set_hidden(main_grid, M.acc_level == 0);
    if (M.acc_level) dirty();
    else login_render();
}

void set_update(uint32_t now)
{
    if (M.acc_level == 0) { login_render(); return; }
    /* автоматичен изход след „acc.tmo“ минути без натискане */
    lv_indev_t *in = lv_indev_get_next(NULL);
    if (in && lv_indev_get_state(in) == LV_INDEV_STATE_PRESSED) last_act = now;
    if (!last_act) last_act = now;
    if (now - last_act > cfg_num("acc.tmo") * 60000) { logout(" (без действие)"); return; }
    if (rebuild && !dlg_open()) build();
    bool d = cfg_dirty();
    ui_set_text(dirty_l, d ? "незаписани промени" : "");
    ui_set_text(note, d ? "Промените важат след „Запиши“." : "");
    ui_set_text(who, LVN[M.acc_level]);
}

/* за снимките от симулатора: вход и раздел */
/* тест: „Запиши“ */
void set_test_save(void) { on_save(NULL); }

/* тест: отваря редакцията на параметър от текущия раздел, напр. „ip“ */
void set_test_edit(const char *k)
{
    int n;
    const pdef_t *P = sec_form(sec, &n);
    for (int i = 0; P && i < n; i++)
        if (P[i].k && !strcmp(P[i].k, k)) {
            char key[40];
            snprintf(key, sizeof key, "%s.%s", SEC_K[sec], k);
            edit(&P[i], key);
            return;
        }
    fprintf(stderr, "set_test_edit: няма параметър %s\n", k);
}

void set_test(int level, const char *s)
{
    M.acc_level = level;
    for (int i = 0; i < S_N; i++)
        if (!strcmp(s, SEC_K[i])) sec = i;
    set_show();
}
