/* Екранът „MDI“ (.mdi в макета): работна позиция, команда с история, траектория и клавиатура. */
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "ui_style.h"

#define HIST_MAX 12

static dro_t dro;
static lv_obj_t *in, *hist_box;
static char cmd[256];
static char hist[HIST_MAX][128];
static int nhist;
static bool dirty = true;

static void on_hist(lv_event_t *e);

static void render(void)
{
    char t[300];
    /* редовете се показват както са, курсорът е „▌“ от шрифта; без recolor */
    snprintf(t, sizeof t, "%s_", cmd);
    ui_set_text(in, t);
    lv_obj_clean(hist_box);
    for (int i = 0; i < nhist; i++) {
        char h[160];
        size_t o = 0;
        for (const char *p = hist[i]; *p && o + 4 < sizeof h; p++) {
            if (*p == '\n') { memcpy(h + o, " / ", 3); o += 3; }
            else h[o++] = *p;
        }
        h[o] = 0;
        lv_obj_t *b = ui_btn(hist_box, h, BTN_NORMAL, false);
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_height(b, EM(2.6f));
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_text_font(b, F.num_md, 0);
        lv_obj_set_style_text_letter_space(b, 0, 0);
        lv_obj_set_user_data(b, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, on_hist, LV_EVENT_CLICKED, NULL);
    }
    dirty = false;
}

static void on_hist(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target_obj(e));
    snprintf(cmd, sizeof cmd, "%s", hist[i]);
    dirty = true;
}

static void add_hist(const char *c)
{
    int k = 0;
    while (k < nhist && strcmp(hist[k], c)) k++;
    if (k == nhist && nhist < HIST_MAX) nhist++;
    if (k >= nhist) k = nhist - 1;
    memmove(hist[1], hist[0], (size_t)k * sizeof hist[0]);
    snprintf(hist[0], sizeof hist[0], "%s", c);
}

static void on_key(lv_event_t *e)
{
    const char *k = lv_label_get_text(ui_btn_label(lv_event_get_target_obj(e)));
    size_t n = strlen(cmd);
    if (n + 2 < sizeof cmd) { cmd[n] = k[0]; cmd[n + 1] = 0; }
    dirty = true;
}

static void on_fn(lv_event_t *e)
{
    int f = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(cmd);
    switch (f) {
    case 0: cmd[0] = 0; break;                                                 /* Изчисти */
    case 1: if (n && cmd[n - 1] != '\n' && n + 2 < sizeof cmd) strcat(cmd, "\n"); break;   /* Нов ред */
    case 2:                                                                    /* Изпълни */
        while (n && (cmd[n - 1] == '\n' || cmd[n - 1] == ' ')) cmd[--n] = 0;
        if (machine_mdi(cmd, lv_tick_get()) == 0) { add_hist(cmd); cmd[0] = 0; }
        break;
    case 3: if (n + 2 < sizeof cmd) strcat(cmd, " "); break;                    /* Интервал */
    case 4: if (n) cmd[n - 1] = 0; break;                                      /* Изтрий */
    }
    dirty = true;
}

static void cmd_card(lv_obj_t *scr, lv_obj_t **card)
{
    lv_obj_t *c = *card = ui_card(scr, "КОМАНДА", NULL);
    lv_obj_t *b = ui_body(c, 0.55f);
    lv_obj_t *box = ui_box(b);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_style_min_height(box, EM(4.8f), 0);
    lv_obj_set_style_bg_color(box, C_PANEL, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, C_LINE, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, EM(0.35f), 0);
    lv_obj_set_style_pad_hor(box, EM(0.6f), 0);
    lv_obj_set_style_pad_ver(box, EM(0.45f), 0);
    in = ui_label(box, F.num_pg, C_FG, "");
    lv_obj_set_width(in, LV_PCT(100));
    lv_label_set_long_mode(in, LV_LABEL_LONG_MODE_WRAP);

    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *g = ui_box(b);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, cols, rows, 0.35f);
    static const char *const T[3] = {"ИЗЧИСТИ", "НОВ РЕД", "ИЗПЪЛНИ"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *bt = ui_btn(g, T[i], i == 2 ? BTN_GO : BTN_NORMAL, false);
        lv_obj_add_event_cb(bt, on_fn, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ui_cell(bt, i, 1, 0, 1);
    }
    ui_lab(b, "ИСТОРИЯ");
    hist_box = ui_box(b);
    lv_obj_set_width(hist_box, LV_PCT(100));
    lv_obj_set_flex_grow(hist_box, 1);
    if (g_portrait) lv_obj_set_style_max_height(hist_box, EM(11), 0);
    lv_obj_set_scrollable(hist_box, true);
    lv_obj_set_scroll_dir(hist_box, LV_DIR_VER);
    ui_flex(hist_box, LV_FLEX_FLOW_COLUMN, 0.3f);
}

static void keys_card(lv_obj_t *scr, lv_obj_t **card)
{
    lv_obj_t *c = *card = ui_card(scr, "КЛАВИАТУРА", NULL);
    lv_obj_t *b = ui_body(c, 0);
    if (g_portrait) { lv_obj_set_flex_grow(b, 0); lv_obj_set_height(b, LV_SIZE_CONTENT); }
    static int32_t cols[7], rows[7];
    for (int i = 0; i < 6; i++) cols[i] = LV_GRID_FR(1);
    cols[6] = LV_GRID_TEMPLATE_LAST;
    for (int i = 0; i < 6; i++) rows[i] = g_portrait ? EM(2.8f) : LV_GRID_FR(1);
    rows[6] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(b);
    lv_obj_set_width(g, LV_PCT(100));
    if (g_portrait) lv_obj_set_height(g, LV_SIZE_CONTENT);
    else lv_obj_set_flex_grow(g, 1);
    ui_grid(g, cols, rows, 0.3f);
    static const char KEYS[] = "GMXYZFSTABCPQRIJKH789456123-0.";
    for (int i = 0; KEYS[i]; i++) {
        char t[2] = {KEYS[i], 0};
        lv_obj_t *k = ui_btn(g, t, BTN_NORMAL, false);
        lv_obj_set_style_pad_hor(k, 0, 0);
        lv_obj_set_style_text_font(k, F.num_pg, 0);
        lv_obj_add_event_cb(k, on_key, LV_EVENT_CLICKED, NULL);
        ui_cell(k, i % 6, 1, i / 6, 1);
    }
    lv_obj_t *sp = ui_btn(g, "ИНТЕРВАЛ", BTN_NORMAL, false);
    lv_obj_add_event_cb(sp, on_fn, LV_EVENT_CLICKED, (void *)(intptr_t)3);
    ui_cell(sp, 0, 3, 5, 1);
    lv_obj_t *del = ui_btn(g, "ИЗТРИЙ", BTN_NORMAL, false);
    lv_obj_add_event_cb(del, on_fn, LV_EVENT_CLICKED, (void *)(intptr_t)4);
    ui_cell(del, 3, 3, 5, 1);
}

void mdi_create(lv_obj_t *scr)
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
    dro_create(&dro, scr, DRO_COMPACT);
    ui_cell(dro.card, 0, 1, 0, 1);
    lv_obj_t *cc, *kc;
    cmd_card(scr, &cc);
    ui_cell(cc, 0, 1, 1, 1);
    keys_card(scr, &kc);
    if (g_portrait) ui_cell(kc, 0, 2, 2, 1);
    else ui_cell(kc, 2, 1, 0, 2);
    ui_traj_slot(SCR_MDI, 1, 1, 0, 2);

    static const char *const H0[] = {"G0 X0 Y0 Z5", "G1 Z-1 F200", "M3 S8000", "T3 M6"};
    for (int i = 3; i >= 0; i--) add_hist(H0[i]);
}

void mdi_update(uint32_t now)
{
    dro_update(&dro, now);
    if (dirty) render();
}
