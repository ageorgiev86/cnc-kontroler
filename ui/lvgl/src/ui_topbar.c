/* Горната лента (.statusbar в макета):
 * RESET, CNC, състояние, режим, вид машина, G-режими, инструмент, MCU | съобщение | час, авариен стоп */
#include <stdio.h>
#include <time.h>

#include "ui.h"
#include "ui_style.h"

static void on_reset(lv_event_t *e) { machine_reset(lv_tick_get()); }
static void on_estop(lv_event_t *e) { machine_estop(); }

/* .chip — рамка с текст; връща контейнера */
static lv_obj_t *chip(lv_obj_t *parent)
{
    lv_obj_t *c = ui_box(parent);
    lv_obj_set_style_bg_color(c, C_SURF, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, C_EDGE2, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, EM(0.35f), 0);
    lv_obj_set_style_pad_hor(c, EM(0.65f), 0);
    lv_obj_set_style_pad_ver(c, EM(0.4f), 0);
    lv_obj_set_style_text_font(c, F.ui_sm, 0);
    lv_obj_set_style_text_color(c, C_FG, 0);
    lv_obj_set_style_text_letter_space(c, EM(0.05f), 0);
    ui_flex(c, LV_FLEX_FLOW_ROW, 0.25f);
    return c;
}

static lv_obj_t *chip_text(lv_obj_t *parent, const char *txt)
{
    lv_obj_t *c = chip(parent);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, txt);
    return l;
}

void topbar_create(topbar_t *tb, lv_obj_t *parent)
{
    lv_obj_t *bar = tb->bar = ui_box(parent);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_style_bg_color(bar, C_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, C_EDGE, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_pad_ver(bar, EM(0.45f), 0);
    lv_obj_set_style_pad_hor(bar, EM(0.6f), 0);
    ui_flex(bar, g_portrait ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_ROW, 0.5f);
    lv_obj_set_style_pad_row(bar, EM(0.4f), 0);

    /* лява група */
    lv_obj_t *l = tb->left = ui_box(bar);
    ui_flex(l, LV_FLEX_FLOW_ROW, 0.4f);
    if (g_portrait) lv_obj_set_width(l, LV_PCT(100));

    tb->reset = ui_btn(l, "RESET", BTN_RST, false);
    lv_obj_add_event_cb(tb->reset, on_reset, LV_EVENT_CLICKED, NULL);

    lv_obj_t *brand = ui_label(l, F.ui_md, C_FG, "CNC");
    lv_obj_set_style_text_letter_space(brand, EM(0.08f), 0);
    lv_obj_set_style_pad_left(brand, EM(0.2f), 0);
    lv_obj_set_style_pad_right(brand, EM(0.35f), 0);

    lv_obj_t *p = tb->pill = ui_box(l);
    lv_obj_set_style_min_width(p, EM(8), 0);
    lv_obj_set_style_pad_hor(p, EM(0.8f), 0);
    lv_obj_set_style_pad_ver(p, EM(0.45f), 0);
    lv_obj_set_style_radius(p, EM(0.3f), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    ui_flex(p, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    tb->pill_lbl = ui_label(p, F.ui_md, C_FG, "");
    lv_obj_set_style_text_letter_space(tb->pill_lbl, EM(0.08f), 0);

    tb->mode = chip_text(l, "АВТО");
    tb->mach = chip_text(l, "");

    lv_obj_t *g = chip(l);
    tb->gmodes_wcs = ui_label(g, F.ui_sm, C_HI, "G54");
    lv_obj_set_style_text_letter_space(tb->gmodes_wcs, EM(0.05f), 0);
    lv_label_set_text(lv_label_create(g), "G17 G21 G90");

    tb->tool = chip_text(l, "");

    lv_obj_t *m = chip(l);
    lv_obj_t *led = tb->mcu_led = ui_box(m);
    lv_obj_set_size(led, EM(0.7f), EM(0.7f));
    lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
    lv_obj_set_style_margin_right(led, EM(0.15f), 0);
    tb->mcu = lv_label_create(m);
    /* .chip.mcu: без рамка, зелен текст (връзката с MCU) */
    lv_obj_set_style_border_width(m, 0, 0);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(m, F.ui_md, 0);
    lv_obj_set_style_text_color(m, C_OK, 0);

    /* съобщение */
    tb->msg = ui_label(bar, F.txt, C_DIM, "");
    lv_label_set_long_mode(tb->msg, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(tb->msg, 1, F.txt->line_height);   /* един ред; ширината идва от flex_grow */
    lv_obj_set_flex_grow(tb->msg, 1);
    lv_obj_set_style_pad_hor(tb->msg, EM(g_portrait ? 0.2f : 0.5f), 0);

    /* дясна група */
    lv_obj_t *r = tb->right = ui_box(bar);
    ui_flex(r, LV_FLEX_FLOW_ROW, 0.4f);
    tb->clock = ui_label(r, F.num_md, C_DIM, "--:--");
    lv_obj_set_style_pad_hor(tb->clock, EM(0.4f), 0);
    tb->estop = ui_btn(r, "АВАРИЕН СТОП", BTN_ESTOP, false);
    lv_obj_add_event_cb(tb->estop, on_estop, LV_EVENT_CLICKED, NULL);
}

static const char *const STATE_TXT[] = {"ГОТОВ", "РАБОТИ", "ПАУЗА", "АЛАРМА", "АВАРИЕН СТОП"};
static const char *const MACH_TXT[] = {"Фреза / рутер", "Лазер", "Плазма", "Струг"};

void topbar_update(topbar_t *tb, uint32_t now)
{
    char buf[96];

    /* RESET: червен и мига, докато машината е изключена */
    static btn_kind_t rk = BTN_RST;
    btn_kind_t k = M.homing ? BTN_RST_HOMING : (M.ready && M.enabled) ? BTN_RST_OK : BTN_RST;
    if (k != rk) { ui_btn_set_kind(tb->reset, k); rk = k; }
    if (k == BTN_RST) lv_obj_set_style_opa(tb->reset, ui_blink_on(now, 1200) ? LV_OPA_COVER : 140, 0);

    /* състояние */
    bool off = !M.enabled && M.state == MS_IDLE;
    lv_color_t bg, fg;
    lv_opa_t bo = LV_OPA_COVER;
    if (off) { bg = C_SURF3; fg = C_DIM; }
    else switch (M.state) {
    case MS_IDLE: bg = C_OK; bo = 46; fg = C_OK; break;   /* 18 % */
    case MS_RUN: bg = C_INFO; fg = lv_color_hex(0x06131d); break;
    case MS_HOLD: bg = C_AMB; fg = C_AMB_INK; break;
    default: bg = C_BAD; fg = lv_color_white(); break;
    }
    lv_obj_set_style_bg_color(tb->pill, bg, 0);
    lv_obj_set_style_bg_opa(tb->pill, bo, 0);
    lv_obj_set_style_text_color(tb->pill_lbl, fg, 0);
    lv_obj_set_style_opa(tb->pill, M.state == MS_ESTOP && !ui_blink_on(now, 1000) ? 140 : LV_OPA_COVER, 0);
    ui_set_text(tb->pill_lbl, off ? "ИЗКЛЮЧЕНА" : STATE_TXT[M.state]);

    ui_set_text(tb->mach, MACH_TXT[M.type]);
    ui_set_text(tb->mode, g_screen == SCR_JOG ? "РЪЧЕН" : g_screen == SCR_MDI ? "MDI" : "АВТО");
    snprintf(buf, sizeof buf, "G%d", 53 + M.wcs);
    ui_set_text(tb->gmodes_wcs, buf);
    if (M.tool) snprintf(buf, sizeof buf, "T%d · %s Ø%g", M.tool, M.tool_desc, M.tool_dia);
    else snprintf(buf, sizeof buf, "Без инструмент");
    ui_set_text(tb->tool, buf);

    lv_obj_set_style_bg_color(tb->mcu_led, M.mcu_link ? C_OK : C_BAD, 0);
    if (M.mcu_link) snprintf(buf, sizeof buf, "MCU %d kHz", M.mcu_rate_hz / 1000);
    else snprintf(buf, sizeof buf, "MCU няма връзка");
    ui_set_text(tb->mcu, buf);
    lv_obj_set_style_text_color(tb->mcu, M.mcu_link ? C_OK : C_BAD, 0);

    /* съобщение: авария > временно съобщение */
    const char *txt = "";
    lv_color_t mc = C_DIM;
    if (M.state == MS_ESTOP) {
        txt = "Аварийният стоп е натиснат. Захранването на драйверите и шпиндела е прекъснато.";
        mc = C_BAD;
    } else if ((int32_t)(M.msg_until - now) > 0) {
        txt = M.msg;
        mc = M.msg_kind == MSG_BAD ? C_BAD : M.msg_kind == MSG_ACT ? C_ACC : C_DIM;
    }
    ui_set_text(tb->msg, txt);
    lv_obj_set_style_text_color(tb->msg, mc, 0);

    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    snprintf(buf, sizeof buf, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    ui_set_text(tb->clock, buf);
}
