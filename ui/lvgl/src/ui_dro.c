/* Картата „Позиция“ (.dro в макета): буква на оста с точка за Home, голямата стойност,
 * мерна единица и под нея машинна/работна стойност, остатък и следяща грешка. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "ui_style.h"

static void on_coord(lv_event_t *e)
{
    dro_t *d = lv_event_get_user_data(e);
    M.coord_mach = lv_event_get_target_obj(e) == d->seg_mach;
    ui_btn_set_kind(d->seg_work, M.coord_mach ? BTN_NORMAL : BTN_ON);
    ui_btn_set_kind(d->seg_mach, M.coord_mach ? BTN_ON : BTN_NORMAL);
}

static void on_home(lv_event_t *e) { machine_home(-1, lv_tick_get()); }
static void on_home_conf(int i, void *ud) { machine_home(-1, lv_tick_get()); }
static void on_home_btn(lv_event_t *e)
{
    if (M.ref_manual && machine_can_move(lv_tick_get()))
        dlg_confirm("Ръчна референтна точка", "Текущото положение на всички активни оси става машинна нула (маш. 0). "
                    "Осите се маркират като ръчно реферирани.", "ПОТВЪРДИ", on_home_conf, NULL);
    else on_home(e);
}
static void on_gozero(lv_event_t *e) { machine_goto_zero(lv_tick_get()); }
static void on_zero_all(lv_event_t *e) { machine_zero(-1, lv_tick_get()); }
static void on_zero(lv_event_t *e) { machine_zero((int)(intptr_t)lv_event_get_user_data(e), lv_tick_get()); }
static void on_setv(lv_event_t *e) { dlg_setpos((int)(intptr_t)lv_event_get_user_data(e)); }
static void on_single(lv_event_t *e) { M.single = !M.single; }
static void on_cool(lv_event_t *e) { machine_coolant(lv_tick_get()); }

/* f3 от макета: 3 знака след точката, без „-0.000“, подравнено вдясно до w знака */
static void f3(char *out, size_t n, double v, int w)
{
    if (fabs(v) < 0.0005) v = 0;
    snprintf(out, n, "%*.3f", w, v);
}

/* Колоните са статични, защото LVGL пази указател към тях */
static int32_t col_p[4], row_p[3], col_l[5], row_l[2], sub_col[3], sub_row[3], col_c[5], row_c[2];

/* компактен ред (Ръчно и MDI): буква, стойност, мярка и при „zero“ бутоните „X = 0“ и „Задай“ */
static void row_compact(dro_row_t *r, lv_obj_t *parent, int a, bool zero)
{
    lv_obj_t *row = r->row = ui_box(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, C_LINE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_ver(row, EM(0.15f), 0);
    lv_obj_set_style_pad_left(row, EM(0.75f), 0);
    lv_obj_set_style_pad_right(row, EM(0.6f), 0);
    lv_obj_set_style_pad_column(row, EM(0.5f), 0);
    lv_obj_set_layout(row, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(row, col_c, row_c);
    lv_obj_set_style_min_height(row, EM(2.6f), 0);

    lv_obj_t *ax = ui_box(row);
    lv_obj_set_overflow_visible(ax, true);
    ui_label(ax, F.ui_lg, C_HI, AX_NAME[a]);
    lv_obj_t *dot = r->dot = ui_box(ax);
    lv_obj_set_ignore_layout(dot, true);
    lv_obj_set_size(dot, EM(0.34f), EM(0.34f));
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, C_SURF3, 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_RIGHT, -EM(0.1f), 0);
    lv_obj_set_grid_cell(ax, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    r->val = ui_label(row, F.num_ov, C_FG, "0.000");
    lv_obj_set_grid_cell(r->val, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    r->unit = ui_label(row, F.ui_xs, C_DIM, M.rotary[a] ? "°" : "мм");
    lv_obj_set_grid_cell(r->unit, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    if (zero) {
        lv_obj_t *zb = ui_box(row);
        ui_flex(zb, LV_FLEX_FLOW_ROW, 0.3f);
        char t[16];
        snprintf(t, sizeof t, "%s = 0", AX_NAME[a]);
        lv_obj_add_event_cb(ui_btn(zb, t, BTN_NORMAL, true), on_zero, LV_EVENT_CLICKED, (void *)(intptr_t)a);
        lv_obj_add_event_cb(ui_btn(zb, "ЗАДАЙ", BTN_NORMAL, true), on_setv, LV_EVENT_CLICKED, (void *)(intptr_t)a);
        lv_obj_set_grid_cell(zb, LV_GRID_ALIGN_END, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
}

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

    /* редът отваря „Задай позиция“ (data-act="setv") */
    lv_obj_set_clickable(row, true);
    lv_obj_set_style_bg_color(row, C_SURF2, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, on_setv, LV_EVENT_CLICKED, (void *)(intptr_t)a);

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

void dro_create(dro_t *d, lv_obj_t *parent, dro_kind_t kind)
{
    memset(d, 0, sizeof *d);
    d->kind = kind;
    d->need_was = d->single_was = d->cool_was = d->wcs_was = -1;
    col_p[0] = EM(2.4f); col_p[1] = LV_GRID_FR(1); col_p[2] = EM(1.6f); col_p[3] = LV_GRID_TEMPLATE_LAST;
    row_p[0] = LV_GRID_CONTENT; row_p[1] = LV_GRID_CONTENT; row_p[2] = LV_GRID_TEMPLATE_LAST;
    col_l[0] = EM(2.2f); col_l[1] = LV_GRID_FR(1); col_l[2] = LV_GRID_CONTENT; col_l[3] = EM(1.6f);
    col_l[4] = LV_GRID_TEMPLATE_LAST;
    row_l[0] = LV_GRID_CONTENT; row_l[1] = LV_GRID_TEMPLATE_LAST;
    sub_col[0] = LV_GRID_CONTENT; sub_col[1] = LV_GRID_CONTENT; sub_col[2] = LV_GRID_TEMPLATE_LAST;
    sub_row[0] = LV_GRID_CONTENT; sub_row[1] = LV_GRID_CONTENT; sub_row[2] = LV_GRID_TEMPLATE_LAST;
    col_c[0] = EM(2); col_c[1] = LV_GRID_FR(1); col_c[2] = EM(1.4f); col_c[3] = LV_GRID_CONTENT; col_c[4] = LV_GRID_TEMPLATE_LAST;
    row_c[0] = LV_GRID_CONTENT; row_c[1] = LV_GRID_TEMPLATE_LAST;

    lv_obj_t *hr;
    d->card = ui_card(parent, kind == DRO_FULL ? "ПОЗИЦИЯ" : "РАБОТНА ПОЗИЦИЯ", &hr);
    d->title = lv_obj_get_child(lv_obj_get_child(d->card, 0), 0);
    if (kind == DRO_FULL) {
        d->home_btn = ui_btn(hr, "HOME", BTN_NORMAL, true);
        lv_obj_add_event_cb(d->home_btn, on_home_btn, LV_EVENT_CLICKED, NULL);
        lv_obj_t *gz = ui_btn(hr, "GO TO 0", BTN_NORMAL, true);
        lv_obj_add_event_cb(gz, on_gozero, LV_EVENT_CLICKED, NULL);
        if (!g_portrait) {   /* .homebtn: по-широк разредка; в портрет няма място */
            lv_obj_set_style_text_letter_space(d->home_btn, EM(0.1f), 0);
            lv_obj_set_style_text_letter_space(gz, EM(0.1f), 0);
        } else {
            lv_obj_set_style_pad_gap(hr, EM(0.25f), 0);
        }
        d->single = ui_btn(hr, "ЕД. БЛОК", BTN_GHOST, true);
        lv_obj_add_event_cb(d->single, on_single, LV_EVENT_CLICKED, NULL);
        d->cool = ui_btn(hr, "M8", BTN_GHOST, true);
        lv_obj_add_event_cb(d->cool, on_cool, LV_EVENT_CLICKED, NULL);
        lv_obj_t *seg = ui_box(hr);
        ui_flex(seg, LV_FLEX_FLOW_ROW, 0.25f);
        d->seg_work = ui_btn(seg, "РАБ.", BTN_ON, true);
        d->seg_mach = ui_btn(seg, "МАШ.", BTN_NORMAL, true);
        lv_obj_add_event_cb(d->seg_work, on_coord, LV_EVENT_CLICKED, d);
        lv_obj_add_event_cb(d->seg_mach, on_coord, LV_EVENT_CLICKED, d);
    } else if (kind == DRO_ZERO) {
        lv_obj_add_event_cb(ui_btn(hr, "GO TO 0", BTN_NORMAL, true), on_gozero, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_btn(hr, "XYZ = 0", BTN_NORMAL, true), on_zero_all, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *body = ui_box(d->card);
    lv_obj_set_width(body, LV_PCT(100));
    ui_flex(body, LV_FLEX_FLOW_COLUMN, 0);
    for (int a = 0; a < AX_N; a++) {
        if (kind == DRO_FULL) row_create(&d->rows[a], body, a);
        else row_compact(&d->rows[a], body, a, kind == DRO_ZERO);
    }
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
        bool mach = d->kind == DRO_FULL && M.coord_mach;
        f3(v, sizeof v, mach ? m : w, d->kind == DRO_FULL ? 9 : 0);
        ui_set_text(r->val, v);
        lv_color_t dc = M.homed[a] == HOMED_SENSOR ? C_OK : M.homed[a] == HOMED_MANUAL ? C_AMB : C_SURF3;
        lv_obj_set_style_bg_color(r->dot, dc, 0);
        if (d->kind != DRO_FULL) continue;
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
    }
    if (last >= 0) lv_obj_set_style_border_width(d->rows[last].row, 0, 0);

    if (d->kind == DRO_ZERO && M.wcs != d->wcs_was) {
        snprintf(buf, sizeof buf, "РАБОТНА ПОЗИЦИЯ · G%d", 53 + M.wcs);
        ui_set_text(d->title, buf);
        d->wcs_was = M.wcs;
    }
    if (d->kind != DRO_FULL) return;
    if (M.single != d->single_was) { ui_btn_set_kind(d->single, M.single ? BTN_ON : BTN_GHOST); d->single_was = M.single; }
    if (M.coolant != d->cool_was) { ui_btn_set_kind(d->cool, M.coolant ? BTN_ON : BTN_GHOST); d->cool_was = M.coolant; }
    ui_set_text(ui_btn_label(d->home_btn), M.ref_manual ? "HOME ТУК" : "HOME");

    /* HOME мига, докато има нереферирани оси */
    bool need = machine_need_home() && M.state == MS_IDLE;
    if (need != d->need_was) { ui_btn_set_kind(d->home_btn, need ? BTN_NEED : BTN_NORMAL); d->need_was = need; }
    if (need) lv_obj_set_style_opa(d->home_btn, ui_blink_on(now, 1400) ? LV_OPA_COVER : 140, 0);
}
