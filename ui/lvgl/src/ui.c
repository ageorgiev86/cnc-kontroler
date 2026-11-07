/* Основният екран „Работа“ (по ui/maket.html):
 *   горна лента
 *   позиции | програма (G-code)
 *   траектория | програма
 *   оверрайди и напредък
 *   бутоните на цикъла
 *   навигация между екраните */
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>

#include "program.h"
#include "ui_style.h"

static topbar_t tb;
static dro_t dro;
static traj_t traj;
static gcode_t gc;
static ctrl_t ctrl;

#define UPDATE_MS 50

static void update_cb(lv_timer_t *t)
{
    static uint32_t last;
    uint32_t now = lv_tick_get();
    machine_tick(now, last ? now - last : 0);
    last = now;
    topbar_update(&tb, now);
    dro_update(&dro, now);
    traj_update(&traj, now);
    gcode_update(&gc, now);
    ctrl_update(&ctrl, now);
}

/* навигация (.navbar): засега само „Работа“; другите екрани са следващите стъпки */
static void on_nav(lv_event_t *e)
{
    const char *n = lv_label_get_text(lv_obj_get_child(lv_event_get_target_obj(e), 0));
    char t[96];
    snprintf(t, sizeof t, "Екранът „%s“ идва в следваща стъпка", n);
    machine_toast(t, lv_tick_get(), 3000);
}

static void navbar_create(lv_obj_t *parent)
{
    lv_obj_t *n = ui_box(parent);
    lv_obj_set_width(n, LV_PCT(100));
    lv_obj_set_style_bg_color(n, C_PANEL, 0);
    lv_obj_set_style_bg_opa(n, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(n, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(n, C_EDGE2, 0);
    lv_obj_set_style_border_width(n, 1, 0);
    lv_obj_set_style_pad_ver(n, EM(0.45f), 0);
    lv_obj_set_style_pad_hor(n, EM(0.6f), 0);
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(n, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(n, cols, rows);
    lv_obj_set_style_pad_column(n, EM(0.35f), 0);
    static const char *const NAV[7] = {"РАБОТА", "РЪЧНО", "MDI", "ИНСТРУМЕНТИ", "ОФСЕТИ", "ДИАГНОСТИКА", "НАСТРОЙКИ"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t *b = ui_btn(n, NAV[i], i == 0 ? BTN_ON : BTN_NORMAL, false);
        lv_obj_set_height(b, EM(3.3f));
        lv_obj_set_style_text_font(b, g_portrait ? F.ui_sm : F.ui_md, 0);
        lv_obj_set_style_text_letter_space(b, EM(g_portrait ? 0.02f : 0.07f), 0);
        lv_obj_set_style_pad_hor(b, EM(0.2f), 0);
        if (i) lv_obj_add_event_cb(b, on_nav, LV_EVENT_CLICKED, NULL);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
}

void ui_create(lv_display_t *disp)
{
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    ui_style_init(w, h);
    machine_init();
    if (!P.nop) program_load_demo();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_style_all(scr);
    lv_obj_set_scrollable(scr, false);
    lv_obj_set_style_bg_color(scr, C_PANEL, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, C_FG, 0);
    lv_obj_set_style_text_font(scr, F.ui_sm, 0);
    ui_flex(scr, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    topbar_create(&tb, scr);

    /* .screens + .run: две колони (63/37, в портрет 57/43), четири реда */
    lv_obj_t *run = ui_box(scr);
    lv_obj_set_width(run, LV_PCT(100));
    lv_obj_set_flex_grow(run, 1);
    lv_obj_set_style_pad_all(run, EM(0.6f), 0);
    lv_obj_set_style_pad_gap(run, EM(0.6f), 0);
    static int32_t cols[3], rows[5];
    cols[0] = LV_GRID_FR((g_portrait ? 57 : 63));  /* LV_GRID_FR не слага скоби */
    cols[1] = LV_GRID_FR((g_portrait ? 43 : 37));
    cols[2] = LV_GRID_TEMPLATE_LAST;
    rows[0] = LV_GRID_CONTENT;
    rows[1] = LV_GRID_FR(1);
    rows[2] = LV_GRID_CONTENT;
    rows[3] = LV_GRID_CONTENT;
    rows[4] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_layout(run, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(run, cols, rows);

    dro_create(&dro, run);
    lv_obj_set_grid_cell(dro.card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);

    traj_create(&traj, run);
    lv_obj_set_grid_cell(traj.card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    gcode_create(&gc, run);
    lv_obj_set_grid_cell(gc.card, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 2);

    lv_obj_t *c = ctrl_create(&ctrl, run);
    lv_obj_set_grid_cell(c, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 2, 1);

    lv_obj_t *cb = cbtns_create(run);
    lv_obj_set_grid_cell(cb, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 3, 1);

    navbar_create(scr);

    update_cb(NULL);
    lv_timer_create(update_cb, UPDATE_MS, NULL);
}

/* 32-битов BMP отгоре надолу; достатъчно за снимки от симулатора */
int ui_save_bmp(const char *path)
{
    lv_draw_buf_t *db = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_XRGB8888);
    if (!db) return -1;
    uint32_t w = db->header.w, h = db->header.h, stride = db->header.stride;
    FILE *f = fopen(path, "wb");
    if (!f) { lv_draw_buf_destroy(db); return -1; }
    uint32_t img = w * h * 4, fsz = 54 + img;
    uint8_t hd[54] = {'B', 'M'};
#define PUT32(o, v) do { uint32_t _v = (uint32_t)(v); hd[o] = _v; hd[o + 1] = _v >> 8; hd[o + 2] = _v >> 16; hd[o + 3] = _v >> 24; } while (0)
    PUT32(2, fsz);
    PUT32(10, 54);
    PUT32(14, 40);
    PUT32(18, w);
    PUT32(22, -(int32_t)h);
    hd[26] = 1;
    hd[28] = 32;
    PUT32(34, img);
#undef PUT32
    fwrite(hd, 1, sizeof hd, f);
    for (uint32_t y = 0; y < h; y++) fwrite(db->data + y * stride, 1, w * 4, f);
    fclose(f);
    lv_draw_buf_destroy(db);
    return 0;
}
