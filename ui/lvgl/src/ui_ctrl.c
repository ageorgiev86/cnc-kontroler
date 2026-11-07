/* Оверрайдите (подаване, шпиндел, бърз ход), лентата с напредъка и бутоните на цикъла (.ctrl и .cbtns в макета). */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "program.h"
#include "ui.h"
#include "ui_style.h"

static ctrl_t *C;

static void ev_fov(lv_event_t *e) { machine_fov((int)(intptr_t)lv_event_get_user_data(e), lv_tick_get()); }
static void ev_sov(lv_event_t *e) { machine_sov((int)(intptr_t)lv_event_get_user_data(e), lv_tick_get()); }
static void ev_rov(lv_event_t *e) { machine_rov((int)(intptr_t)lv_event_get_user_data(e)); }
static void ev_start(lv_event_t *e) { machine_start(lv_tick_get()); }
static void ev_hold(lv_event_t *e) { machine_hold(); }
static void ev_stop(lv_event_t *e) { machine_stop(lv_tick_get()); }
static void ev_rewind(lv_event_t *e) { machine_rewind(lv_tick_get()); }
static void ev_here(lv_event_t *e) { ui_run_from_line(); }

/* карта на оверрайд: надпис + програмирана стойност, голяма стойност, ред с бутони */
static lv_obj_t *ov_card(lv_obj_t *parent, const char *title, lv_obj_t **prog, lv_obj_t **act)
{
    lv_obj_t *c = ui_card(parent, NULL, NULL);
    lv_obj_t *b = ui_box(c);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_style_pad_ver(b, EM(0.5f), 0);
    lv_obj_set_style_pad_hor(b, EM(0.6f), 0);
    ui_flex(b, LV_FLEX_FLOW_COLUMN, 0.45f);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *top = ui_box(b);
    lv_obj_set_width(top, LV_PCT(100));
    ui_flex(top, LV_FLEX_FLOW_ROW_WRAP, 0.4f);   /* .ovtop: пренася се, ако не стига място */
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t *t = ui_label(top, F.ui_xs, C_DIM, title);
    lv_obj_set_style_text_letter_space(t, EM(0.1f), 0);
    *prog = ui_label(top, F.num_sm, C_DIM, "");
    *act = ui_label(b, F.num_ov, C_FG, "0");
    return b;
}

static lv_obj_t *row3(lv_obj_t *parent, lv_obj_t **mid, lv_event_cb_t cb)
{
    lv_obj_t *r = ui_box(parent);
    lv_obj_set_width(r, LV_PCT(100));
    static int32_t cols[] = {LV_GRID_FR(4), LV_GRID_FR(5), LV_GRID_FR(4), LV_GRID_TEMPLATE_LAST}, rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(r, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(r, cols, rows);
    lv_obj_set_style_pad_column(r, EM(0.3f), 0);
    static const char *const T[3] = {"-", "100%", "+"};
    static const int D[3] = {-10, 0, 10};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = ui_btn(r, T[i], BTN_NORMAL, false);
        lv_obj_set_height(b, EM(2.6f));
        lv_obj_set_style_pad_hor(b, 0, 0);
        if (i == 1) { lv_obj_set_style_text_font(b, F.num_md, 0); lv_obj_set_style_text_letter_space(b, 0, 0); *mid = lv_obj_get_child(b, 0); }
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)D[i]);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
    return r;
}

static lv_obj_t *prog_item(lv_obj_t *parent, const char *title, lv_obj_t **val)
{
    lv_obj_t *d = ui_box(parent);
    ui_flex(d, LV_FLEX_FLOW_COLUMN, 0.05f);
    lv_obj_set_flex_align(d, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *t = ui_label(d, F.ui_xs, C_DIM, title);
    lv_obj_set_style_text_letter_space(t, EM(0.1f), 0);
    *val = ui_label(d, F.num_pg, C_FG, "—");
    return d;
}

lv_obj_t *ctrl_create(ctrl_t *c, lv_obj_t *parent)
{
    memset(c, 0, sizeof *c);
    C = c;
    lv_obj_t *ctrl = ui_box(parent);
    lv_obj_set_width(ctrl, LV_PCT(100));
    lv_obj_set_layout(ctrl, LV_LAYOUT_GRID);
    static int32_t col_l[] = {LV_GRID_FR(10), LV_GRID_FR(10), LV_GRID_FR(8), LV_GRID_FR(16), LV_GRID_TEMPLATE_LAST};
    static int32_t col_p[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_l[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST}, row_p[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(ctrl, g_portrait ? col_p : col_l, g_portrait ? row_p : row_l);
    lv_obj_set_style_pad_gap(ctrl, EM(0.6f), 0);

    lv_obj_t *b = ov_card(ctrl, "ПОДАВАНЕ", &c->f_prog, &c->f_act);
    row3(b, &c->f_ov, ev_fov);
    lv_obj_set_grid_cell(lv_obj_get_parent(b), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    b = ov_card(ctrl, "ШПИНДЕЛ", &c->s_prog, &c->s_act);
    row3(b, &c->s_ov, ev_sov);
    lv_obj_set_grid_cell(lv_obj_get_parent(b), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t *rp;
    b = ov_card(ctrl, "БЪРЗ ХОД", &rp, &c->r_act);
    ui_set_text(rp, "G0");
    lv_obj_t *seg = ui_box(b);
    lv_obj_set_width(seg, LV_PCT(100));
    static int32_t sc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, sr[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(seg, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(seg, sc, sr);
    lv_obj_set_style_pad_column(seg, EM(0.25f), 0);
    static const char *const RT[3] = {"25", "50", "100"};
    static const int RV[3] = {25, 50, 100};
    for (int i = 0; i < 3; i++) {
        c->r_btn[i] = ui_btn(seg, RT[i], i == 2 ? BTN_ON : BTN_NORMAL, true);
        lv_obj_add_event_cb(c->r_btn[i], ev_rov, LV_EVENT_CLICKED, (void *)(intptr_t)RV[i]);
        lv_obj_set_grid_cell(c->r_btn[i], LV_GRID_ALIGN_STRETCH, i, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
    lv_obj_set_grid_cell(lv_obj_get_parent(b), LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    /* напредък: лента и проценти, изминало / остава / край */
    lv_obj_t *pg = ui_box(ctrl);
    lv_obj_set_style_bg_color(pg, C_SURF, 0);
    lv_obj_set_style_bg_opa(pg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pg, C_EDGE, 0);
    lv_obj_set_style_border_width(pg, 1, 0);
    lv_obj_set_style_radius(pg, EM(0.45f), 0);
    ui_glow(pg, 1.1f, 90);
    lv_obj_set_style_pad_ver(pg, EM(0.45f), 0);
    lv_obj_set_style_pad_hor(pg, EM(0.7f), 0);
    lv_obj_set_layout(pg, LV_LAYOUT_GRID);
    static int32_t pc_l[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, pr_l[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static int32_t pc_p[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST}, pr_p[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(pg, g_portrait ? pc_p : pc_l, g_portrait ? pr_p : pr_l);
    lv_obj_set_style_pad_column(pg, EM(0.9f), 0);
    lv_obj_set_style_pad_row(pg, EM(0.3f), 0);
    lv_obj_set_grid_align(pg, LV_GRID_ALIGN_STRETCH, LV_GRID_ALIGN_CENTER);
    if (g_portrait) lv_obj_set_grid_cell(pg, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_STRETCH, 1, 1);
    else lv_obj_set_grid_cell(pg, LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t *el = prog_item(pg, "ИЗМИНАЛО", &c->elapsed);
    lv_obj_t *barbox = ui_box(pg);
    ui_flex(barbox, LV_FLEX_FLOW_COLUMN, 0.05f);
    lv_obj_set_flex_align(barbox, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *bar = c->bar = lv_bar_create(barbox);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), EM(0.55f));
    lv_obj_set_style_bg_color(bar, C_SURF3, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, EM(0.3f), 0);
    lv_obj_set_style_bg_color(bar, C_INFO, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, EM(0.3f), LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 1000);
    c->pct = ui_label(barbox, F.num_sm, C_DIM, "0 %");
    lv_obj_t *re = prog_item(pg, "ОСТАВА", &c->remain);
    lv_obj_t *en = prog_item(pg, "КРАЙ", &c->end);
    if (g_portrait) {
        lv_obj_set_grid_cell(el, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(barbox, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(re, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(en, LV_GRID_ALIGN_START, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    } else {   /* лентата отгоре на цялата ширина, под нея трите стойности */
        lv_obj_set_grid_cell(barbox, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(el, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
        lv_obj_set_grid_cell(re, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
        lv_obj_set_grid_cell(en, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    }
    return ctrl;
}

lv_obj_t *cbtns_create(lv_obj_t *parent)
{
    lv_obj_t *r = ui_box(parent);
    lv_obj_set_width(r, LV_PCT(100));
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(r, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(r, cols, rows);
    lv_obj_set_style_pad_column(r, EM(0.45f), 0);
    static const struct { const char *t; btn_kind_t k; lv_event_cb_t cb; } B[5] = {
        {"ЦИКЪЛ СТАРТ", BTN_GO, ev_start}, {"ПАУЗА", BTN_HOLD, ev_hold}, {"СТОП", BTN_STOP, ev_stop},
        {"REWIND", BTN_NORMAL, ev_rewind}, {"RUN FROM HERE", BTN_NORMAL, ev_here}};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = ui_btn(r, B[i].t, B[i].k, false);
        lv_obj_set_height(b, EM(3.6f));
        lv_obj_set_style_text_font(b, F.ui_md, 0);
        lv_obj_add_event_cb(b, B[i].cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
    return r;
}

static void hms(char *b, size_t n, float s)
{
    int t = (int)(s + 0.5f);
    if (t < 0) t = 0;
    snprintf(b, n, "%d:%02d:%02d", t / 3600, t / 60 % 60, t % 60);
}

void ctrl_update(ctrl_t *c, uint32_t now)
{
    char b[64], h[24];
    snprintf(b, sizeof b, "F%d мм/мин", (int)M.feed_prog);
    ui_set_text(c->f_prog, b);
    snprintf(b, sizeof b, "%d", (int)(M.feed_act + 0.5f));
    ui_set_text(c->f_act, b);
    snprintf(b, sizeof b, "%d%%", M.fov);
    ui_set_text(c->f_ov, b);
    snprintf(b, sizeof b, "S%d · %s · Modbus #1", M.sp_cmd, M.sp_on ? "CW" : "спрян");
    ui_set_text(c->s_prog, b);
    snprintf(b, sizeof b, "%d", (int)(M.sp_act + 0.5f));
    ui_set_text(c->s_act, b);
    snprintf(b, sizeof b, "%d%%", M.sov);
    ui_set_text(c->s_ov, b);
    snprintf(b, sizeof b, "%d%%", M.rov);
    ui_set_text(c->r_act, b);
    static const int RV[3] = {25, 50, 100};
    static int shown = -1;
    if (shown != M.rov) { for (int i = 0; i < 3; i++) ui_btn_set_kind(c->r_btn[i], RV[i] == M.rov ? BTN_ON : BTN_NORMAL); shown = M.rov; }

    float tot = M.t_total > 0 ? M.t_total : P.t_total > 0 ? P.t_total : 1, pct = M.done_t / tot * 100;
    lv_bar_set_value(c->bar, (int32_t)(pct * 10), LV_ANIM_OFF);
    snprintf(b, sizeof b, "%d %%", (int)pct);
    ui_set_text(c->pct, b);
    hms(h, sizeof h, M.run_time);
    ui_set_text(c->elapsed, h);
    bool run = M.state == MS_RUN || M.state == MS_HOLD;
    float rem = machine_remaining_s();
    hms(h, sizeof h, rem);
    snprintf(b, sizeof b, "%s%s", M.prog_done ? "" : "~", M.prog_done ? "0:00:00" : h);
    ui_set_text(c->remain, b);
    if (M.prog_done) ui_set_text(c->end, "готово");
    else if (run) {
        time_t t = time(NULL) + (time_t)rem;
        struct tm tm;
        localtime_r(&t, &tm);
        snprintf(b, sizeof b, "~%02d:%02d", tm.tm_hour, tm.tm_min);
        ui_set_text(c->end, b);
    } else ui_set_text(c->end, "—");
}
