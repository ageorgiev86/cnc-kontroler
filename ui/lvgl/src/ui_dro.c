/* Картата „Позиция“ (.dro в макета): буква на оста с точка за Home, голямата стойност,
 * мерна единица и под нея машинна/работна стойност, остатък и следяща грешка. */
#include <math.h>
#include <stdio.h>

#include "ui.h"
#include "ui_style.h"

static dro_t *g_dro;

static void on_coord(lv_event_t *e)
{
    M.coord_mach = (bool)(intptr_t)lv_event_get_user_data(e);
    ui_btn_set_kind(g_dro->seg_work, M.coord_mach ? BTN_NORMAL : BTN_ON);
    ui_btn_set_kind(g_dro->seg_mach, M.coord_mach ? BTN_ON : BTN_NORMAL);
}

/* f3 от макета: 3 знака след точката, без „-0.000“, подравнено вдясно до w знака */
static void f3(char *out, size_t n, double v, int w)
{
    if (fabs(v) < 0.0005) v = 0;
    snprintf(out, n, "%*.3f", w, v);
}

/* Колоните са статични, защото LVGL пази указател към тях */
static int32_t col_p[4], row_p[3], col_l[5], row_l[2], sub_col[3], sub_row[3];

static void row_create(dro_row_t *r, lv_obj_t *parent, int a)
{
    lv_obj_t *row = r->row = ui_box(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, C_LINE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_layout(row, LV_LAYOUT_GRID);
    if (g_portrait) {
        lv_obj_set_grid_dsc_array(row, col_p, row_p);
        lv_obj_set_style_pad_ver(row, EM(0.4f), 0);
        lv_obj_set_style_pad_hor(row, EM(0.8f), 0);
        lv_obj_set_style_pad_column(row, EM(0.5f), 0);
        lv_obj_set_style_pad_row(row, EM(0.1f), 0);
    } else {
        lv_obj_set_grid_dsc_array(row, col_l, row_l);
        lv_obj_set_style_pad_ver(row, EM(0.05f), 0);
        lv_obj_set_style_pad_hor(row, EM(0.75f), 0);
        lv_obj_set_style_pad_column(row, EM(0.7f), 0);
    }

    /* буква на оста + точка за Home */
    lv_obj_t *ax = ui_box(row);
    lv_obj_set_overflow_visible(ax, true);
    ui_label(ax, F.ui_ax, C_HI, AX_NAME[a]);
    float ax_em = 1.95f;
    lv_obj_t *dot = r->dot = ui_box(ax);
    lv_obj_set_ignore_layout(dot, true);
    lv_obj_set_size(dot, EM(0.22f * ax_em), EM(0.22f * ax_em));
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, C_SURF3, 0);
    /* както в макета (right .1em, bottom -.05em): долу вдясно в колоната, на базовата линия на буквата.
     * Шрифтът на осите е само с главни букви, затова кутията свършва на базовата линия. */
    lv_obj_align(dot, LV_ALIGN_BOTTOM_RIGHT, -EM(0.1f * ax_em), EM(0.05f * ax_em) - F.ui_ax->base_line);

    r->val = ui_label(row, F.num_lg, C_FG, "0.000");
    lv_obj_set_style_text_align(r->val, LV_TEXT_ALIGN_RIGHT, 0);
    r->unit = ui_label(row, F.ui_xs, C_DIM, M.rotary[a] ? "°" : "мм");

    /* под реда: машинна/работна, остатък, следяща грешка */
    lv_obj_t *sub = ui_box(row);
    lv_obj_set_style_text_font(sub, F.num_sm, 0);
    lv_obj_set_style_text_color(sub, C_DIM, 0);
    r->sm = lv_label_create(sub);
    r->sd = lv_label_create(sub);
    lv_obj_t *fe = ui_box(sub);
    ui_flex(fe, LV_FLEX_FLOW_ROW, 0.4f);
    lv_obj_t *bar = r->fbar = lv_bar_create(fe);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, EM(3.6f), EM(0.45f));
    lv_obj_set_style_bg_color(bar, C_SURF3, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, EM(0.2f), 0);
    lv_obj_set_style_bg_color(bar, C_OK, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_clip_corner(bar, true, 0);
    lv_bar_set_range(bar, 0, 1000);
    r->ftxt = lv_label_create(fe);
    lv_obj_set_style_min_width(r->ftxt, EM(5.4f * 0.74f), 0);
    lv_obj_set_style_text_align(r->ftxt, LV_TEXT_ALIGN_RIGHT, 0);

    if (g_portrait) {
        lv_obj_set_grid_cell(ax, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 0, 2);
        lv_obj_set_grid_cell(r->val, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(r->unit, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(sub, LV_GRID_ALIGN_STRETCH, 1, 2, LV_GRID_ALIGN_CENTER, 1, 1);
        ui_flex(sub, LV_FLEX_FLOW_ROW, 0.6f);
        lv_obj_set_flex_align(sub, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_set_grid_cell(ax, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(sub, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(r->val, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(r->unit, LV_GRID_ALIGN_START, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_layout(sub, LV_LAYOUT_GRID);
        lv_obj_set_grid_dsc_array(sub, sub_col, sub_row);
        lv_obj_set_style_pad_column(sub, EM(1.1f), 0);
        lv_obj_set_style_pad_row(sub, EM(0.05f), 0);
        lv_obj_set_grid_cell(r->sm, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(r->sd, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_set_grid_cell(fe, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 1, 1);
    }
}

void dro_create(dro_t *d, lv_obj_t *parent)
{
    g_dro = d;
    col_p[0] = EM(2.4f); col_p[1] = LV_GRID_FR(1); col_p[2] = EM(1.6f); col_p[3] = LV_GRID_TEMPLATE_LAST;
    row_p[0] = LV_GRID_CONTENT; row_p[1] = LV_GRID_CONTENT; row_p[2] = LV_GRID_TEMPLATE_LAST;
    col_l[0] = EM(2.2f); col_l[1] = LV_GRID_FR(1); col_l[2] = LV_GRID_CONTENT; col_l[3] = EM(1.6f);
    col_l[4] = LV_GRID_TEMPLATE_LAST;
    row_l[0] = LV_GRID_CONTENT; row_l[1] = LV_GRID_TEMPLATE_LAST;
    sub_col[0] = LV_GRID_CONTENT; sub_col[1] = LV_GRID_CONTENT; sub_col[2] = LV_GRID_TEMPLATE_LAST;
    sub_row[0] = LV_GRID_CONTENT; sub_row[1] = LV_GRID_CONTENT; sub_row[2] = LV_GRID_TEMPLATE_LAST;

    lv_obj_t *hr;
    d->card = ui_card(parent, "ПОЗИЦИЯ", &hr);
    d->home_btn = ui_btn(hr, "HOME", BTN_NORMAL, true);
    lv_obj_t *gz = ui_btn(hr, "GO TO 0", BTN_NORMAL, true);
    if (!g_portrait) {   /* .homebtn: по-широк разредка; в портрет няма място */
        lv_obj_set_style_text_letter_space(d->home_btn, EM(0.1f), 0);
        lv_obj_set_style_text_letter_space(gz, EM(0.1f), 0);
    } else {
        lv_obj_set_style_pad_gap(hr, EM(0.25f), 0);
    }
    ui_btn(hr, "ЕД. БЛОК", BTN_GHOST, true);
    ui_btn(hr, "M8", BTN_GHOST, true);
    lv_obj_t *seg = ui_box(hr);
    ui_flex(seg, LV_FLEX_FLOW_ROW, 0.25f);
    d->seg_work = ui_btn(seg, "РАБ.", BTN_ON, true);
    d->seg_mach = ui_btn(seg, "МАШ.", BTN_NORMAL, true);
    lv_obj_add_event_cb(d->seg_work, on_coord, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_add_event_cb(d->seg_mach, on_coord, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lv_obj_t *body = ui_box(d->card);
    lv_obj_set_width(body, LV_PCT(100));
    ui_flex(body, LV_FLEX_FLOW_COLUMN, 0);
    for (int a = 0; a < AX_N; a++) row_create(&d->rows[a], body, a);
}

void dro_update(dro_t *d, uint32_t now)
{
    char buf[48], v[24];
    int last = -1;
    for (int a = 0; a < AX_N; a++) {
        dro_row_t *r = &d->rows[a];
        if (!M.active[a]) { lv_obj_set_hidden(r->row, true); continue; }
        lv_obj_set_hidden(r->row, false);
        lv_obj_set_style_border_width(r->row, 1, 0);
        last = a;

        double w = machine_work(a), m = M.mach[a];
        f3(v, sizeof v, M.coord_mach ? m : w, 9);
        ui_set_text(r->val, v);
        f3(v, sizeof v, M.coord_mach ? w : m, 9);
        snprintf(buf, sizeof buf, "%s%s", M.coord_mach ? "РАБ" : "МАШ", v);
        ui_set_text(r->sm, buf);
        f3(v, sizeof v, M.togo[a], 9);
        snprintf(buf, sizeof buf, "ОСТ%s", v);
        ui_set_text(r->sd, buf);

        double e = fabs(M.ferr[a]), lim = M.ferr_lim[a];
        if (M.enc_on[a]) {
            int32_t pm = (int32_t)fmin(1000.0, e / lim * 1000.0);
            lv_bar_set_value(r->fbar, pm, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(r->fbar, e > lim * 0.8 ? C_BAD : e > lim * 0.5 ? C_WARN : C_OK,
                                      LV_PART_INDICATOR);
            snprintf(buf, sizeof buf, "%.1f %s", e, M.rotary[a] ? "″" : "µm");
        } else {
            lv_bar_set_value(r->fbar, 0, LV_ANIM_OFF);
            snprintf(buf, sizeof buf, "без енкодер");
        }
        ui_set_text(r->ftxt, buf);

        lv_color_t dc = M.homed[a] == HOMED_SENSOR ? C_OK : M.homed[a] == HOMED_MANUAL ? C_AMB : C_SURF3;
        lv_obj_set_style_bg_color(r->dot, dc, 0);
    }
    if (last >= 0) lv_obj_set_style_border_width(d->rows[last].row, 0, 0);

    /* HOME мига, докато има нереферирани оси */
    bool need = machine_need_home() && M.state == MS_IDLE;
    static int was = -1;
    if (need != was) { ui_btn_set_kind(d->home_btn, need ? BTN_NEED : BTN_NORMAL); was = need; }
    if (need) lv_obj_set_style_opa(d->home_btn, ui_blink_on(now, 1400) ? LV_OPA_COVER : 140, 0);
}
