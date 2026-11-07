/* Екранът „Ръчно“ (.jog в макета): работна позиция с нулиране, бутоните за движение,
 * траекторията и параметрите (стъпка, скорост, реферирване). */
#include <stdio.h>

#include "ui.h"
#include "ui_style.h"

static dro_t dro;
static lv_obj_t *hint, *inc[6], *speed, *ref[2], *rot, *rotb[6], *home_all, *home_ax[AX_N];
static double inc_was = -1;
static int ref_was = -1;

static const double INC[6] = {0.001, 0.01, 0.1, 1, 10, 0};
static const int SPEEDS[] = {100, 250, 500, 1000, 2000, 3000, 5000};

/* натиснат бутон: стъпка или начало на непрекъснато движение; пуснат: край */
static void on_jog(lv_event_t *e)
{
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) machine_jog(v / 2, v % 2 ? 1 : -1, lv_tick_get());
    else machine_jog_release();
}

static void on_inc(lv_event_t *e) { M.jog_inc = INC[(int)(intptr_t)lv_event_get_user_data(e)]; }

static void on_speed(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e), n = (int)(sizeof SPEEDS / sizeof SPEEDS[0]), i = 0;
    while (i < n - 1 && SPEEDS[i] < M.jog_f) i++;
    i += d;
    M.jog_f = SPEEDS[i < 0 ? 0 : i >= n ? n - 1 : i];
}

static void on_ref(lv_event_t *e)
{
    M.ref_manual = (bool)(intptr_t)lv_event_get_user_data(e);
    machine_toast(M.ref_manual ? "Ръчен режим: текущата позиция на оста става машинна нула"
                               : "Реферирване с датчик и Z индекс на енкодера", lv_tick_get(), 3500);
}

static void on_home_conf(int i, void *ud) { machine_home(-1, lv_tick_get()); }
static void on_home(lv_event_t *e)
{
    int a = (int)(intptr_t)lv_event_get_user_data(e);
    if (a < 0 && M.ref_manual) {
        if (machine_can_move(lv_tick_get()))
            dlg_confirm("Ръчна референтна точка", "Текущото положение на всички активни оси става машинна нула (маш. 0). "
                        "Осите се маркират като ръчно реферирани.", "ПОТВЪРДИ", on_home_conf, NULL);
        return;
    }
    machine_home(a, lv_tick_get());
}

static lv_obj_t *jog_btn(lv_obj_t *parent, const char *t, int ax, int dir)
{
    lv_obj_t *b = ui_btn(parent, t, BTN_NORMAL, false);
    lv_obj_set_style_text_font(b, F.ui_lg, 0);
    lv_obj_set_style_pad_hor(b, 0, 0);
    void *ud = (void *)(intptr_t)(ax * 2 + (dir > 0));
    lv_obj_add_event_cb(b, on_jog, LV_EVENT_PRESSED, ud);
    lv_obj_add_event_cb(b, on_jog, LV_EVENT_RELEASED, ud);
    lv_obj_add_event_cb(b, on_jog, LV_EVENT_PRESS_LOST, ud);
    return b;
}

static void pad_card(lv_obj_t *scr, lv_obj_t **card)
{
    lv_obj_t *hr;
    lv_obj_t *c = *card = ui_card(scr, "РЪЧНО ДВИЖЕНИЕ", &hr);
    hint = ui_label(hr, F.num_sm, C_DIM, "");
    lv_obj_t *b = ui_body(c, 0.45f);

    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_t *pad = ui_box(b);
    lv_obj_set_width(pad, LV_PCT(100));
    lv_obj_set_flex_grow(pad, 1);
    ui_grid(pad, cols, rows, 0.45f);
    ui_cell(jog_btn(pad, "Y+", AX_Y, 1), 1, 1, 0, 1);
    ui_cell(jog_btn(pad, "Z+", AX_Z, 1), 3, 1, 0, 1);
    ui_cell(jog_btn(pad, "X−", AX_X, -1), 0, 1, 1, 1);
    ui_cell(jog_btn(pad, "X+", AX_X, 1), 2, 1, 1, 1);
    ui_cell(jog_btn(pad, "Y−", AX_Y, -1), 1, 1, 2, 1);
    ui_cell(jog_btn(pad, "Z−", AX_Z, -1), 3, 1, 2, 1);
    static const char *const LBL[2] = {"XY", "Z"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *l = ui_label(pad, F.ui_md, C_FAINT, LBL[i]);
        lv_obj_set_style_text_letter_space(l, EM(0.1f), 0);
        lv_obj_set_grid_cell(l, LV_GRID_ALIGN_CENTER, i ? 3 : 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    }

    rot = ui_box(b);
    lv_obj_set_size(rot, LV_PCT(100), EM(4.2f));
    ui_flex(rot, LV_FLEX_FLOW_ROW, 0.45f);
    static const char *const R[6] = {"A−", "A+", "B−", "B+", "C−", "C+"};
    for (int i = 0; i < 6; i++) {
        rotb[i] = jog_btn(rot, R[i], AX_A + i / 2, i % 2 ? 1 : -1);
        lv_obj_set_height(rotb[i], LV_PCT(100));
        lv_obj_set_flex_grow(rotb[i], 1);
    }
}

static void params_card(lv_obj_t *scr, lv_obj_t **card)
{
    lv_obj_t *c = *card = ui_card(scr, "ПАРАМЕТРИ", NULL);
    lv_obj_t *b = ui_body(c, 0.55f);
    /* в портрет: надпис вляво, контрола вдясно */
    static int32_t pcols[3], prows[5];
    if (g_portrait) {
        pcols[0] = LV_GRID_CONTENT; pcols[1] = LV_GRID_FR(1); pcols[2] = LV_GRID_TEMPLATE_LAST;
        prows[0] = prows[1] = prows[2] = prows[3] = LV_GRID_CONTENT; prows[4] = LV_GRID_TEMPLATE_LAST;
        ui_grid(b, pcols, prows, 0.55f);
        lv_obj_set_flex_grow(b, 0);
        lv_obj_set_height(b, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_column(b, EM(1.2f), 0);
    }
    int row = 0;
#define PLACE(lab, ctl) do { if (g_portrait) { lv_obj_set_grid_cell(lab, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1); \
    ui_cell(ctl, 1, 1, row, 1); row++; } else lv_obj_set_width(ctl, LV_PCT(100)); } while (0)

    lv_obj_t *l = ui_lab(b, "СТЪПКА, ММ");
    static int32_t c3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t r2[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *g = ui_box(b);
    ui_grid(g, c3, r2, 0.35f);
    static const char *const IT[6] = {"0.001", "0.01", "0.1", "1", "10", "НЕПРЕК."};
    for (int i = 0; i < 6; i++) {
        inc[i] = ui_btn(g, IT[i], BTN_NORMAL, true);
        lv_obj_add_event_cb(inc[i], on_inc, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ui_cell(inc[i], i % 3, 1, i / 3, 1);
    }
    PLACE(l, g);

    l = ui_lab(b, "СКОРОСТ, ММ/МИН");
    lv_obj_t *st = ui_box(b);
    static int32_t sc[4], sr[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    sc[0] = EM(3); sc[1] = LV_GRID_FR(1); sc[2] = EM(3); sc[3] = LV_GRID_TEMPLATE_LAST;
    ui_grid(st, sc, sr, 0.3f);
    lv_obj_t *m = ui_btn(st, "−", BTN_NORMAL, false);
    lv_obj_set_height(m, EM(2.5f));
    lv_obj_add_event_cb(m, on_speed, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    ui_cell(m, 0, 1, 0, 1);
    speed = ui_label(st, F.num_pg, C_FG, "");
    lv_obj_set_grid_cell(speed, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_t *p = ui_btn(st, "+", BTN_NORMAL, false);
    lv_obj_set_height(p, EM(2.5f));
    lv_obj_add_event_cb(p, on_speed, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    ui_cell(p, 2, 1, 0, 1);
    PLACE(l, st);

    l = ui_lab(b, "РЕФЕРИРВАНЕ");
    lv_obj_t *seg = ui_box(b);
    ui_flex(seg, LV_FLEX_FLOW_ROW, 0.25f);
    ref[0] = ui_btn(seg, "С ДАТЧИК", BTN_ON, true);
    ref[1] = ui_btn(seg, "РЪЧНО, ТУК", BTN_NORMAL, true);
    for (int i = 0; i < 2; i++) {
        lv_obj_set_flex_grow(ref[i], 1);
        lv_obj_add_event_cb(ref[i], on_ref, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    PLACE(l, seg);

    static int32_t rc[] = {LV_GRID_FR(10), LV_GRID_FR(10), LV_GRID_FR(10), LV_GRID_FR(15), LV_GRID_TEMPLATE_LAST};
    lv_obj_t *rg = ui_box(b);
    ui_grid(rg, rc, r2, 0.35f);
    for (int a = 0; a < AX_N; a++) {
        home_ax[a] = ui_btn(rg, AX_NAME[a], BTN_NORMAL, true);
        lv_obj_add_event_cb(home_ax[a], on_home, LV_EVENT_CLICKED, (void *)(intptr_t)a);
        ui_cell(home_ax[a], a % 3, 1, a / 3, 1);
    }
    home_all = ui_btn(rg, "ВСИЧКИ\nОСИ", BTN_NORMAL, true);
    lv_obj_set_style_text_align(ui_btn_label(home_all), LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_height(home_all, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(home_all, on_home, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    ui_cell(home_all, 3, 1, 0, 2);
    if (g_portrait) ui_cell(rg, 1, 1, row, 1);
    else lv_obj_set_width(rg, LV_PCT(100));
#undef PLACE
}

void jog_create(lv_obj_t *scr)
{
    static int32_t cols[4], rows[4];
    if (g_portrait) {
        cols[0] = LV_GRID_FR(1); cols[1] = LV_GRID_FR(1); cols[2] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_CONTENT; rows[1] = LV_GRID_FR(1); rows[2] = LV_GRID_CONTENT; rows[3] = LV_GRID_TEMPLATE_LAST;
    } else {
        cols[0] = LV_GRID_FR(33); cols[1] = LV_GRID_FR(42); cols[2] = LV_GRID_FR(25); cols[3] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_CONTENT; rows[1] = LV_GRID_FR(1); rows[2] = LV_GRID_TEMPLATE_LAST;
    }
    ui_grid(scr, cols, rows, 0.6f);
    dro_create(&dro, scr, DRO_ZERO);
    lv_obj_set_grid_cell(dro.card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_t *pc, *par;
    pad_card(scr, &pc);
    ui_cell(pc, 0, 1, 1, 1);
    params_card(scr, &par);
    if (g_portrait) {
        ui_cell(par, 0, 2, 2, 1);
        ui_traj_slot(SCR_JOG, 1, 1, 0, 2);
    } else {
        ui_cell(par, 2, 1, 0, 2);
        ui_traj_slot(SCR_JOG, 1, 1, 0, 2);
    }
}

void jog_update(uint32_t now)
{
    dro_update(&dro, now);
    if (M.jog_inc != inc_was) {
        int on = 5;
        for (int i = 0; i < 6; i++)
            if (INC[i] == M.jog_inc) on = i;
        ui_seg_set(inc, 6, on);
        char t[80];
        if (M.jog_inc > 0) snprintf(t, sizeof t, "стъпка %.3f мм", M.jog_inc);
        else snprintf(t, sizeof t, "непрекъснато · задръж бутона");
        ui_set_text(hint, t);
        inc_was = M.jog_inc;
    }
    char t[16];
    snprintf(t, sizeof t, "%d", M.jog_f);
    ui_set_text(speed, t);
    if (M.ref_manual != ref_was) {
        ui_seg_set(ref, 2, M.ref_manual);
        ui_set_text(ui_btn_label(home_all), M.ref_manual ? "ВСИЧКИ\nОСИ ТУК" : "ВСИЧКИ\nОСИ");
        ref_was = M.ref_manual;
    }
    bool any = false;
    for (int i = 0; i < 6; i++) {
        bool on = M.active[AX_A + i / 2];
        lv_obj_set_hidden(rotb[i], !on);
        any |= on;
    }
    lv_obj_set_hidden(rot, !any);
    for (int a = 0; a < AX_N; a++) lv_obj_set_style_opa(home_ax[a], M.active[a] ? LV_OPA_COVER : 90, 0);
}
