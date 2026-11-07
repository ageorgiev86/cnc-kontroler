/* Диалогът „Карта на височините“ (#hmDlg в макета) и цветната карта в „Резултат“.
 * Диалогът е отделен слой над екрана, а числата се въвеждат с dlg_num отгоре му. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "program.h"
#include "ui.h"
#include "ui_style.h"

static lv_obj_t *modal, *info, *vals[16], *apply_btn;

typedef struct { const char *n, *u, *h; double *v; int d; double min, max; } hp_t;
static const hp_t HP[] = {
    {"Мин X", "работни", "Левият край на областта.", &HM.x0, 3, -9999, 99999},
    {"Макс X", "работни", "Десният край. „По програмата“ взима областта от заредената програма.", &HM.x1, 3, -9999, 99999},
    {"Мин Y", "работни", "Предният край.", &HM.y0, 3, -9999, 99999},
    {"Макс Y", "работни", "Задният край.", &HM.y1, 3, -9999, 99999},
    {"Мин Z", "работни", "До тази височина сондата слиза. Ако няма допир дотук, сканирането спира с аларма.", &HM.z0, 3, -9999, 99999},
    {"Макс Z", "работни", "Височината на старта и горната граница на вдигането. Текущата Z трябва да е между Мин Z и Макс Z.", &HM.z1, 3, -9999, 99999},
    {"Резолюция", "мм", "Стъпката на мрежата по X и Y. Последната точка е точно на Макс. По-малка стъпка = по-точна карта, но повече точки.", &HM.res, 3, 0.001, 99999},
    {"Z повърхност", "работни", "Височината, спрямо която се смята корекцията: Z в програмата се променя с „карта − Z повърхност“. Обикновено 0.", &HM.zs, 3, -9999, 99999},
    {"Z отдръпване", "% от Макс Z − Мин Z", "Колко се вдига сондата над допира, преди да отиде до следващата точка. 100 % = до Макс Z.", &HM.ret, 0, 1, 100},
    {"Скорост на сондата", "мм/мин", "Скоростта на спускане до допир. Бавно, за да не се счупи върхът на гравьора.", &HM.f, 0, 1, 99999},
    {"Скорост между точките", "мм/мин", "Скоростта на ходовете между точките и на вдигането.", &HM.scan, 0, 1, 99999},
};
#define HP_N (int)(sizeof HP / sizeof HP[0])

void hmap_label(char *b, size_t n)
{
    if (HM.done) snprintf(b, n, "има · %d×%d", HM.nx, HM.ny);
    else snprintf(b, n, "стъпка %g мм", HM.res);
}

static void render(void)
{
    if (!modal) return;
    char b[120];
    int nx = (int)ceil((HM.x1 - HM.x0) / (HM.res > 0 ? HM.res : 1) - 1e-9) + 1, ny = (int)ceil((HM.y1 - HM.y0) / (HM.res > 0 ? HM.res : 1) - 1e-9) + 1;
    snprintf(b, sizeof b, "мрежа %d×%d · %d точки · G%d", nx, ny, nx * ny, 53 + M.wcs);
    ui_set_text(info, b);
    for (int i = 0; i < HP_N; i++) {
        snprintf(b, sizeof b, "%.*f", HP[i].d, *HP[i].v);
        ui_set_text(ui_btn_label(vals[i]), b);
    }
    ui_btn_set_kind(apply_btn, HM.apply ? BTN_ON : BTN_NORMAL);
    ui_set_text(ui_btn_label(apply_btn), HM.apply ? "ВКЛЮЧЕНА" : "ИЗКЛЮЧЕНА");
}

static void on_val_set(double v, void *ud)
{
    *HP[(int)(intptr_t)ud].v = v;
    render();
}

static void on_val(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    dlg_num(HP[i].n, HP[i].u, HP[i].h, *HP[i].v, HP[i].min, HP[i].max, HP[i].d, on_val_set, (void *)(intptr_t)i);
}

static void close_dlg(void) { if (modal) lv_obj_delete_async(modal); modal = NULL; }
static void on_close(lv_event_t *e) { close_dlg(); }
static void on_apply(lv_event_t *e) { HM.apply = !HM.apply; render(); }
static void on_prog(lv_event_t *e)
{
    if (!P.nop) { machine_toast("Няма заредена програма", lv_tick_get(), 3000); return; }
    HM.x0 = P.x0; HM.x1 = P.x1; HM.y0 = P.y0; HM.y1 = P.y1;
    render();
}
static void on_clear(lv_event_t *e) { hmap_grid(); M.res.seq++; machine_toast("Картата е изчистена", lv_tick_get(), 3000); }
static void on_scan(lv_event_t *e)
{
    if (HM.x1 <= HM.x0 || HM.y1 <= HM.y0 || HM.z1 <= HM.z0) { machine_toast("Макс трябва да е по-голямо от Мин", lv_tick_get(), 3500); return; }
    close_dlg();
    hmap_scan(lv_tick_get());
}

void hmap_open(void)
{
    close_dlg();
    lv_obj_t *m = modal = ui_box(lv_layer_top());
    lv_obj_set_size(m, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m, lv_color_hex(0x05080a), 0);
    lv_obj_set_style_bg_opa(m, 189, 0);
    lv_obj_set_clickable(m, true);
    ui_flex(m, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(m, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *d = ui_box(m);
    lv_obj_set_width(d, EM(40));
    lv_obj_set_style_max_width(d, LV_PCT(94), 0);
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
    ui_label(h, F.ui_md, C_FG, "Карта на височините");
    info = ui_label(h, F.num_sm, C_DIM, "");

    /* параметрите в две колони: име (с мярка) и бутон със стойността */
    static int32_t cols[5], rows[8];
    cols[0] = LV_GRID_FR(13); cols[1] = LV_GRID_FR(9); cols[2] = LV_GRID_FR(13); cols[3] = LV_GRID_FR(9); cols[4] = LV_GRID_TEMPLATE_LAST;
    for (int i = 0; i < 7; i++) rows[i] = LV_GRID_CONTENT;
    rows[7] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(d);
    lv_obj_set_width(g, LV_PCT(100));
    ui_grid(g, cols, rows, 0.3f);
    lv_obj_set_style_pad_column(g, EM(0.6f), 0);
    for (int i = 0; i <= HP_N; i++) {
        int c = (i % 2) * 2, r = i / 2;
        lv_obj_t *nb = ui_box(g);
        ui_flex(nb, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_flex_align(nb, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_grid_cell(nb, LV_GRID_ALIGN_STRETCH, c, 1, LV_GRID_ALIGN_CENTER, r, 1);
        lv_obj_t *b;
        if (i == HP_N) {   /* последната клетка: прилагане към G-code */
            ui_label(nb, F.txt, C_FG, "Приложи към G-code");
            ui_label(nb, F.txt_sm, C_DIM, "корекция на Z");
            b = apply_btn = ui_btn(g, "", BTN_NORMAL, false);
            lv_obj_add_event_cb(b, on_apply, LV_EVENT_CLICKED, NULL);
        } else {
            ui_label(nb, F.txt, C_FG, HP[i].n);
            ui_label(nb, F.txt_sm, C_DIM, HP[i].u);
            b = vals[i] = ui_btn(g, "", BTN_NORMAL, false);
            lv_obj_set_style_text_font(b, F.num_md, 0);
            lv_obj_set_style_text_letter_space(b, 0, 0);
            lv_obj_add_event_cb(b, on_val, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, c + 1, 1, LV_GRID_ALIGN_CENTER, r, 1);
    }
    lv_obj_t *n = ui_label(d, F.txt_sm, C_DIM, "Сондата (гравьор или пробник) се води до платката, Z между Мин Z и Макс Z. Обхождането е колона по колона по X, "
                                                 "змиевидно по Y. Картата важи за активната нула.");
    lv_obj_set_width(n, LV_PCT(100));
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_WRAP);

    static int32_t c4[] = {LV_GRID_FR(10), LV_GRID_FR(10), LV_GRID_FR(10), LV_GRID_FR(16), LV_GRID_TEMPLATE_LAST}, r1[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *f = ui_box(d);
    lv_obj_set_width(f, LV_PCT(100));
    ui_grid(f, c4, r1, 0.35f);
    static const char *const T[4] = {"ПО ПРОГРАМАТА", "ИЗЧИСТИ", "ЗАТВОРИ", "СКАНИРАЙ ПОВЪРХНОСТТА"};
    static const lv_event_cb_t CB[4] = {on_prog, on_clear, on_close, on_scan};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = ui_btn(f, T[i], i == 3 ? BTN_GO : BTN_NORMAL, i < 2);
        lv_obj_add_event_cb(b, CB[i], LV_EVENT_CLICKED, NULL);
        ui_cell(b, i, 1, 0, 1);
    }
    render();
}

/* цветна карта в „Резултат“: червено (ниско) → зелено (високо), както hmCol в макета */
static void on_map_draw(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target_obj(e);
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t co;
    lv_obj_get_coords(o, &co);
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < HM.nx; i++)
        for (int j = 0; j < HM.ny; j++)
            if (!isnan(HM.z[i][j])) { lo = fminf(lo, HM.z[i][j]); hi = fmaxf(hi, HM.z[i][j]); }
    int32_t W = lv_area_get_width(&co), H = lv_area_get_height(&co);
    float cw = (float)W / HM.nx, ch = (float)H / HM.ny;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    for (int i = 0; i < HM.nx; i++)
        for (int j = 0; j < HM.ny; j++) {
            float z = HM.z[i][j];
            if (isnan(z)) d.bg_color = C_SURF2;
            else {
                float t = hi > lo ? (z - lo) / (hi - lo) : 0.5f;
                d.bg_color = lv_color_make((uint8_t)(236 + (61 - 236) * t), (uint8_t)(79 + (187 - 79) * t), (uint8_t)(84 + (126 - 84) * t));
            }
            /* Y нагоре, както на машината */
            lv_area_t a = {co.x1 + (int32_t)(i * cw), co.y1 + (int32_t)((HM.ny - 1 - j) * ch), co.x1 + (int32_t)((i + 1) * cw) - 1, co.y1 + (int32_t)((HM.ny - j) * ch) - 1};
            lv_draw_rect(l, &d, &a);
        }
}

static lv_obj_t *map_obj;
void hmap_result(lv_obj_t *parent)
{
    map_obj = NULL;
    if (!HM.got || strncmp(M.res.title, "Карта", 10)) return;
    lv_obj_t *m = map_obj = ui_box(parent);
    lv_obj_set_width(m, LV_PCT(100));
    float ar = (float)(HM.y1 - HM.y0) / (float)(HM.x1 - HM.x0 > 0 ? HM.x1 - HM.x0 : 1);
    lv_obj_set_height(m, (int32_t)fminf(EM(8), fmaxf(EM(4), EM(16) * ar)));
    lv_obj_add_event_cb(m, on_map_draw, LV_EVENT_DRAW_MAIN, NULL);
}

void hmap_tick(void)
{
    static int got_was = -1;
    if (map_obj && HM.got != got_was) lv_obj_invalidate(map_obj);
    got_was = HM.got;
}
