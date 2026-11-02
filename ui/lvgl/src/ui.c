/* Основният екран „Работа“: горна лента, позиции, траектория и програма (засега празни). */
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>

#include "ui_style.h"

static topbar_t tb;
static dro_t dro;

#define UPDATE_MS 50

static void update_cb(lv_timer_t *t)
{
    static uint32_t last;
    uint32_t now = lv_tick_get();
    machine_tick(now, last ? now - last : 0);
    last = now;
    topbar_update(&tb, now);
    dro_update(&dro, now);
}

static lv_obj_t *seg(lv_obj_t *parent, const char *const *items, int on)
{
    lv_obj_t *s = ui_box(parent);
    ui_flex(s, LV_FLEX_FLOW_ROW, 0.25f);
    for (int i = 0; items[i]; i++) ui_btn(s, items[i], i == on ? BTN_ON : BTN_NORMAL, true);
    return s;
}

void ui_create(lv_display_t *disp)
{
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    ui_style_init(w, h);
    machine_init();

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

    /* .screens + .run: две колони (63/37, в портрет 57/43) */
    lv_obj_t *run = ui_box(scr);
    lv_obj_set_width(run, LV_PCT(100));
    lv_obj_set_flex_grow(run, 1);
    lv_obj_set_style_pad_all(run, EM(0.6f), 0);
    lv_obj_set_style_pad_gap(run, EM(0.6f), 0);
    static int32_t cols[3], rows[3];
    cols[0] = LV_GRID_FR((g_portrait ? 57 : 63));  /* LV_GRID_FR не слага скоби */
    cols[1] = LV_GRID_FR((g_portrait ? 43 : 37));
    cols[2] = LV_GRID_TEMPLATE_LAST;
    rows[0] = LV_GRID_CONTENT;
    rows[1] = LV_GRID_FR(1);
    rows[2] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_layout(run, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(run, cols, rows);

    dro_create(&dro, run);
    lv_obj_set_grid_cell(dro.card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);

    lv_obj_t *hr;
    lv_obj_t *traj = ui_card(run, "ТРАЕКТОРИЯ", &hr);
    static const char *const views[] = {"3D", "XY", "XZ", "YZ", "ПОБЕРИ", NULL};
    seg(hr, views, 0);
    /* в портрет заглавието се скрива (както в макета), остават само изгледите */
    if (g_portrait) lv_obj_set_hidden(lv_obj_get_child(lv_obj_get_child(traj, 0), 0), true);
    lv_obj_set_grid_cell(traj, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    lv_obj_t *prog = ui_card(run, "—", &hr);
    lv_obj_t *pos = ui_label(hr, F.num_sm, C_DIM, "0 / 0");
    (void)pos;
    ui_btn(hr, "ОТВОРИ", BTN_NORMAL, true);
    lv_obj_set_grid_cell(prog, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 2);

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
