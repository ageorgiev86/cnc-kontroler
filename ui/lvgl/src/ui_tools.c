/* Екранът „Инструменти“ (.tools в макета): таблицата на инструментите и магазинът.
 * Таблицата е един lv_table (и при 255 гнезда е един обект); редът в шпиндела, избраният ред и колоните
 * се оцветяват в LV_EVENT_DRAW_TASK_ADDED. Магазинът се рисува директно (LV_EVENT_DRAW_MAIN): карусел
 * с едно или няколко колела, верига или линейна поставка. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "ui.h"
#include "ui_style.h"

static lv_obj_t *tbl, *cnt, *mag_card_title, *mag_pos, *mag, *atc_led, *atc_txt, *atc_btn, *done_btn, *tc_btn, *pick_t, *pick_d, *mag_body;
static unsigned tbl_sum;
static int tbl_n = -1;
static float drawn_ph = -1, drawn_w[6];
static int drawn_tool = -1, drawn_sel = -1, drawn_n = -1;
static char drawn_key[200];

/* ---------- таблица ---------- */
static unsigned table_sum(int n)
{
    unsigned h = (unsigned)n * 31u + (unsigned)M.tool * 7u + (unsigned)M.sel_tool * 131u;
    for (int i = 1; i <= n; i++) {
        h = h * 33u + (unsigned)lround(TT[i].len * 1000) + (unsigned)lround(TT[i].dia * 1000) * 3u + (unsigned)lround(TT[i].wear * 1000) * 5u;
        for (const char *p = TT[i].name; *p; p++) h = h * 33u + (unsigned char)*p;
    }
    return h;
}

static void fill_table(void)
{
    int n = mag_n();
    if (n != tbl_n) { lv_table_set_row_count(tbl, (uint32_t)n + 1); tbl_n = n; }
    char b[64];
    for (int i = 1; i <= n; i++) {
        snprintf(b, sizeof b, "T%d", i);
        lv_table_set_cell_value(tbl, i, 0, b);
        const tool_t *t = &TT[i];
        lv_table_set_cell_value(tbl, i, 1, t->name[0] ? t->name : "Празно гнездо");
        for (int c = 0; c < 3; c++) {
            double v = c == 0 ? t->len : c == 1 ? t->dia : t->wear;
            if (t->name[0]) snprintf(b, sizeof b, "%.3f", v); else snprintf(b, sizeof b, "—");
            lv_table_set_cell_value(tbl, i, 2 + c, b);
        }
    }
    tbl_sum = table_sum(n);
    snprintf(b, sizeof b, "%d %s", n, cfg_num("mag.mOn") ? "гнезда" : "инструмента");
    ui_set_text(cnt, b);
}

static void on_tbl_size(lv_event_t *e)
{
    int32_t w = lv_obj_get_content_width(tbl);
    static const int pct[5] = {9, 37, 18, 18, 18};
    for (int c = 0; c < 5; c++) lv_table_set_column_width(tbl, c, w * pct[c] / 100);
}

/* оцветяване на клетките */
static void on_tbl_draw(lv_event_t *e)
{
    lv_draw_task_t *t = lv_event_get_param(e);
    lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(t);
    if (base->part != LV_PART_ITEMS) return;
    int row = (int)base->id1, col = (int)base->id2;
    lv_draw_label_dsc_t *ld = lv_draw_task_get_label_dsc(t);
    lv_draw_fill_dsc_t *fd = lv_draw_task_get_fill_dsc(t);
    if (row == 0) {
        if (ld) {
            ld->font = F.ui_xs; ld->color = C_DIM; ld->letter_space = EM(0.08f);
            if (col >= 2) ld->align = LV_TEXT_ALIGN_RIGHT;
        }
        return;
    }
    if (fd && row == M.sel_tool) { fd->color = C_ACC; fd->opa = 77; }
    if (!ld) return;
    bool empty = !TT[row].name[0];
    if (col == 0) { ld->font = F.ui_md; ld->color = C_HI; }
    else if (col == 1) {
        ld->font = F.txt;
        ld->color = empty ? C_FAINT : C_FG;
        if (row == M.tool) {   /* знак „шпиндел“ след името (.badge) */
            lv_area_t a;
            lv_draw_task_get_area(t, &a);
            int32_t tw = ui_text_w(ld->text, ld->font);
            lv_area_t b = {a.x1 + tw + EM(0.4f), a.y1 + EM(0.05f), 0, 0};
            const char *bt = M.type == MT_LATHE ? "в работа" : "шпиндел";
            b.x2 = b.x1 + ui_text_w(bt, F.num_sm) + EM(0.7f);
            b.y2 = b.y1 + F.num_sm->line_height + EM(0.2f);
            lv_draw_rect_dsc_t rd;
            lv_draw_rect_dsc_init(&rd);
            rd.bg_color = C_ACC; rd.bg_opa = LV_OPA_COVER; rd.radius = EM(0.25f);
            lv_draw_rect(base->layer, &rd, &b);
            lv_draw_label_dsc_t bl;
            lv_draw_label_dsc_init(&bl);
            bl.text = bt; bl.font = F.num_sm; bl.color = C_ACC_INK; bl.align = LV_TEXT_ALIGN_CENTER;
            lv_area_t bb = b;
            bb.y1 += EM(0.1f);
            lv_draw_label(base->layer, &bl, &bb);
        }
    } else {
        ld->font = F.num_md;
        ld->color = empty ? C_FAINT : C_FG;
        ld->align = LV_TEXT_ALIGN_RIGHT;
    }
}

static void name_cb_apply(int row, const char *txt)
{
    snprintf(TT[row].name, sizeof TT[row].name, "%s", txt);
    if (!txt[0]) TT[row].len = TT[row].dia = TT[row].wear = 0;
    if (row == M.tool) machine_select_tool(row);
}
static int on_name(const char *txt, void *ud) { name_cb_apply((int)(intptr_t)ud, txt); return 0; }
static void on_num(double v, void *ud)
{
    int k = (int)(intptr_t)ud, row = k / 8, col = k % 8;
    if (col == 2) TT[row].len = v;
    else if (col == 3) TT[row].dia = v;
    else TT[row].wear = v;
    if (row == M.tool) machine_select_tool(row);
}

static void on_tbl_click(lv_event_t *e)
{
    uint32_t row, col;
    lv_table_get_selected_cell(tbl, &row, &col);
    if (row == LV_TABLE_CELL_NONE || row == 0 || (int)row > mag_n()) return;
    M.sel_tool = (int)row;
    char title[64];
    if (col == 1) {
        snprintf(title, sizeof title, "T%u · име на инструмента", (unsigned)row);
        dlg_text(title, TT[row].name, 39, on_name, (void *)(intptr_t)row);
    } else if (col >= 2 && TT[row].name[0]) {
        static const char *const N[3] = {"дължина", "диаметър", "износване"};
        static const double MIN[3] = {0, 0, -5}, MAX[3] = {1000, 500, 5};
        double cur = col == 2 ? TT[row].len : col == 3 ? TT[row].dia : TT[row].wear;
        snprintf(title, sizeof title, "T%u · %s", (unsigned)row, N[col - 2]);
        dlg_num(title, "мм", col == 2 ? "Дължината от челото на шпиндела до върха (G43). Измерва се най-точно с toolsetter-а."
                           : col == 3 ? "Диаметърът на инструмента (за компенсация G41/G42 и за 3D изгледа)."
                                      : "Корекция за износване на диаметъра.",
                cur, MIN[col - 2], MAX[col - 2], 3, on_num, (void *)(intptr_t)(row * 8 + col));
    }
}

/* ---------- магазин ---------- */
typedef struct { float x, y; } pt2_t;

static int mtype(void)
{
    const char *t = cfg_str("mag.mtype");
    return !strcmp(t, "chain") ? 1 : !strcmp(t, "linear") ? 2 : 0;
}

static void mag_geom(int32_t W, int32_t H, int n, float *size, pt2_t *pos)
{
    float em = g_em;
    int ty = mtype();
    if (ty == 0) {
        int wn = mag_wheels(), per = (n + wn - 1) / wn;
        int cols = wn <= 3 ? wn : (wn + 1) / 2, rows = (wn + cols - 1) / cols;
        float cw = (float)W / cols, ch = (float)H / rows, D = fminf(cw, ch);
        float R0 = D / 2 - (wn > 1 ? 0.9f : 1.6f) * em;
        *size = fminf(2.7f * em, 2 * (float)M_PI * R0 / per * 0.8f);
        float R = D / 2 - *size / 2 - 0.2f * em;
        for (int i = 1; i <= n; i++) {
            int w = (i - 1) / per < wn ? (i - 1) / per : wn - 1, lc = (i - 1) % per + 1;
            float cx = (w % cols + 0.5f) * cw, cy = (w / cols + 0.5f) * ch;
            float g = (lc - M.wph[w]) / per * 2 * (float)M_PI;
            pos[i].x = cx + sinf(g) * R;
            pos[i].y = cy - cosf(g) * R;
        }
    } else if (ty == 1) {
        float s = 2 * em, Rr = 0, L = 0, Pp = 0;
        for (int k = 0; k < 2; k++) {
            Rr = H / 2.0f - s / 2 - 0.25f * em;
            L = fmaxf(0, W - 2 * Rr - s - 0.5f * em);
            Pp = 2 * L + 2 * (float)M_PI * Rr;
            s = fminf(2.4f * em, Pp / n * 0.78f);
        }
        Rr = H / 2.0f - s / 2 - 0.25f * em;
        L = fmaxf(0, W - 2 * Rr - s - 0.5f * em);
        Pp = 2 * L + 2 * (float)M_PI * Rr;
        *size = s;
        for (int i = 1; i <= n; i++) {
            float u = fmodf(fmodf((i - M.mag_ph) / n, 1) + 1, 1) * Pp, x, y;
            if (u < L / 2) { x = u; y = -Rr; }
            else if ((u -= L / 2) < (float)M_PI * Rr) { float g = u / Rr; x = L / 2 + sinf(g) * Rr; y = -cosf(g) * Rr; }
            else if ((u -= (float)M_PI * Rr) < L) { x = L / 2 - u; y = Rr; }
            else if ((u -= L) < (float)M_PI * Rr) { float g = u / Rr; x = -L / 2 - sinf(g) * Rr; y = cosf(g) * Rr; }
            else { u -= (float)M_PI * Rr; x = -L / 2 + u; y = -Rr; }
            pos[i].x = W / 2.0f + x;
            pos[i].y = H / 2.0f + y;
        }
    } else {
        int rows = (n + 19) / 20 < 3 ? (n + 19) / 20 : 3, per = (n + rows - 1) / rows;
        float cw = (float)W / per;
        *size = fminf(2.6f * em, cw * 0.82f);
        for (int i = 1; i <= n; i++) {
            int k = i - 1, r = k / per, c = k % per;
            pos[i].x = (c + 0.5f) * cw;
            pos[i].y = H / 2.0f + (rows == 1 ? 0.6f : (r - (rows - 1) / 2.0f) * 1.4f + 0.65f) * *size + H * 0.1f - *size;
        }
    }
}

static void label_at(lv_layer_t *l, const char *txt, const lv_font_t *f, lv_color_t c, int32_t cx, int32_t cy)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = txt; d.font = f; d.color = c; d.align = LV_TEXT_ALIGN_CENTER;
    d.text_local = 1;   /* текстът е в локален буфер: копира се */
    int32_t w = ui_text_w(txt, f) + 4, h = f->line_height;
    lv_area_t a = {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
    lv_draw_label(l, &d, &a);
}

static void on_mag_draw(lv_event_t *e)
{
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t co;
    lv_obj_get_coords(mag, &co);
    int32_t W = lv_area_get_width(&co), H = lv_area_get_height(&co);
    int n = mag_n(), ty = mtype(), wn = mag_wheels();
    static pt2_t pos[TOOL_MAX + 1];
    float size;
    mag_geom(W, H, n, &size, pos);
    float em = g_em;
    lv_draw_rect_dsc_t rd;

    if (ty == 0 && wn == 1) {   /* пръстен, „смяна“ и главината */
        lv_draw_rect_dsc_init(&rd);
        rd.bg_opa = LV_OPA_TRANSP; rd.border_color = C_LINE; rd.border_width = 1; rd.radius = LV_RADIUS_CIRCLE;
        int32_t in = EM(1.4f);
        lv_area_t a = {co.x1 + in, co.y1 + in, co.x2 - in, co.y2 - in};
        lv_draw_rect(l, &rd, &a);
        label_at(l, "ШПИНДЕЛ", F.ui_xs, C_DIM, co.x1 + W / 2, co.y1 + H / 2 - EM(0.8f));
        char t[16];
        if (M.tool) snprintf(t, sizeof t, "T%d", M.tool); else snprintf(t, sizeof t, "—");
        label_at(l, t, F.ui_lg, C_FG, co.x1 + W / 2, co.y1 + H / 2 + EM(0.5f));
        label_at(l, "СМЯНА", F.txt_sm, C_ACC, co.x1 + W / 2 + EM(1.3f), co.y1 + F.txt_sm->line_height / 2);
        lv_draw_triangle_dsc_t td;
        lv_draw_triangle_dsc_init(&td);
        td.color = C_ACC; td.opa = LV_OPA_COVER;
        int32_t tx = co.x1 + W / 2 - EM(0.2f), ty0 = co.y1 + EM(0.2f);
        td.p[0].x = tx - EM(0.3f); td.p[0].y = ty0;
        td.p[1].x = tx + EM(0.3f); td.p[1].y = ty0;
        td.p[2].x = tx; td.p[2].y = ty0 + EM(0.35f);
        lv_draw_triangle(l, &td);
    } else if (ty == 0) {         /* надписите на колелата */
        int per = (n + wn - 1) / wn, cols = wn <= 3 ? wn : (wn + 1) / 2, rows = (wn + cols - 1) / cols;
        int curw = (M.mag_pos - 1) / per < wn ? (M.mag_pos - 1) / per : wn - 1;
        for (int w = 0; w < wn; w++) {
            char t[32];
            int32_t cx = co.x1 + (int32_t)((w % cols + 0.5f) * W / cols), cy = co.y1 + (int32_t)((w / cols + 0.5f) * H / rows);
            snprintf(t, sizeof t, "КОЛЕЛО %d", w + 1);
            label_at(l, t, F.ui_xs, w == curw ? C_HI : C_DIM, cx, cy - EM(0.4f));
            snprintf(t, sizeof t, "%d–%d", w * per + 1, (w + 1) * per < n ? (w + 1) * per : n);
            label_at(l, t, F.num_sm, C_FAINT, cx, cy + EM(0.45f));
        }
    } else if (ty == 1) {
        label_at(l, "ШПИНДЕЛ", F.ui_xs, C_DIM, co.x1 + W / 2, co.y1 + H / 2 - EM(0.7f));
        char t[16];
        if (M.tool) snprintf(t, sizeof t, "T%d", M.tool); else snprintf(t, sizeof t, "—");
        label_at(l, t, F.ui_lg, C_FG, co.x1 + W / 2, co.y1 + H / 2 + EM(0.5f));
    }

    bool tiny = size < 1.15f * em;
    const lv_font_t *nf = size >= 2.4f * em ? F.num_pg : size >= 1.8f * em ? F.num_md : F.num_sm;
    int per = (n + wn - 1) / wn;
    for (int i = 1; i <= n; i++) {
        bool empty = i == M.tool || !TT[i].name[0];
        bool top;
        if (ty == 0) { int w = (i - 1) / per < wn ? (i - 1) / per : wn - 1; top = i == M.mag_pos && fabsf(M.wph[w] - ((i - 1) % per + 1)) < 0.05f; }
        else top = i == M.mag_pos && fabsf(M.mag_ph - M.mag_pos) < 0.05f;
        bool sel = i == M.sel_tool && i != M.tool;
        int32_t cx = co.x1 + (int32_t)pos[i].x, cy = co.y1 + (int32_t)pos[i].y, r = (int32_t)(size / 2);
        lv_area_t a = {cx - r, cy - r, cx + r, cy + r};
        lv_draw_rect_dsc_init(&rd);
        rd.radius = ty == 2 ? EM(0.3f) : LV_RADIUS_CIRCLE;
        if (tiny) {
            rd.bg_opa = LV_OPA_COVER;
            rd.bg_color = top ? C_HI : sel ? C_ACC : empty ? C_LINE : C_EDGE2;
            lv_draw_rect(l, &rd, &a);
            continue;
        }
        if (top) {   /* .pk.top: синя рамка и мек ореол */
            lv_draw_rect_dsc_t g;
            lv_draw_rect_dsc_init(&g);
            g.radius = rd.radius; g.bg_opa = LV_OPA_TRANSP; g.border_color = C_ACC; g.border_opa = 77; g.border_width = EM(0.15f);
            lv_area_t ga = {a.x1 - EM(0.15f), a.y1 - EM(0.15f), a.x2 + EM(0.15f), a.y2 + EM(0.15f)};
            lv_draw_rect(l, &g, &ga);
        }
        rd.bg_opa = empty ? LV_OPA_TRANSP : sel ? 77 : LV_OPA_COVER;
        rd.bg_color = sel ? C_ACC : C_SURF2;
        rd.border_width = 1;
        rd.border_color = top ? C_ACC : empty ? C_FAINT : C_LINE;
        lv_draw_rect(l, &rd, &a);
        char t[8];
        snprintf(t, sizeof t, "%d", i);
        label_at(l, t, nf, empty ? C_FAINT : sel ? C_ACC : C_FG, cx, cy);
    }
    if (ty == 2) {   /* шпинделът над поставката */
        int a0 = (int)floorf(M.mag_ph);
        float f = M.mag_ph - a0;
        int i0 = a0 < 1 ? 1 : a0 > n ? n : a0, i1 = a0 + 1 > n ? n : a0 + 1;
        float x = pos[i0].x + (pos[i1].x - pos[i0].x) * f, y = fminf(pos[i0].y, pos[i1].y) - size * 0.55f - 1.2f * em;
        char t[16];
        if (M.tool) snprintf(t, sizeof t, "T%d", M.tool); else snprintf(t, sizeof t, "празен");
        label_at(l, t, F.ui_sm, C_ACC, co.x1 + (int32_t)x, co.y1 + (int32_t)y);
    }
}

static void on_tc(lv_event_t *e) { machine_tool_change(M.sel_tool, lv_tick_get()); }
static void on_meas(lv_event_t *e) { machine_measure_tool(lv_tick_get()); }
static void on_done(lv_event_t *e) { machine_start(lv_tick_get()); }
static void on_atc(lv_event_t *e)
{
    bool on = !cfg_num("mag.atcOn");
    drf_set("mag.atcOn", on ? "1" : "0");
    /* превключва се веднага (без „Запиши“), както в макета */
    cfg_commit_key("mag.atcOn");
    machine_toast(on ? "Автоматичната смяна е включена" : "Автоматичната смяна е изключена: M6 = ръчна смяна", lv_tick_get(), 3500);
}
static void on_pick_num(double v, void *ud)
{
    int t = (int)lround(v);
    if (t < 1 || t > mag_n()) { char b[48]; snprintf(b, sizeof b, "Няма инструмент T%d", t); machine_toast(b, lv_tick_get(), 3000); return; }
    M.sel_tool = t;
    char b[96];
    snprintf(b, sizeof b, "Избран е T%d · %s", t, TT[t].name[0] ? TT[t].name : "празно гнездо");
    machine_toast(b, lv_tick_get(), 3000);
    /* редът се показва в таблицата */
    lv_obj_scroll_to_y(tbl, (t - 3) * (F.txt->line_height + EM(1.0f)), LV_ANIM_ON);
}
static void on_pick(lv_event_t *e)
{
    dlg_num("Инструмент за смяна", "", "Номерът на инструмента (T), който да се постави в шпиндела. После натиснете „Смени“.",
            M.sel_tool, 1, mag_n(), 0, on_pick_num, NULL);
}

void tools_create(lv_obj_t *scr)
{
    static int32_t cols[3], rows[3];
    if (g_portrait) {
        cols[0] = LV_GRID_FR(1); cols[1] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_FR(1); rows[1] = LV_GRID_CONTENT; rows[2] = LV_GRID_TEMPLATE_LAST;
    } else {
        cols[0] = LV_GRID_FR(58); cols[1] = LV_GRID_FR(42); cols[2] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_FR(1); rows[1] = LV_GRID_TEMPLATE_LAST;
    }
    ui_grid(scr, cols, rows, 0.6f);

    /* таблица */
    lv_obj_t *hr;
    lv_obj_t *c = ui_card(scr, "ТАБЛИЦА НА ИНСТРУМЕНТИТЕ", &hr);
    ui_cell(c, 0, 1, 0, 1);
    if (!g_portrait) ui_label(hr, F.txt_sm, C_FAINT, "натиснете име или стойност за промяна  ·");
    cnt = ui_label(hr, F.num_sm, C_DIM, "");
    tbl = lv_table_create(c);
    lv_obj_remove_style_all(tbl);
    lv_obj_set_width(tbl, LV_PCT(100));
    lv_obj_set_flex_grow(tbl, 1);
    ui_scroll_v(tbl);
    lv_obj_set_style_text_font(tbl, F.txt, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tbl, C_FG, LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(tbl, EM(0.5f), LV_PART_ITEMS);
    lv_obj_set_style_pad_hor(tbl, EM(0.6f), LV_PART_ITEMS);
    lv_obj_set_style_border_side(tbl, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_border_color(tbl, C_LINE, LV_PART_ITEMS);
    lv_obj_set_style_border_width(tbl, 1, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tbl, C_SURF, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(tbl, LV_OPA_COVER, LV_PART_ITEMS);   /* иначе избраният ред няма фон за оцветяване */
    lv_obj_set_style_bg_color(tbl, C_SURF3, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(tbl, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_table_set_column_count(tbl, 5);
    lv_table_set_row_count(tbl, 1);
    static const char *const H[5] = {"T", "ОПИСАНИЕ", "ДЪЛЖИНА", "Ø", "ИЗНОСВ."};
    for (int i = 0; i < 5; i++) lv_table_set_cell_value(tbl, 0, i, H[i]);
    lv_obj_set_send_draw_task_events(tbl, true);
    lv_obj_add_event_cb(tbl, on_tbl_draw, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_event_cb(tbl, on_tbl_click, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(tbl, on_tbl_size, LV_EVENT_SIZE_CHANGED, NULL);

    /* магазин */
    lv_obj_t *m = ui_card(scr, "МАГАЗИН · КАРУСЕЛ", &hr);
    if (g_portrait) lv_obj_set_grid_cell(m, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 1, 1); else ui_cell(m, 1, 1, 0, 1);
    mag_card_title = lv_obj_get_child(lv_obj_get_child(m, 0), 0);
    mag_pos = ui_label(hr, F.num_sm, C_DIM, "");
    lv_obj_t *b = mag_body = ui_body(m, 0.6f);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    if (g_portrait) { lv_obj_set_flex_grow(b, 0); lv_obj_set_height(b, LV_SIZE_CONTENT); }
    mag = ui_box(b);
    lv_obj_add_event_cb(mag, on_mag_draw, LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t *atc = ui_box(b);
    lv_obj_set_width(atc, LV_PCT(100));
    lv_obj_set_style_min_height(atc, EM(2.6f), 0);
    lv_obj_set_style_bg_color(atc, C_PANEL, 0);
    lv_obj_set_style_bg_opa(atc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(atc, C_LINE, 0);
    lv_obj_set_style_border_width(atc, 1, 0);
    lv_obj_set_style_radius(atc, EM(0.35f), 0);
    lv_obj_set_style_pad_hor(atc, EM(0.65f), 0);
    lv_obj_set_style_pad_ver(atc, EM(0.4f), 0);
    ui_flex(atc, LV_FLEX_FLOW_ROW, 0.5f);
    atc_led = ui_led(atc, 0.75f);
    atc_txt = ui_label(atc, F.txt, C_DIM, "");
    lv_obj_set_flex_grow(atc_txt, 1);
    lv_obj_set_width(atc_txt, EM(12));
    lv_label_set_long_mode(atc_txt, LV_LABEL_LONG_MODE_WRAP);
    atc_btn = ui_btn(atc, "АВТ. СМЯНА ВКЛ.", BTN_ON, true);
    lv_obj_add_event_cb(atc_btn, on_atc, LV_EVENT_CLICKED, NULL);
    done_btn = ui_btn(atc, "ГОТОВО, ПРОДЪЛЖИ", BTN_GO, true);
    lv_obj_add_event_cb(done_btn, on_done, LV_EVENT_CLICKED, NULL);

    static int32_t c2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, r1[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *g = ui_box(b);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, c2, r1, 0.35f);
    tc_btn = ui_btn(g, "СМЕНИ С T3", BTN_ON, false);
    lv_obj_add_event_cb(tc_btn, on_tc, LV_EVENT_CLICKED, NULL);
    ui_cell(tc_btn, 0, 1, 0, 1);
    lv_obj_t *mb = ui_btn(g, "ИЗМЕРИ ДЪЛЖИНА", BTN_NORMAL, false);
    lv_obj_add_event_cb(mb, on_meas, LV_EVENT_CLICKED, NULL);
    ui_cell(mb, 1, 1, 0, 1);

    lv_obj_t *tp = ui_box(b);
    lv_obj_set_width(tp, LV_PCT(100));
    ui_flex(tp, LV_FLEX_FLOW_COLUMN, 0.3f);
    lv_obj_set_flex_align(tp, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    ui_lab(tp, "ИНСТРУМЕНТ ЗА СМЯНА");
    lv_obj_t *pb = ui_btn(tp, "", BTN_NORMAL, false);
    lv_obj_delete(ui_btn_label(pb));
    lv_obj_set_size(pb, LV_PCT(100), EM(3.2f));
    lv_obj_set_style_bg_color(pb, C_PANEL, 0);
    lv_obj_set_flex_align(pb, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(pb, EM(0.8f), 0);
    pick_t = ui_label(pb, F.ui_lg, C_HI, "");
    pick_d = ui_label(pb, F.txt, C_DIM, "");
    lv_obj_add_event_cb(pb, on_pick, LV_EVENT_CLICKED, NULL);
}

void tools_update(uint32_t now)
{
    int n = mag_n();
    if (n != tbl_n || table_sum(n) != tbl_sum) { fill_table(); lv_obj_invalidate(tbl); }

    bool has_mag = cfg_num("mag.mOn") != 0, atc = machine_auto_tc();
    bool busy = !strncmp(M.act_label, "Смяна", 10) || M.mtc;
    int ty = mtype(), wn = mag_wheels();
    char b[200];

    /* размерът на магазина според вида (.mag, .mag.chain, .mag.multi, .mag.linear) */
    snprintf(b, sizeof b, "%d|%d|%d|%d|%d", has_mag, ty, wn, n, g_portrait);
    if (strcmp(b, drawn_key)) {
        snprintf(drawn_key, sizeof drawn_key, "%s", b);
        lv_obj_set_hidden(mag, !has_mag);
        if (ty == 0 && wn > 1) lv_obj_set_size(mag, g_portrait ? EM(24) : LV_PCT(100), g_portrait ? EM(15) : EM(21));
        else if (ty == 0) lv_obj_set_size(mag, EM(16.5f), EM(16.5f));
        else if (ty == 1) lv_obj_set_size(mag, n > 60 ? LV_PCT(100) : EM(23), n > 60 ? EM(17) : EM(12.5f));
        else lv_obj_set_size(mag, LV_PCT(100), EM(8));
        static const char *const TN[3] = {"МАГАЗИН · КАРУСЕЛ", "МАГАЗИН · ВЕРИГА", "МАГАЗИН · ЛИНЕЕН"};
        ui_set_text(mag_card_title, has_mag ? TN[ty] : "РЪЧНА СМЯНА");
        drawn_ph = -1;
    }
    bool moved = M.mag_ph != drawn_ph || M.tool != drawn_tool || M.sel_tool != drawn_sel || n != drawn_n;
    for (int w = 0; w < wn; w++) moved |= M.wph[w] != drawn_w[w];
    if (moved && has_mag) {
        lv_obj_invalidate(mag);
        drawn_ph = M.mag_ph; drawn_tool = M.tool; drawn_sel = M.sel_tool; drawn_n = n;
        memcpy(drawn_w, M.wph, sizeof drawn_w);
    }

    if (!has_mag) snprintf(b, sizeof b, M.tool ? "в шпиндела T%d" : "шпинделът е празен", M.tool);
    else if (wn > 1) {
        int per = (n + wn - 1) / wn;
        snprintf(b, sizeof b, "колело %d · гнездо %d · в шпиндела %s%d", (M.mag_pos - 1) / per + 1, M.mag_pos, M.tool ? "T" : "—", M.tool);
        if (!M.tool) b[strlen(b) - 1] = 0;
    } else snprintf(b, sizeof b, "гнездо %d", M.mag_pos);
    ui_set_text(mag_pos, b);

    /* състояние на смяната */
    const char *txt;
    char tb[200];
    if (M.mtc) { snprintf(tb, sizeof tb, "Ръчна смяна: поставете T%d и натиснете „Готово“", M.mtc); txt = tb; }
    else if (busy) { const char *p = M.act_label; if (!strncmp(p, "Смяна на инструмент ", 37)) p += 37; txt = p; }
    else if (!has_mag) { snprintf(tb, sizeof tb, "Без магазин · при M6 шпинделът отива на X %.0f Y %.0f и машината чака", cfg_num("mag.mcX"), cfg_num("mag.mcY")); txt = tb; }
    else if (!atc) txt = "Автоматичната смяна е изключена · M6 = ръчна смяна";
    else txt = "Магазинът е готов · инструментът е затегнат";
    ui_set_text(atc_txt, txt);
    lv_obj_set_style_text_color(atc_txt, busy ? C_ACC : (has_mag && !atc) ? C_WARN : C_DIM, 0);
    ui_led_set(atc_led, busy || (has_mag && !atc) ? C_AMB : C_OK, true);
    lv_obj_set_hidden(atc_btn, !has_mag || busy);
    static int atc_was = -1;
    if (atc != atc_was) {
        ui_btn_set_kind(atc_btn, atc ? BTN_ON : BTN_GHOST);
        ui_set_text(ui_btn_label(atc_btn), atc ? "АВТ. СМЯНА ВКЛ." : "АВТ. СМЯНА ИЗКЛ.");
        atc_was = atc;
    }
    lv_obj_set_hidden(done_btn, !(M.mtc && M.state == MS_HOLD));

    snprintf(b, sizeof b, "%s T%d", atc ? "СМЕНИ С" : "РЪЧНА СМЯНА С", M.sel_tool);
    ui_set_text(ui_btn_label(tc_btn), b);
    snprintf(b, sizeof b, "T%d", M.sel_tool);
    ui_set_text(pick_t, b);
    snprintf(b, sizeof b, "%s · натиснете, за да въведете номер", TT[M.sel_tool].name[0] ? TT[M.sel_tool].name : "празно");
    ui_set_text(pick_d, b);
}
