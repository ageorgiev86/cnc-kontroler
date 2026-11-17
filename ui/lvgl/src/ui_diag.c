/* Екранът „Диагностика“ (.diag в макета): енкодери и следяща грешка, системни входове и изходи на STM32,
 * аналоговите канали, 32 + 32 входа и изхода (изходите се включват ръчно с двойно натискане) и връзката с MCU. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "io.h"
#include "ui.h"
#include "ui_style.h"
#include "update.h"

#define ENC_ROWS (AX_N + 1)
static lv_obj_t *enc_row[ENC_ROWS][7], *enc_st[ENC_ROWS], *enc_line[ENC_ROWS];
static lv_obj_t *sys_led[24], *ai_box[AIN_N + AOUT_N], *ai_nm[6], *ai_v[6], *ai_tag[6], *ai_bar[6], *ai_mk[6][2], *ai_ft[6][3], *ai_empty;
static lv_obj_t *gio[2][IO_N], *gio_led[2][IO_N], *gio_nm[2][IO_N], *gio_cnt[2], *air_btn, *sim_al_btn, *lk_v[5];
static int st_was[ENC_ROWS];

static void on_alarm(lv_event_t *e)
{
    uint32_t now = lv_tick_get();
    if (M.state == MS_ESTOP) return;
    if (!M.enc_on[AX_Y]) { machine_toast("Ос Y е без енкодер: няма следяща грешка", now, 3500); return; }
    M.ferr[AX_Y] = 52.4;
    char t[120];
    snprintf(t, sizeof t, "Следящата грешка по Y е 52.4 µm при лимит %.0f µm", M.ferr_lim[AX_Y]);
    machine_alarm(t);
}
static void on_air(lv_event_t *e)
{
    air_drop = !air_drop;
    ui_btn_set_kind(air_btn, air_drop ? BTN_ON : BTN_NORMAL);
    machine_toast(air_drop ? "Симулация: налягането на въздуха пада" : "Налягането е възстановено", lv_tick_get(), 3500);
}
static void on_force_clr(lv_event_t *e)
{
    for (int i = 0; i < IO_N; i++) io_force[i] = -1;
    io_force_arm = -1;
    machine_toast("Всички изходи са върнати към автоматично управление", lv_tick_get(), 3500);
}
static uint32_t arm_at;
static void on_force(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    uint32_t now = lv_tick_get();
    char t[160];
    if (M.acc_level < 1) { machine_toast("Ръчното включване на изходи изисква вход като Настройчик (Настройки)", now, 4000); return; }
    if (!M.enabled) { machine_can_move(now); return; }
    if (M.state != MS_IDLE) { machine_toast("Изходите се включват ръчно само когато машината е спряна", now, 3500); return; }
    if (io_force_arm != i || now - arm_at > 3000) {
        io_force_arm = i;
        arm_at = now;
        snprintf(t, sizeof t, "OUT%d: натиснете пак за ръчно %s", i + 1, io_out(i) ? "изключване" : "включване");
        machine_toast(t, now, 3000);
        return;
    }
    io_force_arm = -1;
    io_force[i] = !io_out(i);
    snprintf(t, sizeof t, "OUT%d е ръчно %s. „Освободи ръчните“ го връща към автоматично.", i + 1, io_force[i] ? "включен" : "изключен");
    machine_toast(t, now, 4000);
}

static lv_obj_t *cell(lv_obj_t *g, const lv_font_t *f, lv_color_t c, int col, int row, bool right)
{
    lv_obj_t *l = ui_label(g, f, c, "");
    lv_obj_set_grid_cell(l, right ? LV_GRID_ALIGN_END : LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);
    return l;
}

static lv_obj_t *enc_card(lv_obj_t *scr)
{
    lv_obj_t *hr;
    lv_obj_t *c = ui_card(scr, "ЕНКОДЕРИ И СЛЕДЯЩА ГРЕШКА", &hr);
    sim_al_btn = ui_btn(hr, "СИМУЛИРАЙ АЛАРМА ПО Y", BTN_NORMAL, true);
    lv_obj_add_event_cb(sim_al_btn, on_alarm, LV_EVENT_CLICKED, NULL);
    static int32_t cols[8], rows[ENC_ROWS + 2];
    cols[0] = EM(2.2f); cols[1] = LV_GRID_FR(10); cols[2] = LV_GRID_FR(12); cols[3] = LV_GRID_FR(12); cols[4] = LV_GRID_FR(9);
    cols[5] = LV_GRID_FR(11); cols[6] = LV_GRID_CONTENT; cols[7] = LV_GRID_TEMPLATE_LAST;
    rows[0] = EM(2.2f);
    for (int i = 1; i <= ENC_ROWS; i++) rows[i] = LV_GRID_CONTENT;
    rows[ENC_ROWS + 1] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(c);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_style_pad_hor(g, EM(0.6f), 0);
    lv_obj_set_style_pad_bottom(g, EM(0.4f), 0);
    ui_grid(g, cols, rows, 0);
    lv_obj_set_style_pad_column(g, EM(0.6f), 0);
    static const char *const H[7] = {"ОС", "КОНТУР", "ЗАПОВЯДАНА", "ЕНКОДЕР", "ГРЕШКА", "ЛИМИТ", "СТАТУС"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t *l = cell(g, F.ui_xs, C_DIM, i, 0, i >= 2 && i <= 5);
        ui_set_text(l, H[i]);
        lv_obj_set_style_text_letter_space(l, EM(0.08f), 0);
    }
    for (int r = 0; r < ENC_ROWS; r++) {
        lv_obj_t *line = enc_line[r] = ui_box(g);   /* ред с разделител (.tt td border-bottom) */
        lv_obj_set_height(line, EM(2.6f));
        lv_obj_set_style_border_side(line, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(line, C_LINE, 0);
        lv_obj_set_style_border_width(line, 1, 0);
        lv_obj_set_grid_cell(line, LV_GRID_ALIGN_STRETCH, 0, 7, LV_GRID_ALIGN_START, r + 1, 1);
        enc_row[r][0] = cell(g, F.ui_md, C_HI, 0, r + 1, false);
        ui_set_text(enc_row[r][0], r < AX_N ? AX_NAME[r] : "S");
        enc_row[r][1] = cell(g, F.txt, C_FG, 1, r + 1, false);
        for (int k = 2; k <= 5; k++) enc_row[r][k] = cell(g, F.num_md, C_FG, k, r + 1, true);
        lv_obj_t *st = enc_st[r] = ui_label(g, F.num_sm, C_OK, "");
        lv_obj_set_style_pad_hor(st, EM(0.35f), 0);
        lv_obj_set_style_pad_ver(st, EM(0.15f), 0);
        lv_obj_set_style_radius(st, EM(0.2f), 0);
        lv_obj_set_style_bg_opa(st, 46, 0);
        lv_obj_set_grid_cell(st, LV_GRID_ALIGN_START, 6, 1, LV_GRID_ALIGN_CENTER, r + 1, 1);
        st_was[r] = -1;
    }
    return c;
}

/* собствените входове на STM32 (STATUS.local_in, бит i): имената и пиновете както в макета */
#define SYS_NN 24
static const char *const SYS_N[SYS_NN] = {"LIM 1", "LIM 2", "LIM 3", "LIM 4", "LIM 5", "LIM 6", "HOME 1", "HOME 2", "HOME 3", "HOME 4",
    "HOME 5", "HOME 6", "3D пробник", "Toolsetter", "Авариен стоп", "Аларма драйвер", "Бутон Старт", "Бутон Стоп",
    "MPG ос X", "MPG ос Y", "MPG ос Z", "MPG ×1", "MPG ×10", "MPG ×100"};
static const char *const SYS_P[SYS_NN] = {"PG8", "PG9", "PG10", "PG11", "PG12", "PG13", "PG14", "PG15", "PE10", "PF7", "PF8", "PF9",
    "PA7", "PB1", "PC13", "PE3", "PG0", "PG1", "PG2", "PG3", "PG4", "PF2", "PF3", "PF4"};

static lv_obj_t *sys_card(lv_obj_t *scr)
{
    lv_obj_t *c = ui_card(scr, "СОБСТВЕНИ ВХОДОВЕ НА STM32", NULL);
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, rows[SYS_NN / 2 + 1];
    for (int i = 0; i < SYS_NN / 2; i++) rows[i] = EM(1.95f);
    rows[SYS_NN / 2] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(c);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_style_pad_all(g, EM(0.5f), 0);
    lv_obj_set_style_pad_hor(g, EM(0.7f), 0);
    ui_grid(g, cols, rows, 0);
    lv_obj_set_style_pad_column(g, EM(1), 0);
    for (int i = 0; i < SYS_NN; i++) {
        lv_obj_t *r = ui_box(g);
        ui_flex(r, LV_FLEX_FLOW_ROW, 0.45f);
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(r, C_LINE, 0);
        lv_obj_set_style_border_width(r, 1, 0);
        ui_cell(r, i % 2, 1, i / 2, 1);
        sys_led[i] = ui_led(r, 0.7f);
        lv_obj_t *n = ui_label(r, F.txt, C_FG, SYS_N[i]);
        lv_obj_set_flex_grow(n, 1);
        ui_label(r, F.num_sm, C_FAINT, SYS_P[i]);
    }
    return c;
}

static lv_obj_t *aio_card(lv_obj_t *scr)
{
    lv_obj_t *hr;
    lv_obj_t *c = ui_card(scr, "АНАЛОГОВИ ВХОДОВЕ И ИЗХОДИ", &hr);
    air_btn = ui_btn(hr, "СИМУЛИРАЙ СПАД НА НАЛЯГАНЕТО", BTN_NORMAL, true);
    lv_obj_add_event_cb(air_btn, on_air, LV_EVENT_CLICKED, NULL);
    lv_obj_t *b = ui_box(c);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_style_pad_all(b, EM(0.5f), 0);
    lv_obj_set_style_pad_hor(b, EM(0.7f), 0);
    ui_flex(b, LV_FLEX_FLOW_COLUMN, 0.45f);
    for (int i = 0; i < AIN_N + AOUT_N; i++) {
        lv_obj_t *x = ai_box[i] = ui_box(b);
        lv_obj_set_width(x, LV_PCT(100));
        lv_obj_set_style_bg_color(x, C_PANEL, 0);
        lv_obj_set_style_bg_opa(x, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(x, C_LINE, 0);
        lv_obj_set_style_border_width(x, 1, 0);
        lv_obj_set_style_radius(x, EM(0.35f), 0);
        lv_obj_set_style_pad_ver(x, EM(0.45f), 0);
        lv_obj_set_style_pad_hor(x, EM(0.6f), 0);
        ui_flex(x, LV_FLEX_FLOW_COLUMN, 0.25f);
        lv_obj_t *top = ui_box(x);
        lv_obj_set_width(top, LV_PCT(100));
        ui_flex(top, LV_FLEX_FLOW_ROW, 0.6f);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        ai_nm[i] = ui_label(top, F.txt, C_FG, "");
        lv_obj_t *vr = ui_box(top);
        ui_flex(vr, LV_FLEX_FLOW_ROW, 0.4f);
        ai_v[i] = ui_label(vr, F.num_pg, C_FG, "");
        ai_tag[i] = ui_label(vr, F.num_sm, C_OK, "");
        lv_obj_set_style_pad_hor(ai_tag[i], EM(0.3f), 0);
        lv_obj_set_style_radius(ai_tag[i], EM(0.2f), 0);
        lv_obj_set_style_bg_opa(ai_tag[i], 46, 0);
        lv_obj_t *bar = ai_bar[i] = lv_bar_create(x);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, LV_PCT(100), EM(0.6f));
        lv_obj_set_style_bg_color(bar, C_SURF3, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, EM(0.3f), 0);
        lv_obj_set_style_bg_color(bar, i < AIN_N ? C_OK : C_ACC, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, EM(0.3f), LV_PART_INDICATOR);
        lv_bar_set_range(bar, 0, 1000);
        for (int m = 0; m < 2; m++) {
            lv_obj_t *mk = ai_mk[i][m] = ui_box(bar);
            lv_obj_set_size(mk, 2, EM(1));
            lv_obj_set_style_bg_color(mk, C_WARN, 0);
            lv_obj_set_style_bg_opa(mk, LV_OPA_COVER, 0);
            lv_obj_set_overflow_visible(bar, true);
        }
        lv_obj_t *ft = ui_box(x);
        lv_obj_set_width(ft, LV_PCT(100));
        ui_flex(ft, LV_FLEX_FLOW_ROW, 0.5f);
        lv_obj_set_flex_align(ft, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        for (int k = 0; k < 3; k++) ai_ft[i][k] = ui_label(ft, F.num_sm, C_DIM, "");
    }
    ai_empty = ui_label(b, F.txt, C_DIM, "Няма активни аналогови канали. Настройват се от Настройки → Входове/изходи → Аналогови.");
    lv_obj_set_width(ai_empty, LV_PCT(100));
    lv_label_set_long_mode(ai_empty, LV_LABEL_LONG_MODE_WRAP);
    return c;
}

static void gio_grid(lv_obj_t *c, int out)
{
    lv_obj_t *h = ui_box(c);
    lv_obj_set_width(h, LV_PCT(100));
    lv_obj_set_style_pad_top(h, EM(0.55f), 0);
    lv_obj_set_style_pad_hor(h, EM(0.75f), 0);
    lv_obj_set_style_pad_bottom(h, EM(0.25f), 0);
    ui_flex(h, LV_FLEX_FLOW_ROW, 0.5f);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_lab(h, out ? "ИЗХОДИ · ДВОЙНО НАТИСКАНЕ = РЪЧНО" : "ВХОДОВЕ");
    gio_cnt[out] = ui_label(h, F.num_sm, C_DIM, "");
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, rows[17];
    for (int i = 0; i < 16; i++) rows[i] = LV_GRID_CONTENT;
    rows[16] = LV_GRID_TEMPLATE_LAST;
    lv_obj_t *g = ui_box(c);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_style_pad_hor(g, EM(0.6f), 0);
    lv_obj_set_style_pad_bottom(g, EM(0.6f), 0);
    ui_grid(g, cols, rows, 0.3f);
    for (int i = 0; i < IO_N; i++) {
        lv_obj_t *b = gio[out][i] = ui_box(g);
        lv_obj_set_style_bg_color(b, C_PANEL, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(b, C_LINE, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, EM(0.35f), 0);
        lv_obj_set_style_pad_ver(b, EM(0.32f), 0);
        lv_obj_set_style_pad_hor(b, EM(0.5f), 0);
        ui_flex(b, LV_FLEX_FLOW_ROW, 0.45f);
        gio_led[out][i] = ui_led(b, 0.75f);
        char t[8];
        snprintf(t, sizeof t, "%s%d", out ? "OUT" : "IN", i + 1);
        ui_label(b, F.num_sm, C_DIM, t);
        lv_obj_t *n = gio_nm[out][i] = ui_label(b, F.txt_sm, C_FG, "");
        lv_obj_set_flex_grow(n, 1);
        lv_obj_set_width(n, 1);
        lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
        if (out) {
            lv_obj_set_clickable(b, true);
            lv_obj_add_event_cb(b, on_force, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 2, 1, LV_GRID_ALIGN_CENTER, i / 2, 1);
    }
}

static lv_obj_t *gio_card(lv_obj_t *scr)
{
    lv_obj_t *hr;
    lv_obj_t *c = ui_card(scr, "ВХОДОВЕ И ИЗХОДИ 32 + 32", &hr);
    lv_obj_add_event_cb(ui_btn(hr, "ОСВОБОДИ РЪЧНИТЕ", BTN_NORMAL, true), on_force_clr, LV_EVENT_CLICKED, NULL);
    gio_grid(c, 0);
    gio_grid(c, 1);
    return c;
}

static lv_obj_t *link_row(lv_obj_t *scr)
{
    lv_obj_t *r = ui_box(scr);
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    ui_grid(r, cols, rows, 0.6f);
    static const char *const N[5] = {"SPI ВРЪЗКА", "ЦИКЪЛ", "БУФЕР СЕГМЕНТИ", "CRC ГРЕШКИ", "WATCHDOG"};
    static const char *const V[5] = {"16 MHz", "1.000 kHz", "0 %", "0", "ОК · 1 ms"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *c = ui_card(r, NULL, NULL);
        ui_cell(c, i, 1, 0, 1);
        lv_obj_set_style_pad_all(c, EM(0.6f), 0);
        lv_obj_set_style_pad_gap(c, EM(0.2f), 0);
        ui_lab(c, N[i]);
        lk_v[i] = ui_label(c, F.num_pg, C_FG, V[i]);
    }
    return r;
}

/* „Софтуер“: версия и „Обновяване“ (без PIN) */
static lv_obj_t *sw_ver, *sw_st;
static void on_sw_upd(lv_event_t *e) { update_open(); }
static lv_obj_t *sw_card(lv_obj_t *scr)
{
    lv_obj_t *c = ui_card(scr, "СОФТУЕР", NULL);
    lv_obj_t *b = ui_box(c);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_style_pad_hor(b, EM(0.75f), 0);
    lv_obj_set_style_pad_ver(b, EM(0.6f), 0);
    ui_flex(b, LV_FLEX_FLOW_ROW, 1.0f);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_lab(b, "ВЕРСИЯ");
    sw_ver = ui_label(b, F.num_pg, C_FG, "");
    sw_st = ui_label(b, F.txt, C_DIM, "");
    lv_obj_set_flex_grow(sw_st, 1);
    lv_obj_set_width(sw_st, 1);
    lv_label_set_long_mode(sw_st, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_event_cb(ui_btn(b, "ОБНОВЯВАНЕ", BTN_GO, false), on_sw_upd, LV_EVENT_CLICKED, NULL);
    return c;
}

static lv_obj_t *diag_scr;
/* тест: превърта екрана до y (px); -1 = до края */
void diag_test_scroll(int y)
{
    if (!diag_scr) return;
    lv_obj_update_layout(diag_scr);
    if (y < 0) lv_obj_scroll_to_y(diag_scr, LV_COORD_MAX, LV_ANIM_OFF);
    else lv_obj_scroll_to_y(diag_scr, y, LV_ANIM_OFF);
}

void diag_create(lv_obj_t *scr)
{
    diag_scr = scr;
    ui_scroll_v(scr);
    static int32_t cols[3], rows[7];
    if (g_portrait) {
        cols[0] = LV_GRID_FR(1); cols[1] = LV_GRID_TEMPLATE_LAST;
        for (int i = 0; i < 6; i++) rows[i] = LV_GRID_CONTENT;
        rows[6] = LV_GRID_TEMPLATE_LAST;
    } else {
        cols[0] = LV_GRID_FR(52); cols[1] = LV_GRID_FR(48); cols[2] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_CONTENT; rows[1] = LV_GRID_CONTENT; rows[2] = LV_GRID_CONTENT; rows[3] = LV_GRID_TEMPLATE_LAST;
    }
    ui_grid(scr, cols, rows, 0.6f);
    /* хоризонтално: левите карти са в една колона. Карта през няколко реда не става: LVGL смята
     * височината на редовете само от картите в един ред и долният ред застъпва входовете и изходите */
    lv_obj_t *left = scr;
    if (!g_portrait) {
        left = ui_box(scr);
        ui_flex(left, LV_FLEX_FLOW_COLUMN, 0.6f);
        lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    }
    lv_obj_t *e = enc_card(left), *s = sys_card(left), *a = aio_card(left), *g = gio_card(scr), *w = sw_card(scr), *l = link_row(scr);
#define TOP(o, c, cs, r, rs) lv_obj_set_grid_cell(o, LV_GRID_ALIGN_STRETCH, c, cs, LV_GRID_ALIGN_START, r, rs)
    if (g_portrait) { TOP(e, 0, 1, 0, 1); TOP(s, 0, 1, 1, 1); TOP(a, 0, 1, 2, 1); TOP(g, 0, 1, 3, 1); TOP(w, 0, 1, 4, 1); TOP(l, 0, 1, 5, 1); }
    else {
        TOP(left, 0, 1, 0, 1); TOP(g, 1, 1, 0, 1); TOP(w, 0, 2, 1, 1); TOP(l, 0, 2, 2, 1);
        lv_obj_set_width(e, LV_PCT(100)); lv_obj_set_width(s, LV_PCT(100)); lv_obj_set_width(a, LV_PCT(100));
    }
#undef TOP
}

static void f3(char *b, size_t n, double v) { snprintf(b, n, "%.3f", fabs(v) < 0.0005 ? 0 : v); }

void diag_update(uint32_t now)
{
    ui_set_text(sw_ver, UPD.cur[0] ? UPD.cur : "?");
    char sws[80];
    if (UPD.avail[0]) snprintf(sws, sizeof sws, "Има нова версия %.30s%s", UPD.avail, UPD.crit ? " (важна)" : "");
    else snprintf(sws, sizeof sws, "%s", UPD.checked[0] ? "Системата е обновена" : "");
    ui_set_text(sw_st, sws);
    lv_obj_set_style_text_color(sw_st, UPD.avail[0] ? (UPD.crit ? C_BAD : C_WARN) : C_DIM, 0);
    char b[200], k[32];
    for (int r = 0; r < ENC_ROWS; r++) {
        bool ax = r < AX_N;
        lv_obj_t **c = enc_row[r];
        bool show = !ax || M.active[r];
        for (int i = 0; i < 7; i++) lv_obj_set_hidden(c[i] ? c[i] : enc_st[r], !show);
        lv_obj_set_hidden(enc_st[r], !show);
        lv_obj_set_hidden(enc_line[r], !show);
        if (!show) continue;
        int st;
        if (ax) {
            snprintf(k, sizeof k, "%s.encMode", AX_NAME[r]);
            bool open = !M.enc_on[r];
            ui_set_text(c[1], open ? "отворен" : !strcmp(cfg_str(k), "dual") ? "двоен" : "затворен");
            f3(b, sizeof b, M.mach[r]);
            ui_set_text(c[2], b);
            if (open) { ui_set_text(c[3], "—"); ui_set_text(c[4], "—"); ui_set_text(c[5], "—"); }
            else {
                f3(b, sizeof b, M.mach[r] - fabs(M.ferr[r]) / 1000);
                ui_set_text(c[3], b);
                snprintf(b, sizeof b, "%.1f", fabs(M.ferr[r]));
                ui_set_text(c[4], b);
                snprintf(b, sizeof b, "%.0f %s", M.ferr_lim[r], M.rotary[r] ? "″" : "µm");
                ui_set_text(c[5], b);
            }
            st = open ? 2 : fabs(M.ferr[r]) > M.ferr_lim[r] ? 1 : 0;
        } else {
            int sa = (int)lroundf(M.sp_act), sc = M.sp_on ? M.sp_cmd * M.sov / 100 : 0;
            ui_set_text(c[1], "шпиндел");
            snprintf(b, sizeof b, "%d", sc); ui_set_text(c[2], b);
            snprintf(b, sizeof b, "%d", sa); ui_set_text(c[3], b);
            snprintf(b, sizeof b, "%d", sa - sc); ui_set_text(c[4], b);
            ui_set_text(c[5], "300 об/мин");
            st = abs(sa - sc) > 300 ? 3 : 0;
        }
        if (st != st_was[r]) {
            static const char *const T[4] = {"ОК", "ГРЕШКА", "БЕЗ ЕНКОДЕР", "РАМПА"};
            lv_color_t col = st == 1 ? C_BAD : st >= 2 ? C_DIM : C_OK;
            ui_set_text(enc_st[r], T[st]);
            lv_obj_set_style_text_color(enc_st[r], st == 1 ? lv_color_white() : col, 0);
            lv_obj_set_style_bg_color(enc_st[r], col, 0);
            lv_obj_set_style_bg_opa(enc_st[r], st == 1 ? LV_OPA_COVER : 46, 0);
            st_was[r] = st;
        }
    }

    /* собствените входове: активен = жълто, авариен стоп и аларма = червено; без връзка всички изключени */
    for (int i = 0; i < SYS_NN; i++) {
        bool on = M.mcu_link && (M.local_in >> i & 1);
        ui_led_set(sys_led[i], i == 14 || i == 15 ? C_BAD : C_AMB, on);
    }

    /* аналогови */
    int shown = 0;
    for (int i = 0; i < AIN_N + AOUT_N; i++) {
        bool out = i >= AIN_N;
        int ch = out ? i - AIN_N : i;
        const char *fmt = out ? "io.aout%d.%s" : "io.ain%d.%s";
#define CS(f) (snprintf(k, sizeof k, fmt, ch + 1, f), cfg_str(k))
#define CN(f) atof(CS(f))
        const char *fn = CS("f");
        bool on = strcmp(fn, "none") != 0;
        lv_obj_set_hidden(ai_box[i], !on);
        if (!on) continue;
        shown++;
        double v = out ? aio_out(ch) : aio_in(ch), lo = CN("lo"), hi = CN("hi"), r = hi - lo != 0 ? hi - lo : 1;
        double mn = CN("amin"), mx = CN("amax");
        char sig[24], nm[64], u[16];
        snprintf(u, sizeof u, "%s", CS("u"));
        aio_sig(out, ch, v, sig, sizeof sig);
        const char *n0 = CS("n");
        snprintf(nm, sizeof nm, "%s", n0[0] ? n0 : opt_label(out ? AOUT_F : AIN_F, fn));
        snprintf(b, sizeof b, "%s%d · %s   %s", out ? "AOUT" : "AIN", ch + 1, nm, sig);
        ui_set_text(ai_nm[i], b);
        snprintf(b, sizeof b, out ? "%.1f %s" : "%.2f %s", v, u);
        ui_set_text(ai_v[i], b);
        int st = out ? 0 : v < mn ? 1 : v > mx ? 2 : 0;
        bool act = !out && strcmp(CS("act"), "none");
        lv_obj_set_hidden(ai_tag[i], out);
        ui_set_text(ai_tag[i], st == 1 ? "НИСКО" : st == 2 ? "ВИСОКО" : "ОК");
        lv_obj_set_style_text_color(ai_tag[i], st ? lv_color_white() : C_OK, 0);
        lv_obj_set_style_bg_color(ai_tag[i], st ? C_BAD : C_OK, 0);
        lv_obj_set_style_bg_opa(ai_tag[i], st ? LV_OPA_COVER : 46, 0);
        lv_obj_set_style_border_color(ai_box[i], st ? C_BAD : C_LINE, 0);
        if (!out) lv_obj_set_style_bg_color(ai_bar[i], st ? C_BAD : C_OK, LV_PART_INDICATOR);
        lv_bar_set_value(ai_bar[i], (int32_t)fmax(0, fmin(1000, (v - lo) / r * 1000)), LV_ANIM_OFF);
        int32_t bw = lv_obj_get_width(ai_bar[i]);
        for (int m = 0; m < 2; m++) {
            lv_obj_set_hidden(ai_mk[i][m], !act);
            double x = fmax(0, fmin(1, ((m ? mx : mn) - lo) / r));
            lv_obj_set_pos(ai_mk[i][m], (int32_t)(x * bw) - 1, -EM(0.2f));
        }
        snprintf(b, sizeof b, "%g %s", lo, u); ui_set_text(ai_ft[i][0], b);
        if (act) snprintf(b, sizeof b, "граници %g … %g %s", mn, mx, u); else b[0] = 0;
        ui_set_text(ai_ft[i][1], b);
        snprintf(b, sizeof b, "%g %s", hi, u); ui_set_text(ai_ft[i][2], b);
#undef CS
#undef CN
    }
    lv_obj_set_hidden(ai_empty, shown > 0);

    /* 32 + 32 */
    for (int out = 0; out < 2; out++) {
        int on_n = 0;
        for (int i = 0; i < IO_N; i++) {
            bool v = out ? io_out(i) : io_in(i);
            on_n += v;
            char nm[64];
            io_name(out, i, nm, sizeof nm);
            if (out && io_force[i] >= 0) strncat(nm, "  · ръчно", sizeof nm - strlen(nm) - 1);
            ui_set_text(gio_nm[out][i], nm[0] ? nm : "резерв");
            lv_obj_set_style_text_color(gio_nm[out][i], nm[0] ? C_FG : C_FAINT, 0);
            ui_led_set(gio_led[out][i], C_OK, v);
            lv_obj_set_style_opa(gio[out][i], v ? LV_OPA_COVER : 107, 0);
            bool arm = out && io_force_arm == i && now - arm_at < 3000;
            lv_obj_set_style_border_color(gio[out][i], arm ? C_AMB : v ? lv_color_mix(C_OK, C_LINE, 140) : C_LINE, 0);
        }
        snprintf(b, sizeof b, "%d вкл. от 32", on_n);
        ui_set_text(gio_cnt[out], b);
    }
    /* симулираните аварии само без истинска машина */
    lv_obj_set_hidden(sim_al_btn, !machine_sim());
    lv_obj_set_hidden(air_btn, !machine_sim());
    /* връзката със STM32 (mc) */
    machine_link_t L;
    machine_link_info(&L);
    if (L.sim) snprintf(b, sizeof b, "симулация");
    else snprintf(b, sizeof b, "%.0f kHz", cfg_num("mcu.lnkHz") / 1000);
    ui_set_text(lk_v[0], b);
    snprintf(b, sizeof b, "%.0f /s", L.rate);
    ui_set_text(lk_v[1], b);
    snprintf(b, sizeof b, "%d ms", L.queue);
    ui_set_text(lk_v[2], b);
    snprintf(b, sizeof b, "%u / %u", L.rx_errors, L.bad_rx);
    ui_set_text(lk_v[3], b);
    ui_set_text(lk_v[4], !L.link ? "НЯМА" : M.state == MS_ESTOP ? "СПРЯН" : M.state == MS_ALARM ? "ГРЕШКА" : "ОК");
    lv_obj_set_style_text_color(lk_v[4], !L.link || M.state == MS_ALARM || M.state == MS_ESTOP ? C_BAD : C_OK, 0);
}
