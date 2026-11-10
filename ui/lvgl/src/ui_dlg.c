/* Диалозите от макета (.modal / .dlg): задаване на позиция, въвеждане на число, на текст и избор от списък.
 * Диалогът се създава при отваряне върху lv_layer_top() и се изтрива при затваряне. Отворен е най-много един. */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "ui_style.h"

typedef enum { D_NONE, D_SETPOS, D_NUM, D_TEXT, D_PICK } dkind_t;

static struct {
    dkind_t kind;
    lv_obj_t *modal, *title, *cur, *in, *unit, *note, *ax[AX_N], *tgt[2], *keys;
    char txt[128];
    int ax_i;
    bool mach;
    /* число */
    double curval, min, max;
    int dec;
    dlg_num_cb ncb;
    /* текст */
    dlg_text_cb tcb;
    dlg_line_cb lcb;
    bool cyr, shift, gc;   /* gc: ред от G-code (главни без изключване, скоби вместо _ /) */
    int maxlen;
    /* избор */
    dlg_pick_cb pcb;
    void *ud;
} D;

void dlg_close(void)
{
    if (D.modal) lv_obj_delete_async(D.modal);
    D.modal = NULL;
    D.kind = D_NONE;
}

bool dlg_open(void) { return D.kind != D_NONE; }

static void on_cancel(lv_event_t *e) { dlg_close(); }

/* фон и рамка на диалога */
static lv_obj_t *dlg_box(float w_em)
{
    dlg_close();
    lv_obj_t *m = D.modal = ui_box(lv_layer_top());
    lv_obj_set_size(m, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m, lv_color_hex(0x05080a), 0);
    lv_obj_set_style_bg_opa(m, 189, 0);
    lv_obj_set_clickable(m, true);   /* поглъща натисканията под диалога */
    ui_flex(m, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(m, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *d = ui_box(m);
    lv_obj_set_width(d, EM(w_em));
    lv_obj_set_style_max_width(d, LV_PCT(94), 0);
    lv_obj_set_style_max_height(d, LV_PCT(94), 0);
    lv_obj_set_style_bg_color(d, C_SURF, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(d, C_EDGE, 0);
    lv_obj_set_style_border_width(d, 1, 0);
    lv_obj_set_style_radius(d, EM(0.5f), 0);
    ui_glow(d, 2, 120);
    lv_obj_set_style_pad_all(d, EM(0.8f), 0);
    ui_flex(d, LV_FLEX_FLOW_COLUMN, 0.5f);
    lv_obj_set_flex_align(d, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *h = ui_box(d);
    lv_obj_set_width(h, LV_PCT(100));
    ui_flex(h, LV_FLEX_FLOW_ROW, 0.6f);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    D.title = ui_label(h, F.ui_md, C_FG, "");
    lv_obj_set_style_text_letter_space(D.title, EM(0.03f), 0);
    D.cur = ui_label(h, F.num_sm, C_DIM, "");
    return d;
}

static lv_obj_t *input(lv_obj_t *d)
{
    lv_obj_t *b = ui_box(d);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_LINE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_radius(b, EM(0.35f), 0);
    lv_obj_set_style_pad_hor(b, EM(0.6f), 0);
    lv_obj_set_style_pad_ver(b, EM(0.3f), 0);
    ui_flex(b, LV_FLEX_FLOW_ROW, 0.4f);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return b;
}

static lv_obj_t *caret(lv_obj_t *parent, float h_em)
{
    lv_obj_t *c = ui_box(parent);
    lv_obj_set_size(c, EM(0.5f), EM(h_em));
    lv_obj_set_style_bg_color(c, C_ACC, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    return c;
}

static lv_obj_t *note(lv_obj_t *d)
{
    lv_obj_t *n = ui_label(d, F.txt_sm, C_DIM, "");
    lv_obj_set_width(n, LV_PCT(100));
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_min_height(n, EM(2.4f), 0);
    return n;
}

/* ред с бутони Отказ / … / основен */
static lv_obj_t *foot(lv_obj_t *d, const char *mid, lv_event_cb_t mid_cb, const char *ok, lv_event_cb_t ok_cb)
{
    lv_obj_t *r = ui_box(d);
    lv_obj_set_width(r, LV_PCT(100));
    ui_flex(r, LV_FLEX_FLOW_ROW, 0.35f);
    lv_obj_t *c = ui_btn(r, "ОТКАЗ", BTN_NORMAL, false);
    lv_obj_set_flex_grow(c, 10);
    lv_obj_add_event_cb(c, on_cancel, LV_EVENT_CLICKED, NULL);
    if (mid) {
        lv_obj_t *m = ui_btn(r, mid, BTN_NORMAL, false);
        lv_obj_set_flex_grow(m, 10);
        lv_obj_add_event_cb(m, mid_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ok) {
        lv_obj_t *o = ui_btn(r, ok, BTN_GO, false);
        lv_obj_set_flex_grow(o, 16);
        lv_obj_add_event_cb(o, ok_cb, LV_EVENT_CLICKED, NULL);
    }
    return r;
}

/* ---------- числова клавиатура (.dk) ---------- */
static void render(void);

static void dk_press(const char *k);
static void on_key(lv_event_t *e) { dk_press(lv_event_get_user_data(e)); }

static void dk_press(const char *k)
{
    char *t = D.txt;
    size_t n = strlen(t);
    if (!strcmp(k, "bs")) { if (n) t[n - 1] = 0; }
    else if (!strcmp(k, "clr")) t[0] = 0;
    else if (!strcmp(k, "pm")) {
        if (D.kind == D_NUM && D.min >= 0) return;
        if (t[0] == '-') memmove(t, t + 1, n);
        else if (n + 1 < sizeof D.txt) { memmove(t + 1, t, n + 1); t[0] = '-'; }
    } else if (!strcmp(k, "cur")) {
        double v = D.kind == D_SETPOS ? (D.mach ? M.mach[D.ax_i] : machine_work(D.ax_i)) : D.curval;
        snprintf(t, sizeof D.txt, "%.*f", D.kind == D_SETPOS ? 3 : D.dec, v);
    } else if (!strcmp(k, ".")) {
        if ((D.kind == D_NUM && D.dec == 0) || strchr(t, '.')) return;
        strcat(t, n == 0 || (n == 1 && t[0] == '-') ? "0." : ".");
    } else {
        int digits = 0;
        for (const char *p = t; *p; p++) digits += isdigit((unsigned char)*p) != 0;
        if (digits < 8) strcat(t, k);
    }
    render();
}

static void keypad(lv_obj_t *d)
{
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[5];
    rows[0] = rows[1] = rows[2] = rows[3] = EM(2.8f);
    rows[4] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(d);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, cols, rows, 0.3f);
    static const char *const K[] = {"7", "8", "9", "bs", "4", "5", "6", "pm", "1", "2", "3", ".", "0", "cur", "clr"};
    static const char *const L[] = {"7", "8", "9", "DEL", "4", "5", "6", "±", "1", "2", "3", ".", "0", "ТЕКУЩАТА", "C"};
    for (int i = 0; i < 15; i++) {
        lv_obj_t *b = ui_btn(g, L[i], BTN_NORMAL, false);
        lv_obj_set_style_pad_hor(b, 0, 0);
        lv_obj_set_style_text_font(b, i == 13 || i == 3 ? F.ui_sm : F.num_pg, 0);
        lv_obj_add_event_cb(b, on_key, LV_EVENT_CLICKED, (void *)K[i]);
        int c = i < 12 ? i % 4 : i == 12 ? 0 : i == 13 ? 1 : 3;
        ui_cell(b, c, i == 13 ? 2 : 1, i < 12 ? i / 4 : 3, 1);
    }
}

/* ---------- задаване на позиция ---------- */
static void render(void)
{
    char buf[400], v[24];
    if (D.kind == D_SETPOS) {
        int a = D.ax_i;
        for (int i = 0; i < AX_N; i++) {
            ui_btn_set_kind(D.ax[i], i == a ? BTN_ON : BTN_NORMAL);
            lv_obj_set_hidden(D.ax[i], !M.active[i]);
        }
        ui_seg_set(D.tgt, 2, D.mach);
        if (D.mach) snprintf(buf, sizeof buf, "Референтна точка · %s", AX_NAME[a]);
        else snprintf(buf, sizeof buf, "Работна нула G%d · %s", 53 + M.wcs, AX_NAME[a]);
        ui_set_text(D.title, buf);
        snprintf(buf, sizeof buf, "раб. %.3f  маш. %.3f", machine_work(a), M.mach[a]);
        ui_set_text(D.cur, buf);
        snprintf(buf, sizeof buf, "РАБОТНА · G%d", 53 + M.wcs);
        ui_set_text(ui_btn_label(D.tgt[0]), buf);
        snprintf(v, sizeof v, "%.3f", D.mach ? M.mach[a] : machine_work(a));
        ui_set_text(D.unit, M.rotary[a] ? "°" : "мм");
        if (D.mach) {
            snprintf(buf, sizeof buf, "Текущото положение на %s получава въведената машинна координата и оста се маркира като ръчно реферирана. "
                     "G%d остава спрямо машинната нула, така че работната позиция се измества със същата разлика.", AX_NAME[a], 53 + M.wcs);
            lv_obj_set_style_text_color(D.note, C_WARN, 0);
        } else {
            snprintf(buf, sizeof buf, "Текущото положение на %s получава въведената работна координата в G%d. "
                     "Машинната координата и референтната точка не се променят.", AX_NAME[a], 53 + M.wcs);
            lv_obj_set_style_text_color(D.note, C_DIM, 0);
        }
        ui_set_text(D.note, buf);
    } else {
        snprintf(v, sizeof v, "%.*f", D.dec, D.curval);
    }
    if (D.kind == D_SETPOS || D.kind == D_NUM) {
        ui_set_text(D.in, D.txt[0] ? D.txt : v);
        lv_obj_set_style_text_color(D.in, D.txt[0] ? C_FG : C_FAINT, 0);
    }
}

static void on_ax(lv_event_t *e) { D.ax_i = (int)(intptr_t)lv_event_get_user_data(e); D.txt[0] = 0; render(); }
static void on_tgt(lv_event_t *e) { D.mach = (bool)(intptr_t)lv_event_get_user_data(e); render(); }

static void setpos_apply(void)
{
    char *end;
    double v = strtod(D.txt, &end);
    if (!D.txt[0] || *end || !strcmp(D.txt, "-")) { machine_toast("Въведете стойност или натиснете „Задай 0“", lv_tick_get(), 3000); return; }
    if (machine_set_pos(D.ax_i, D.mach, v, lv_tick_get()) == 0) dlg_close();
}

static void on_sp_zero(lv_event_t *e) { snprintf(D.txt, sizeof D.txt, "0"); setpos_apply(); }
static void on_sp_ok(lv_event_t *e) { setpos_apply(); }

void dlg_setpos(int ax)
{
    lv_obj_t *d = dlg_box(31);
    D.kind = D_SETPOS;
    D.ax_i = ax;
    D.mach = false;
    D.txt[0] = 0;
    lv_obj_t *s = ui_box(d);
    lv_obj_set_width(s, LV_PCT(100));
    ui_flex(s, LV_FLEX_FLOW_ROW, 0.25f);
    for (int i = 0; i < AX_N; i++) {
        D.ax[i] = ui_btn(s, AX_NAME[i], BTN_NORMAL, true);
        lv_obj_set_flex_grow(D.ax[i], 1);
        lv_obj_add_event_cb(D.ax[i], on_ax, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    s = ui_box(d);
    lv_obj_set_width(s, LV_PCT(100));
    ui_flex(s, LV_FLEX_FLOW_ROW, 0.25f);
    D.tgt[0] = ui_btn(s, "РАБОТНА", BTN_ON, true);
    D.tgt[1] = ui_btn(s, "МАШИННА · РЕФ. ТОЧКА", BTN_NORMAL, true);
    for (int i = 0; i < 2; i++) {
        lv_obj_set_flex_grow(D.tgt[i], 1);
        lv_obj_add_event_cb(D.tgt[i], on_tgt, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_t *in = input(d);
    D.in = ui_label(in, F.num_ov, C_FG, "");
    D.unit = ui_label(in, F.ui_sm, C_DIM, "мм");
    caret(in, 1.4f);
    D.note = note(d);
    keypad(d);
    foot(d, "ЗАДАЙ 0", on_sp_zero, "ЗАДАЙ", on_sp_ok);
    render();
}

/* ---------- число ---------- */
static void on_num_ok(lv_event_t *e)
{
    if (!D.txt[0] || !strcmp(D.txt, "-")) { dlg_close(); return; }
    char *end;
    double v = strtod(D.txt, &end);
    if (*end || v < D.min || v > D.max) {
        char t[96];
        snprintf(t, sizeof t, "Стойността трябва да е между %g и %g", D.min, D.max);
        machine_toast(t, lv_tick_get(), 3500);
        return;
    }
    dlg_num_cb cb = D.ncb;
    void *ud = D.ud;
    dlg_close();
    if (cb) cb(v, ud);
}

void dlg_num(const char *title, const char *unit, const char *hint, double cur, double min, double max, int dec, dlg_num_cb cb, void *ud)
{
    lv_obj_t *d = dlg_box(28);
    D.kind = D_NUM;
    D.txt[0] = 0;
    D.curval = cur; D.min = min; D.max = max; D.dec = dec; D.ncb = cb; D.ud = ud;
    ui_set_text(D.title, title);
    char buf[64];
    snprintf(buf, sizeof buf, "%g … %g", min, max);
    ui_set_text(D.cur, buf);
    lv_obj_t *in = input(d);
    D.in = ui_label(in, F.num_ov, C_FG, "");
    D.unit = ui_label(in, F.ui_sm, C_DIM, unit ? unit : "");
    caret(in, 1.4f);
    D.note = note(d);
    ui_set_text(D.note, hint ? hint : "");
    keypad(d);
    foot(d, NULL, NULL, "ГОТОВО", on_num_ok);
    render();
}

/* ---------- текст (.tk): латиница и кирилица ---------- */
static const char *const TK_LAT[4] = {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm._/"};
static const char *const TK_CYR[4] = {"1234567890", "явертъуиоп", "асдфгхйкл-", "зьцжбнмчшщ"};
static const char *const TK_GC[4] = {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm.()"};

static void text_render(void);

static void on_tk(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target_obj(e);
    const char *k = lv_label_get_text(ui_btn_label(b));
    if (strlen(D.txt) + strlen(k) <= (size_t)D.maxlen) strcat(D.txt, k);   /* maxlen = позволени байтове */
    if (!D.gc) D.shift = false;
    text_render();
}

static void on_tk_fn(lv_event_t *e)
{
    int f = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(D.txt);
    if (f == 0 && n) {   /* изтриване на последния знак (UTF-8) */
        n--;
        while (n && ((unsigned char)D.txt[n] & 0xC0) == 0x80) n--;
        D.txt[n] = 0;
    } else if (f == 1) D.txt[0] = 0;
    else if (f == 2) D.shift = !D.shift;
    else if (f == 3) D.cyr = !D.cyr;
    else if (f == 4 && n + 1 <= (size_t)D.maxlen) strcat(D.txt, " ");
    text_render();
}

static void text_render(void)
{
    ui_set_text(D.in, D.txt);
    /* надписите на клавишите: малки/главни, латиница/кирилица */
    const char *const *rows = D.cyr ? TK_CYR : D.gc ? TK_GC : TK_LAT;
    int idx = 0;
    for (int r = 0; r < 4; r++) {
        const char *p = rows[r];
        while (*p) {
            char ch[8] = {0};
            int len = ((unsigned char)*p < 0x80) ? 1 : 2;
            memcpy(ch, p, len);
            p += len;
            if (D.shift) {
                if (len == 1) ch[0] = (char)toupper((unsigned char)ch[0]);
                else {   /* кирилица: малка → главна (U+0430–044F → U+0410–042F) */
                    unsigned cp = (((unsigned char)ch[0] & 0x1F) << 6) | ((unsigned char)ch[1] & 0x3F);
                    if (cp >= 0x430 && cp <= 0x44F) cp -= 0x20;
                    ch[0] = (char)(0xC0 | (cp >> 6));
                    ch[1] = (char)(0x80 | (cp & 0x3F));
                }
            }
            lv_obj_t *b = lv_obj_get_child(D.keys, idx++);
            ui_set_text(ui_btn_label(b), ch);
        }
    }
}

static void on_text_ok(lv_event_t *e)
{
    dlg_text_cb cb = D.tcb;
    void *ud = D.ud;
    char t[sizeof D.txt];
    snprintf(t, sizeof t, "%s", D.txt);
    lv_obj_t *m = D.modal;
    if (cb && cb(t, ud) != 0) return;   /* проверката не е минала: диалогът остава */
    if (D.modal == m) dlg_close();      /* освен ако обратното извикване е отворило нов */
}

static void on_line_fn(lv_event_t *e)
{
    dlg_line_cb cb = D.lcb;
    void *ud = D.ud;
    char t[sizeof D.txt];
    snprintf(t, sizeof t, "%s", D.txt);
    lv_obj_t *m = D.modal;
    if (cb) cb((int)(intptr_t)lv_event_get_user_data(e), t, ud);
    if (D.modal == m) dlg_close();
}

static void text_build(const char *title, const char *cur, int maxlen, dlg_text_cb cb, dlg_line_cb lcb, void *ud, const char *hint);

void dlg_text(const char *title, const char *cur, int maxlen, dlg_text_cb cb, void *ud)
{
    D.gc = false;
    text_build(title, cur, maxlen, cb, NULL, ud, NULL);
}

void dlg_text_line(const char *title, const char *cur, const char *hint, dlg_text_cb cb, dlg_line_cb lcb, void *ud)
{
    D.gc = true;
    text_build(title, cur, (int)sizeof D.txt - 1, cb, lcb, ud, hint);
}

static void text_build(const char *title, const char *cur, int maxlen, dlg_text_cb cb, dlg_line_cb lcb, void *ud, const char *hint)
{
    bool gc = D.gc;
    lv_obj_t *d = dlg_box(40);
    D.kind = D_TEXT;
    D.gc = gc;
    D.tcb = cb; D.lcb = lcb; D.ud = ud; D.shift = gc; D.cyr = false;
    D.maxlen = maxlen > 0 && maxlen < (int)sizeof D.txt ? maxlen : (int)sizeof D.txt - 1;
    snprintf(D.txt, sizeof D.txt, "%s", cur ? cur : "");
    ui_set_text(D.title, title);
    lv_obj_t *in = input(d);
    /* дългият ред на G-code се вижда откъм края (където е курсорът) */
    lv_obj_set_flex_align(in, gc ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    D.in = ui_label(in, F.num_pg, C_FG, "");
    caret(in, 1.1f);
    if (hint) { D.note = note(d); ui_set_text(D.note, hint); }
    static int32_t cols[11], rows[6];
    for (int i = 0; i < 10; i++) cols[i] = LV_GRID_FR(1);
    cols[10] = LV_GRID_TEMPLATE_LAST;
    for (int i = 0; i < 5; i++) rows[i] = EM(2.6f);
    rows[5] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = D.keys = ui_box(d);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, cols, rows, 0.25f);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 10; c++) {
            lv_obj_t *b = ui_btn(g, "", BTN_NORMAL, false);
            lv_obj_set_style_pad_hor(b, 0, 0);
            lv_obj_set_style_text_font(b, F.num_pg, 0);
            lv_obj_add_event_cb(b, on_tk, LV_EVENT_CLICKED, NULL);
            ui_cell(b, c, 1, r, 1);
        }
    static const char *const FN[] = {"DEL", "C", "ГЛАВНИ", "БГ / EN", "ИНТЕРВАЛ"};
    static const int FC[][2] = {{8, 2}, {0, 1}, {1, 2}, {3, 2}, {5, 3}};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = ui_btn(g, FN[i], BTN_NORMAL, false);
        lv_obj_set_style_pad_hor(b, 0, 0);
        lv_obj_add_event_cb(b, on_tk_fn, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ui_cell(b, FC[i][0], FC[i][1], 4, 1);
    }
    if (lcb) {
        lv_obj_t *r = ui_box(d);
        lv_obj_set_width(r, LV_PCT(100));
        ui_flex(r, LV_FLEX_FLOW_ROW, 0.35f);
        static const char *const LN[2] = {"ИЗТРИЙ РЕДА", "НОВ РЕД СЛЕД"};
        for (int i = 0; i < 2; i++) {
            lv_obj_t *b = ui_btn(r, LN[i], BTN_NORMAL, false);
            lv_obj_set_flex_grow(b, 1);
            lv_obj_add_event_cb(b, on_line_fn, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
    }
    foot(d, NULL, NULL, "ГОТОВО", on_text_ok);
    text_render();
}

/* ---------- избор от списък (.pick) ---------- */
static void on_pick(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    dlg_pick_cb cb = D.pcb;
    void *ud = D.ud;
    dlg_close();
    if (cb) cb(i, ud);
}

void dlg_pick(const char *title, const char *const *opts, int n, int cur, dlg_pick_cb cb, void *ud)
{
    lv_obj_t *d = dlg_box(28);
    D.kind = D_PICK;
    D.pcb = cb; D.ud = ud;
    ui_set_text(D.title, title);
    lv_obj_t *l = ui_box(d);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_max_height(l, EM(30), 0);
    ui_scroll_v(l);
    ui_flex(l, LV_FLEX_FLOW_COLUMN, 0.3f);
    for (int i = 0; i < n; i++) {
        lv_obj_t *b = ui_btn(l, opts[i], i == cur ? BTN_ON : BTN_NORMAL, false);
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_text_font(b, F.txt, 0);
        lv_obj_set_style_text_letter_space(b, 0, 0);
        lv_obj_add_event_cb(b, on_pick, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_t *c = ui_btn(d, "ОТКАЗ", BTN_NORMAL, false);
    lv_obj_set_width(c, LV_PCT(100));
    lv_obj_add_event_cb(c, on_cancel, LV_EVENT_CLICKED, NULL);
}

/* ---------- потвърждение ---------- */
static dlg_pick_cb confirm_cb;
static void on_conf_ok(lv_event_t *e)
{
    dlg_pick_cb cb = confirm_cb;
    void *ud = D.ud;
    dlg_close();
    if (cb) cb(1, ud);
}

void dlg_confirm(const char *title, const char *text, const char *ok, dlg_pick_cb cb, void *ud)
{
    lv_obj_t *d = dlg_box(30);
    D.kind = D_PICK;
    D.ud = ud;
    confirm_cb = cb;
    ui_set_text(D.title, title);
    D.note = note(d);
    ui_set_text(D.note, text);
    foot(d, NULL, NULL, ok, on_conf_ok);
}

/* ---------- физическа клавиатура ---------- */
bool dlg_key(uint32_t key)
{
    if (D.kind == D_NONE) return false;
    if (key == LV_KEY_ESC) { dlg_close(); return true; }
    if (D.kind == D_SETPOS || D.kind == D_NUM) {
        char k[2] = {(char)key, 0};
        if (key == LV_KEY_ENTER) { if (D.kind == D_NUM) on_num_ok(NULL); else setpos_apply(); }
        else if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) dk_press("bs");
        else if ((key >= '0' && key <= '9') || key == '.') dk_press(k);
        else if (key == ',') dk_press(".");
        else if (key == '-') dk_press("pm");
        return true;
    }
    if (D.kind == D_TEXT) {
        size_t n = strlen(D.txt);
        if (key == LV_KEY_ENTER) on_text_ok(NULL);
        else if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            if (n) { n--; while (n && ((unsigned char)D.txt[n] & 0xC0) == 0x80) n--; D.txt[n] = 0; }
            text_render();
        } else if (key >= 0x20 && key < 0x800 && n + (key < 0x80 ? 1 : 2) <= (size_t)D.maxlen) {   /* латиница и кирилица (UTF-8) */
            if (D.gc && key >= 'a' && key <= 'z') key -= 32;
            if (key < 0x80) D.txt[n++] = (char)key;
            else { D.txt[n++] = (char)(0xC0 | (key >> 6)); D.txt[n++] = (char)(0x80 | (key & 0x3F)); }
            D.txt[n] = 0;
            text_render();
        }
        return true;
    }
    if (D.kind == D_PICK && key == LV_KEY_ENTER && confirm_cb) { on_conf_ok(NULL); return true; }
    return true;   /* докато има диалог, клавишите не стигат до екрана */
}
