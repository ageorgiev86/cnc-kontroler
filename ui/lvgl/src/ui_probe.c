/* Екранът „Офсети“ (.probe в макета): входовете за измерване, циклите с 3D пробника,
 * нулевите точки G54–G59 с детайлите и резултатът от последното измерване. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "program.h"
#include "ui.h"
#include "ui_style.h"

static lv_obj_t *led_prb, *led_ts, *mt_prb, *mt_ts, *ev_txt, *ts_pos, *prb_hdr, *crn[4], *cyc_sub[9], *cyc_btn[9];
static lv_obj_t *off_info, *off_chk[7], *off_name[7], *off_val[7][3], *off_row[7], *off_note, *skip_btn;
static lv_obj_t *res_box, *res_title, *res_rows, *res_note, *warn_box, *warn_txt;
static int corner = 0;   /* fl */
static int res_seq = -1, prb_cnt_was = -1, ts_cnt_was = -1;
static uint32_t test_prb, test_ts;
static char off_key[64];

static const char *const CYC_K[9] = {"z", "hole", "boss", "rot", "corner", "icorner", "width", "widthy", "hmap"};
static const char *const CYC_N[9] = {"Повърхност Z", "Център на отвор", "Център на бос", "Ротация на детайла", "Външен ъгъл",
                                     "Вътрешен ъгъл", "Ширина на жлеб X", "Ширина на жлеб Y", "Карта на височините"};

/* ---------- иконите на циклите (SVG от макета, 68×52) ---------- */
typedef struct { lv_layer_t *l; float x0, y0, k; int fx, fy; } ic_t;

static lv_point_precise_t IP(const ic_t *c, float x, float y)
{
    if (c->fx) x = 68 - x;
    if (c->fy) y = 52 - y;
    lv_point_precise_t p = {(lv_value_precise_t)lroundf(c->x0 + x * c->k), (lv_value_precise_t)lroundf(c->y0 + y * c->k)};
    return p;
}

static void iline(const ic_t *c, float x1, float y1, float x2, float y2, lv_color_t col, bool dash)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = IP(c, x1, y1); d.p2 = IP(c, x2, y2); d.color = col;
    d.width = (int32_t)fmaxf(1, 2 * c->k);
    if (dash) { d.dash_width = (int32_t)fmaxf(1, 2 * c->k); d.dash_gap = d.dash_width; d.width = (int32_t)fmaxf(1, 1.5f * c->k); }
    lv_draw_line(c->l, &d);
}

static void icirc(const ic_t *c, float x, float y, float r, lv_color_t col, bool fill)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    lv_point_precise_t p = IP(c, x, y);
    int32_t rr = (int32_t)lroundf(r * c->k);
    lv_area_t a = {(int32_t)p.x - rr, (int32_t)p.y - rr, (int32_t)p.x + rr, (int32_t)p.y + rr};
    if (fill) { d.bg_color = col; d.bg_opa = LV_OPA_COVER; }
    else { d.bg_opa = LV_OPA_TRANSP; d.border_color = col; d.border_width = (int32_t)fmaxf(1, 2 * c->k); }
    lv_draw_rect(c->l, &d, &a);
}

static void draw_icon(lv_layer_t *l, const lv_area_t *a, int k, int fx, int fy)
{
    ic_t c = {l, 0, 0, 0, fx, fy};
    float w = (float)lv_area_get_width(a), h = (float)lv_area_get_height(a);
    c.k = fminf(w / 68, h / 52);
    c.x0 = a->x1 + (w - 68 * c.k) / 2;
    c.y0 = a->y1 + (h - 52 * c.k) / 2;
    lv_color_t fg = C_FG, ac = C_ACC;
    switch (k) {
    case 0:   /* z */
        iline(&c, 6, 36, 62, 36, fg, 0); iline(&c, 62, 36, 62, 48, fg, 0); iline(&c, 62, 48, 6, 48, fg, 0); iline(&c, 6, 48, 6, 36, fg, 0);
        iline(&c, 34, 4, 34, 26, fg, 0); icirc(&c, 34, 30, 4, ac, 1);
        iline(&c, 28, 18, 34, 26, ac, 0); iline(&c, 34, 26, 40, 18, ac, 0);
        break;
    case 1:   /* hole */
        icirc(&c, 34, 26, 20, fg, 0); icirc(&c, 34, 26, 3.5f, ac, 1);
        iline(&c, 38, 26, 50, 26, ac, 0); iline(&c, 30, 26, 18, 26, ac, 0); iline(&c, 34, 22, 34, 10, ac, 0); iline(&c, 34, 30, 34, 42, ac, 0);
        break;
    case 2:   /* boss */
        icirc(&c, 34, 26, 13, fg, 0); icirc(&c, 34, 26, 2, fg, 1);
        iline(&c, 4, 26, 17, 26, ac, 0); iline(&c, 64, 26, 51, 26, ac, 0); iline(&c, 34, 0, 34, 9, ac, 0); iline(&c, 34, 52, 34, 43, ac, 0);
        break;
    case 3:   /* rot */
        iline(&c, 8, 40, 58, 28, fg, 0); icirc(&c, 20, 22, 3.5f, ac, 1); icirc(&c, 48, 15, 3.5f, ac, 1);
        iline(&c, 20, 26, 20, 35, ac, 0); iline(&c, 48, 19, 48, 28, ac, 0);
        break;
    case 4:   /* corner */
        iline(&c, 24, 48, 24, 20, fg, 0); iline(&c, 24, 20, 62, 20, fg, 0); icirc(&c, 12, 10, 3.5f, ac, 1);
        iline(&c, 12, 14, 12, 30, ac, 0); iline(&c, 16, 10, 40, 10, ac, 0);
        break;
    case 5:   /* icorner */
        iline(&c, 10, 4, 10, 46, fg, 0); iline(&c, 10, 46, 64, 46, fg, 0); icirc(&c, 30, 28, 3.5f, ac, 1);
        iline(&c, 26, 28, 14, 28, ac, 0); iline(&c, 30, 32, 30, 42, ac, 0);
        break;
    case 6:   /* width */
        iline(&c, 10, 6, 10, 46, fg, 0); iline(&c, 58, 6, 58, 46, fg, 0); icirc(&c, 34, 26, 3.5f, ac, 1);
        iline(&c, 30, 26, 14, 26, ac, 0); iline(&c, 38, 26, 54, 26, ac, 0);
        break;
    case 7:   /* widthy */
        iline(&c, 6, 6, 62, 6, fg, 0); iline(&c, 6, 46, 62, 46, fg, 0); icirc(&c, 34, 26, 3.5f, ac, 1);
        iline(&c, 34, 22, 34, 10, ac, 0); iline(&c, 34, 30, 34, 42, ac, 0);
        break;
    case 8: { /* hmap: вълна и четири точки */
        float px = 4, py = 38;
        for (int i = 1; i <= 15; i++) {
            float x = 4 + i * 4, y = 36 + 3.5f * sinf(x / 9.0f) - (x - 4) * 0.04f;
            iline(&c, px, py, x, y, fg, 0);
            px = x; py = y;
        }
        static const float X4[4] = {10, 26, 42, 58}, Y4[4] = {33, 36, 35, 31};
        for (int i = 0; i < 4; i++) { icirc(&c, X4[i], 14, 2.5f, ac, 1); iline(&c, X4[i], 18, X4[i], Y4[i], ac, 1); }
        break;
    }
    }
}

static void on_icon_draw(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target_obj(e);
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    bool cn = k == 4 || k == 5;
    draw_icon(lv_event_get_layer(e), &a, k, cn && (corner == 1 || corner == 3), cn && corner >= 2);
}

/* ---------- действия ---------- */
static void on_test(lv_event_t *e)
{
    int ts = (int)(intptr_t)lv_event_get_user_data(e);
    uint32_t now = lv_tick_get();
    if (ts) { M.ts_cnt++; M.ts_flash = now; test_ts = now; }
    else { M.prb_cnt++; M.prb_flash = now; test_prb = now; }
}

static void on_teach_ts(lv_event_t *e)
{
    uint32_t now = lv_tick_get();
    if (!M.homed[AX_X] || !M.homed[AX_Y]) { machine_toast("Осите X, Y не са реферирани. Позицията няма да е вярна. Натиснете HOME.", now, 4000); return; }
    drf_set_num("ts.tsx", M.mach[AX_X], 3); cfg_commit_key("ts.tsx");
    drf_set_num("ts.tsy", M.mach[AX_Y], 3); cfg_commit_key("ts.tsy");
    char t[96];
    snprintf(t, sizeof t, "Toolsetter: X %.3f Y %.3f (записано)", M.mach[AX_X], M.mach[AX_Y]);
    machine_toast(t, now, 3500);
}

static void on_cal(lv_event_t *e) { machine_probe_cycle("cal", 0, lv_tick_get()); }
static void on_meas(lv_event_t *e) { machine_measure_tool(lv_tick_get()); }
static void on_corner(lv_event_t *e) { corner = (int)(intptr_t)lv_event_get_user_data(e); ui_seg_set(crn, 4, corner); for (int i = 4; i <= 5; i++) lv_obj_invalidate(cyc_btn[i]); }
static void on_cyc(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k == 8) { hmap_open(); return; }
    machine_probe_cycle(CYC_K[k], corner, lv_tick_get());
}
static void on_load_t10(lv_event_t *e) { prb_warn = false; machine_tool_change(10, lv_tick_get()); }
static void on_wcs_chk(lv_event_t *e) { machine_wcs_toggle((int)(intptr_t)lv_event_get_user_data(e), lv_tick_get()); }
static void on_wcs_act(lv_event_t *e) { machine_wcs_select((int)(intptr_t)lv_event_get_user_data(e), lv_tick_get()); }
static void on_wcs_set(double v, void *ud)
{
    int k = (int)(intptr_t)ud, n = k / 8, a = k % 8;
    M.wcs_off[n][a] = v;
}
static void on_wcs_val(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e), n = k / 8, a = k % 8;
    if (M.state != MS_IDLE) { machine_toast("Спрете машината", lv_tick_get(), 3000); return; }
    char t[48];
    snprintf(t, sizeof t, "G%d · отместване %s", 53 + n, AX_NAME[a]);
    dlg_num(t, "маш.", "Машинната координата на нулата по тази ос. Най-лесно се задава с „X = 0“ или „Задай“ в „Ръчно“ или с цикъл на пробника, при избрана активна нула.",
            M.wcs_off[n][a], -99999, 99999, 3, on_wcs_set, (void *)(intptr_t)k);
}
static void on_skip(lv_event_t *e) { machine_skip_part(lv_tick_get()); }

/* отметка ✓ (шрифтовете нямат този знак): две линии */
static void on_chk_draw(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target_obj(e);
    if (!lv_obj_get_user_data(o)) return;
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    int32_t cx = (a.x1 + a.x2) / 2, cy = (a.y1 + a.y2) / 2, s = EM(0.4f);
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = C_FG; d.width = EM(0.14f); d.round_start = d.round_end = 1;
    d.p1.x = cx - s; d.p1.y = cy; d.p2.x = cx - s / 3; d.p2.y = cy + s * 2 / 3;
    lv_draw_line(lv_event_get_layer(e), &d);
    d.p1 = d.p2; d.p2.x = cx + s; d.p2.y = cy - s * 2 / 3;
    lv_draw_line(lv_event_get_layer(e), &d);
}

/* ---------- изграждане ---------- */
static lv_obj_t *inp_row(lv_obj_t *card, const char *name, lv_obj_t **led, lv_obj_t **meta, int ts)
{
    lv_obj_t *r = ui_box(card);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_style_pad_all(r, EM(0.6f), 0);
    lv_obj_set_style_pad_hor(r, EM(0.7f), 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r, C_LINE, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    ui_flex(r, LV_FLEX_FLOW_ROW, 0.7f);
    *led = ui_led(r, 1.35f);
    lv_obj_t *c = ui_box(r);
    lv_obj_set_flex_grow(c, 1);
    ui_flex(c, LV_FLEX_FLOW_COLUMN, 0.15f);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *n = ui_label(c, F.ui_md, C_FG, name);
    lv_obj_set_style_text_letter_space(n, EM(0.03f), 0);
    *meta = ui_label(c, F.num_sm, C_DIM, "");
    lv_obj_add_event_cb(ui_btn(r, "ТЕСТ", BTN_NORMAL, true), on_test, LV_EVENT_CLICKED, (void *)(intptr_t)ts);
    return r;
}

static lv_obj_t *full_btn(lv_obj_t *p, const char *t, lv_event_cb_t cb)
{
    lv_obj_t *b = ui_btn(p, t, BTN_NORMAL, false);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

static lv_obj_t *inputs_card(lv_obj_t *scr)
{
    lv_obj_t *c = ui_card(scr, "ВХОДОВЕ ЗА ИЗМЕРВАНЕ", NULL);
    inp_row(c, "3D пробник", &led_prb, &mt_prb, 0);
    inp_row(c, "Toolsetter", &led_ts, &mt_ts, 1);
    lv_obj_t *b = ui_body(c, 0.5f);
    ui_lab(b, "ПОСЛЕДНО СЪБИТИЕ (LATCH ОТ ЕНКОДЕР)");
    ev_txt = ui_label(b, F.num_sm, C_DIM, "Няма събития");
    lv_obj_set_width(ev_txt, LV_PCT(100));
    lv_label_set_long_mode(ev_txt, LV_LABEL_LONG_MODE_WRAP);
    ui_lab(b, "TOOLSETTER");
    ui_kv(b, "Позиция (маш.)", &ts_pos);
    full_btn(b, "ВЗЕМИ ТЕКУЩАТА X Y", on_teach_ts);
    ui_lab(b, "3D ПРОБНИК");
    full_btn(b, "КАЛИБРИРАНЕ В ПРЪСТЕН", on_cal);
    full_btn(b, "ИЗМЕРИ ТЕКУЩИЯ ИНСТРУМЕНТ", on_meas);
    if (g_portrait) { lv_obj_set_flex_grow(b, 0); lv_obj_set_height(b, LV_SIZE_CONTENT); }
    return c;
}

static lv_obj_t *cycles_card(lv_obj_t *scr)
{
    lv_obj_t *hr;
    lv_obj_t *c = ui_card(scr, "ЦИКЛИ С 3D ПРОБНИК", &hr);
    prb_hdr = ui_label(hr, F.num_sm, C_DIM, "");
    lv_obj_t *b = ui_body(c, 0.5f);
    lv_obj_t *cr = ui_box(b);
    lv_obj_set_width(cr, LV_PCT(100));
    ui_flex(cr, LV_FLEX_FLOW_ROW, 0.5f);
    ui_lab(cr, "ЪГЪЛ");
    static int32_t c2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, r2[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *g = ui_box(cr);
    lv_obj_set_flex_grow(g, 1);
    ui_grid(g, c2, r2, 0.25f);
    static const char *const CN[4] = {"ПРЕДЕН ЛЯВ", "ПРЕДЕН ДЕСЕН", "ЗАДЕН ЛЯВ", "ЗАДЕН ДЕСЕН"};
    static const int CP[4][2] = {{0, 1}, {1, 1}, {0, 0}, {1, 0}};
    for (int i = 0; i < 4; i++) {
        crn[i] = ui_btn(g, CN[i], i == corner ? BTN_ON : BTN_NORMAL, true);
        lv_obj_add_event_cb(crn[i], on_corner, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ui_cell(crn[i], CP[i][0], 1, CP[i][1], 1);
    }
    static int32_t c3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t r3[4];
    for (int i = 0; i < 3; i++) r3[i] = g_portrait ? EM(7.5f) : LV_GRID_FR(1);
    r3[3] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *cy = ui_box(b);
    lv_obj_set_width(cy, LV_PCT(100));
    if (g_portrait) { lv_obj_set_flex_grow(b, 0); lv_obj_set_height(b, LV_SIZE_CONTENT); }
    else lv_obj_set_flex_grow(cy, 1);
    ui_grid(cy, c3, r3, 0.45f);
    for (int i = 0; i < 9; i++) {
        lv_obj_t *bt = cyc_btn[i] = ui_btn(cy, "", BTN_NORMAL, false);
        lv_obj_delete(ui_btn_label(bt));
        ui_flex(bt, LV_FLEX_FLOW_COLUMN, 0.25f);
        lv_obj_set_flex_align(bt, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(bt, EM(0.2f), 0);
        lv_obj_t *ic = ui_box(bt);
        lv_obj_set_size(ic, EM(3.4f), EM(2.6f));
        lv_obj_add_event_cb(ic, on_icon_draw, LV_EVENT_DRAW_MAIN, (void *)(intptr_t)i);
        lv_obj_t *n = ui_label(bt, F.ui_sm, C_FG, CYC_N[i]);
        lv_obj_set_style_text_letter_space(n, EM(0.02f), 0);
        lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
        cyc_sub[i] = ui_label(bt, F.num_sm, C_DIM, "");
        lv_obj_add_event_cb(bt, on_cyc, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ui_cell(bt, i % 3, 1, i / 3, 1);
    }
    return c;
}

static lv_obj_t *offsets_card(lv_obj_t *col)
{
    lv_obj_t *hr;
    lv_obj_t *c = ui_card(col, "ОФСЕТИ", &hr);
    lv_obj_set_width(c, LV_PCT(100));
    off_info = ui_label(hr, F.num_sm, C_DIM, "");
    lv_obj_t *b = ui_body(c, 0.4f);
    lv_obj_set_flex_grow(b, 0);
    lv_obj_set_height(b, LV_SIZE_CONTENT);
    static int32_t cols[6], rows[8];
    cols[0] = EM(3.6f); cols[1] = EM(3.6f); cols[2] = cols[3] = cols[4] = LV_GRID_FR(1); cols[5] = LV_GRID_TEMPLATE_LAST;
    for (int i = 0; i < 7; i++) rows[i] = LV_GRID_CONTENT;
    rows[7] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(b);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, cols, rows, 0.25f);
    static const char *const H[5] = {"ДЕТАЙЛ", "НУЛА", "X", "Y", "Z"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *l = ui_label(g, F.ui_xs, C_DIM, H[i]);
        lv_obj_set_grid_cell(l, i >= 2 ? LV_GRID_ALIGN_END : LV_GRID_ALIGN_START, i, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
    for (int n = 1; n <= 6; n++) {
        lv_obj_t *bg = off_row[n] = ui_box(g);   /* фон на реда (.sel) */
        lv_obj_set_style_bg_color(bg, C_ACC, 0);
        lv_obj_set_style_radius(bg, EM(0.2f), 0);
        ui_cell(bg, 0, 5, n, 1);
        off_chk[n] = ui_btn(g, "", BTN_NORMAL, true);
        lv_obj_add_event_cb(off_chk[n], on_chk_draw, LV_EVENT_DRAW_MAIN_END, NULL);
        lv_obj_set_style_pad_hor(off_chk[n], 0, 0);
        lv_obj_set_grid_cell(off_chk[n], LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, n, 1);
        lv_obj_set_width(off_chk[n], EM(2));
        if (n > 1) lv_obj_add_event_cb(off_chk[n], on_wcs_chk, LV_EVENT_CLICKED, (void *)(intptr_t)n);
        char t[8];
        snprintf(t, sizeof t, "G%d", 53 + n);
        off_name[n] = ui_btn(g, t, BTN_NORMAL, true);
        lv_obj_set_style_text_font(off_name[n], F.ui_md, 0);
        lv_obj_set_style_pad_hor(off_name[n], EM(0.3f), 0);
        lv_obj_set_grid_cell(off_name[n], LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, n, 1);
        lv_obj_add_event_cb(off_name[n], on_wcs_act, LV_EVENT_CLICKED, (void *)(intptr_t)n);
        for (int a = 0; a < 3; a++) {
            lv_obj_t *v = off_val[n][a] = ui_btn(g, "", BTN_NORMAL, true);
            lv_obj_set_style_text_font(v, F.num_sm, 0);
            lv_obj_set_style_text_letter_space(v, 0, 0);
            lv_obj_set_style_pad_hor(v, EM(0.3f), 0);
            lv_obj_set_flex_align(v, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_grid_cell(v, LV_GRID_ALIGN_STRETCH, 2 + a, 1, LV_GRID_ALIGN_CENTER, n, 1);
            lv_obj_add_event_cb(v, on_wcs_val, LV_EVENT_CLICKED, (void *)(intptr_t)(n * 8 + a));
        }
    }
    off_note = ui_label(b, F.txt_sm, C_DIM, "");
    lv_obj_set_width(off_note, LV_PCT(100));
    lv_label_set_long_mode(off_note, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_recolor(off_note, true);
    skip_btn = full_btn(b, "ПРОПУСНИ ТЕКУЩИЯ ДЕТАЙЛ", on_skip);
    return c;
}

static lv_obj_t *result_card(lv_obj_t *col)
{
    lv_obj_t *c = ui_card(col, "РЕЗУЛТАТ", NULL);
    lv_obj_set_width(c, LV_PCT(100));
    if (!g_portrait) lv_obj_set_flex_grow(c, 1);
    lv_obj_t *b = res_box = ui_body(c, 0.4f);
    if (g_portrait) { lv_obj_set_flex_grow(b, 0); lv_obj_set_height(b, LV_SIZE_CONTENT); }
    warn_box = ui_box(b);
    lv_obj_set_width(warn_box, LV_PCT(100));
    ui_flex(warn_box, LV_FLEX_FLOW_COLUMN, 0.5f);
    lv_obj_set_flex_align(warn_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    warn_txt = ui_label(warn_box, F.txt, C_WARN, "");
    lv_obj_set_width(warn_txt, LV_PCT(100));
    lv_label_set_long_mode(warn_txt, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_add_event_cb(ui_btn(warn_box, "ЗАРЕДИ T10", BTN_ON, false), on_load_t10, LV_EVENT_CLICKED, NULL);
    res_title = ui_lab(b, "");
    res_rows = ui_box(b);
    lv_obj_set_width(res_rows, LV_PCT(100));
    ui_flex(res_rows, LV_FLEX_FLOW_COLUMN, 0.3f);
    res_note = ui_label(b, F.txt, C_DIM, "");
    lv_obj_set_width(res_note, LV_PCT(100));
    lv_label_set_long_mode(res_note, LV_LABEL_LONG_MODE_WRAP);
    return c;
}

void probe_create(lv_obj_t *scr)
{
    static int32_t cols[4], rows[3];
    if (g_portrait) {
        cols[0] = LV_GRID_FR(1); cols[1] = LV_GRID_FR(1); cols[2] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_CONTENT; rows[1] = LV_GRID_FR(1); rows[2] = LV_GRID_TEMPLATE_LAST;
    } else {
        cols[0] = LV_GRID_FR(30); cols[1] = LV_GRID_FR(42); cols[2] = LV_GRID_FR(28); cols[3] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_FR(1); rows[1] = LV_GRID_TEMPLATE_LAST;
    }
    ui_grid(scr, cols, rows, 0.6f);
    lv_obj_t *in = inputs_card(scr);
    lv_obj_t *cy = cycles_card(scr);
    lv_obj_t *col = ui_box(scr);
    ui_flex(col, LV_FLEX_FLOW_COLUMN, 0.6f);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    offsets_card(col);
    result_card(col);
    if (g_portrait) {
        lv_obj_set_grid_cell(in, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
        lv_obj_set_grid_cell(col, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 0, 1);
        ui_cell(cy, 0, 2, 1, 1);
    } else {
        ui_cell(in, 0, 1, 0, 1);
        ui_cell(cy, 1, 1, 0, 1);
        ui_cell(col, 2, 1, 0, 1);
    }
}

static void off_update(void)
{
    char k[64];
    int parts[6], np = machine_parts(parts);
    bool run = M.run_kind == RK_PROG && (M.state == MS_RUN || M.state == MS_HOLD);
    unsigned h = 0;
    for (int n = 1; n <= 6; n++)
        for (int a = 0; a < 3; a++) h = h * 31u + (unsigned)lround(M.wcs_off[n][a] * 1000);
    snprintf(k, sizeof k, "%d|%d|%d|%d|%u|%d", M.wcs, M.parts, M.state, run, h, M.part_i);
    if (!strcmp(k, off_key)) return;
    snprintf(off_key, sizeof off_key, "%s", k);
    for (int n = 1; n <= 6; n++) {
        bool on = n == 1 || (M.parts & (1 << n)), act = n == M.wcs, set = machine_wcs_isset(n);
        lv_obj_set_style_bg_opa(off_row[n], act ? 77 : LV_OPA_TRANSP, 0);
        ui_btn_set_kind(off_chk[n], on ? BTN_ON : BTN_NORMAL);
        lv_obj_set_user_data(off_chk[n], (void *)(intptr_t)on);
        lv_obj_invalidate(off_chk[n]);
        if (n == 1) lv_obj_set_style_opa(off_chk[n], 160, 0);
        int pi = -1;
        for (int i = 0; i < np; i++) if (parts[i] == n) pi = i;
        char t[16];
        if (np > 1 && pi >= 0) snprintf(t, sizeof t, "G%d·%d", 53 + n, pi + 1); else snprintf(t, sizeof t, "G%d", 53 + n);
        ui_set_text(ui_btn_label(off_name[n]), t);
        ui_btn_set_kind(off_name[n], act ? BTN_ON : on ? BTN_NORMAL : BTN_GHOST);
        lv_obj_set_style_text_color(off_name[n], on ? C_HI : C_FAINT, 0);
        lv_obj_set_style_border_width(off_name[n], on ? (act ? 1 : 1) : 0, 0);
        for (int a = 0; a < 3; a++) {
            if (set) snprintf(t, sizeof t, "%.3f", M.wcs_off[n][a]); else snprintf(t, sizeof t, "—");
            ui_set_text(ui_btn_label(off_val[n][a]), t);
            ui_btn_set_kind(off_val[n][a], on ? BTN_NORMAL : BTN_GHOST);
            lv_obj_set_style_border_width(off_val[n][a], on ? 1 : 0, 0);
            lv_obj_set_style_text_color(off_val[n][a], on ? C_FG : C_FAINT, 0);
            lv_obj_set_clickable(off_val[n][a], on);
        }
    }
    if (np > 1) {
        float tt = P.t_total * np / 2.5f;
        snprintf(k, sizeof k, "%d детайла · ~%d:%02d:%02d", np, (int)tt / 3600, (int)tt % 3600 / 60, (int)tt % 60);
    } else snprintf(k, sizeof k, "G%d", 53 + M.wcs);
    ui_set_text(off_info, k);
    char note[600];
    int o = snprintf(note, sizeof note, "Активна: G%d (натиснете нулата). В нея записват циклите на пробника и „X = 0“ / „Задай“ в „Ръчно“. "
                     "Отметнатите G55–G59 се изработват с G54 с една програма, по инструмент.", 53 + M.wcs);
    for (int i = 0; i < np; i++)
        if (!machine_wcs_isset(parts[i]) && o < (int)sizeof note - 120)
            o += snprintf(note + o, sizeof note - o, "\n#ff9f43 G%d не е зададена: изберете я като активна и я засечете.#", 53 + parts[i]);
    if (run && o < (int)sizeof note - 100) snprintf(note + o, sizeof note - o, "\nМашината работи: офсетите се променят само при спряна машина.");
    ui_set_text(off_note, note);
    lv_obj_set_hidden(skip_btn, !(run && np > 1));
}

void probe_update(uint32_t now)
{
    char b[160];
    /* входовете: светват при сработване (тест или допир) */
    ui_led_set(led_prb, C_OK, now - M.prb_flash < 400 && M.prb_flash);
    ui_led_set(led_ts, C_OK, now - M.ts_flash < 400 && M.ts_flash);
    snprintf(b, sizeof b, "PRB1 · NO · 24 V · %d сраб.", M.prb_cnt);
    ui_set_text(mt_prb, b);
    snprintf(b, sizeof b, "PRB2 · NC · 24 V · %d сраб.", M.ts_cnt);
    ui_set_text(mt_ts, b);
    if (M.prb_cnt != prb_cnt_was || M.ts_cnt != ts_cnt_was) {
        bool ts = M.ts_cnt != ts_cnt_was;
        if (prb_cnt_was >= 0) {
            bool test = ts ? test_ts == M.ts_flash : test_prb == M.prb_flash;
            snprintf(b, sizeof b, "%s%s\nX %.3f  Y %.3f  Z %.3f (маш.)", ts ? "PRB2 Toolsetter" : "PRB1 3D пробник", test ? " · тест" : "",
                     M.mach[AX_X], M.mach[AX_Y], M.mach[AX_Z]);
            ui_set_text(ev_txt, b);
            lv_obj_set_style_text_color(ev_txt, C_FG, 0);
        }
        prb_cnt_was = M.prb_cnt;
        ts_cnt_was = M.ts_cnt;
    }
    snprintf(b, sizeof b, "X %.3f  Y %.3f", cfg_num("ts.tsx"), cfg_num("ts.tsy"));
    ui_set_text(ts_pos, b);
    snprintf(b, sizeof b, "сфера Ø%.3f", cfg_num("prb.prDia"));
    ui_set_text(prb_hdr, b);

    static const char *const CX[4] = {"+", "−", "+", "−"}, *const CY[4] = {"+", "+", "−", "−"};
    static const char *const IX[4] = {"−", "+", "−", "+"}, *const IY[4] = {"−", "−", "+", "+"};
    static const char *const SUB[9] = {"G38.2 Z−", "4 точки", "4 точки", "2 точки по X", NULL, NULL, "2 точки", "2 точки", NULL};
    for (int i = 0; i < 9; i++) {
        if (SUB[i]) ui_set_text(cyc_sub[i], SUB[i]);
        else if (i == 4) { snprintf(b, sizeof b, "X%s / Y%s", CX[corner], CY[corner]); ui_set_text(cyc_sub[i], b); }
        else if (i == 5) { snprintf(b, sizeof b, "X%s / Y%s", IX[corner], IY[corner]); ui_set_text(cyc_sub[i], b); }
        else { hmap_label(b, sizeof b); ui_set_text(cyc_sub[i], b); }
    }
    off_update();
    hmap_tick();

    /* резултат */
    lv_obj_set_hidden(warn_box, !prb_warn);
    if (prb_warn) {
        snprintf(b, sizeof b, "В шпиндела е %s%d. Циклите с 3D пробник изискват T10.", M.tool ? "T" : "празно", M.tool);
        if (!M.tool) snprintf(b, sizeof b, "В шпиндела няма инструмент. Циклите с 3D пробник изискват T10.");
        ui_set_text(warn_txt, b);
    }
    lv_obj_set_hidden(res_title, prb_warn || !M.res.seq);
    lv_obj_set_hidden(res_rows, prb_warn);
    lv_obj_set_hidden(res_note, prb_warn);
    if (M.res.seq != res_seq) {
        res_seq = M.res.seq;
        lv_obj_clean(res_rows);
        if (!M.res.seq) ui_set_text(res_note, "Позиционирай пробника над или в детайла и избери цикъл.");
        else {
            ui_set_text(res_title, M.res.title);
            for (int i = 0; i < M.res.nrows; i++) {
                lv_obj_t *v;
                lv_obj_t *r = ui_kv(res_rows, M.res.rows[i][0], &v);
                (void)r;
                ui_set_text(v, M.res.rows[i][1]);
            }
            ui_set_text(res_note, M.res.note);
        }
        hmap_result(res_rows);
    }
}
