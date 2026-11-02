#include "ui_style.h"

#include <math.h>
#include <string.h>

#define DECL_TIER(t)                                                           \
    LV_FONT_DECLARE(font_ui_xs_t##t) LV_FONT_DECLARE(font_ui_sm_t##t)          \
    LV_FONT_DECLARE(font_ui_md_t##t) LV_FONT_DECLARE(font_ui_ax_t##t)          \
    LV_FONT_DECLARE(font_txt_t##t) LV_FONT_DECLARE(font_num_sm_t##t)           \
    LV_FONT_DECLARE(font_num_md_t##t) LV_FONT_DECLARE(font_num_lg_t##t)
DECL_TIER(16)
DECL_TIER(24)

#define TIER(t)                                                                \
    {&font_ui_xs_t##t, &font_ui_sm_t##t, &font_ui_md_t##t, &font_ui_ax_t##t,  \
     &font_txt_t##t,   &font_num_sm_t##t, &font_num_md_t##t, &font_num_lg_t##t}
static const ui_fonts_t TIER16 = TIER(16);
static const ui_fonts_t TIER24 = TIER(24);

ui_fonts_t F;
float g_em = 16;
bool g_portrait = true;

int32_t EM(float x) { return (int32_t)lroundf(x * g_em); }

void ui_style_init(int32_t w, int32_t h)
{
    int32_t s = w < h ? w : h;
    g_em = s / 800.0f * 16.0f;
    g_portrait = h > w;
    /* Шрифтовете са в две степени; по-едрата е от em 20 нагоре (1080 и 1200) */
    F = g_em >= 20 ? TIER24 : TIER16;
}

void ui_flex(lv_obj_t *obj, lv_flex_flow_t flow, float gap_em)
{
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, flow);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(obj, EM(gap_em), 0);
}

/* Празен контейнер без стил и без превъртане */
lv_obj_t *ui_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, txt);
    return l;
}

void ui_set_text(lv_obj_t *label, const char *txt)
{
    const char *cur = lv_label_get_text(label);
    if (!cur || strcmp(cur, txt) != 0) lv_label_set_text(label, txt);
}

void ui_btn_set_kind(lv_obj_t *b, btn_kind_t kind)
{
    lv_color_t bg = C_SURF2, fg = C_FG, border = C_LINE;
    lv_opa_t bg_opa = LV_OPA_COVER;
    int32_t bw = 1;
    switch (kind) {
    case BTN_NORMAL: break;
    case BTN_ON: bg = C_ACC; fg = C_ACC_INK; border = C_ACC; break;
    case BTN_GHOST: bg_opa = LV_OPA_TRANSP; break;
    case BTN_NEED: bg = C_ACC; bg_opa = 33; fg = C_ACC; border = C_ACC; break;   /* --acc-soft = 13 % */
    case BTN_RST: bg = C_BAD; fg = lv_color_white(); border = C_BAD_RIM; break;
    case BTN_RST_HOMING: bg = C_ACC; fg = C_ACC_INK; border = C_ACC; break;
    case BTN_RST_OK: bg = C_OK; fg = lv_color_hex(0x04170d); border = C_OK; break;
    case BTN_ESTOP: bg = C_BAD; fg = lv_color_white(); border = C_BAD_RIM; bw = EM(0.15f); break;
    }
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, bg_opa, 0);
    lv_obj_set_style_border_color(b, border, 0);
    lv_obj_set_style_border_width(b, bw, 0);
    lv_obj_set_style_text_color(b, fg, 0);
    lv_obj_set_style_opa(b, LV_OPA_COVER, 0);
    /* натиснат (.btn:active) */
    if (kind == BTN_NORMAL || kind == BTN_GHOST) {
        lv_obj_set_style_bg_color(b, C_SURF3, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_color(b, bg, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, bg_opa, LV_STATE_PRESSED);
    }
}

lv_obj_t *ui_btn(lv_obj_t *parent, const char *txt, btn_kind_t kind, bool small)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_scrollable(b, false);
    bool big = kind == BTN_RST || kind == BTN_RST_HOMING || kind == BTN_RST_OK || kind == BTN_ESTOP;
    float h = small ? 2.3f : kind == BTN_ESTOP ? 2.7f : big ? 2.6f : 3.0f;
    lv_obj_set_size(b, LV_SIZE_CONTENT, EM(h));
    lv_obj_set_style_pad_hor(b, EM(small ? (g_portrait ? 0.42f : 0.6f) : 0.85f), 0);
    lv_obj_set_style_radius(b, EM(0.35f), 0);
    /* .btn.sm е .88em; Roboto Condensed е по-широк от Barlow Condensed, затова ui_xs */
    lv_obj_set_style_text_font(b, big ? F.ui_md : small ? F.ui_xs : F.ui_sm, 0);
    lv_obj_set_style_text_letter_space(b, EM(big ? 0.08f : 0.05f), 0);
    ui_flex(b, LV_FLEX_FLOW_ROW, 0.4f);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_btn_set_kind(b, kind);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    return b;
}

/* Карта: рамка, заглавна лента (.card-h) с надпис и група вдясно */
lv_obj_t *ui_card(lv_obj_t *parent, const char *title, lv_obj_t **hdr_right)
{
    lv_obj_t *c = ui_box(parent);
    lv_obj_set_style_bg_color(c, C_SURF, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, C_LINE, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, EM(0.45f), 0);
    lv_obj_set_style_clip_corner(c, true, 0);
    ui_flex(c, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *h = ui_box(c);
    lv_obj_set_size(h, LV_PCT(100), EM(2.9f));
    lv_obj_set_style_pad_left(h, EM(0.75f), 0);
    lv_obj_set_style_pad_right(h, EM(0.4f), 0);
    lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(h, C_LINE, 0);
    lv_obj_set_style_border_width(h, 1, 0);
    ui_flex(h, LV_FLEX_FLOW_ROW, 0.5f);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *t = ui_label(h, F.ui_xs, C_DIM, title);
    lv_obj_set_style_text_letter_space(t, EM(0.1f), 0);
    /* ако бутоните вдясно не оставят място, заглавието се съкращава, а не се застъпва */
    lv_obj_set_flex_grow(t, 1);
    lv_obj_set_width(t, 1);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_CLIP);

    lv_obj_t *r = ui_box(h);
    ui_flex(r, LV_FLEX_FLOW_ROW, 0.5f);
    if (hdr_right) *hdr_right = r;
    return c;
}
