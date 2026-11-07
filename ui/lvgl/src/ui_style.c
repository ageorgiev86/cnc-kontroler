#include "ui_style.h"

#include <math.h>
#include <string.h>

#include "src/misc/lv_text_private.h"

#define DECL_TIER(t)                                                           \
    LV_FONT_DECLARE(font_ui_xs_t##t) LV_FONT_DECLARE(font_ui_sm_t##t)          \
    LV_FONT_DECLARE(font_ui_md_t##t) LV_FONT_DECLARE(font_ui_ax_t##t)          \
    LV_FONT_DECLARE(font_txt_t##t) LV_FONT_DECLARE(font_num_sm_t##t)           \
    LV_FONT_DECLARE(font_num_md_t##t) LV_FONT_DECLARE(font_num_lg_t##t)        \
    LV_FONT_DECLARE(font_num_gc_t##t) LV_FONT_DECLARE(font_num_ov_t##t)        \
    LV_FONT_DECLARE(font_num_pg_t##t) LV_FONT_DECLARE(font_txt_sm_t##t)        \
    LV_FONT_DECLARE(font_ui_lg_t##t)
DECL_TIER(10)
DECL_TIER(16)
DECL_TIER(24)

#define TIER(t)                                                                \
    {&font_ui_xs_t##t, &font_ui_sm_t##t, &font_ui_md_t##t, &font_ui_ax_t##t,  \
     &font_txt_t##t,   &font_num_sm_t##t, &font_num_md_t##t, &font_num_lg_t##t, \
     &font_num_gc_t##t, &font_num_ov_t##t, &font_num_pg_t##t, &font_txt_sm_t##t, \
     &font_ui_lg_t##t}
static const ui_fonts_t TIER10 = TIER(10);
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
    F = g_em >= 20 ? TIER24 : g_em >= 13 ? TIER16 : TIER10;   /* t10 за 480–600 по късата страна */
}

void ui_flex(lv_obj_t *obj, lv_flex_flow_t flow, float gap_em)
{
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, flow);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(obj, EM(gap_em), 0);
}

/* Сияние като box-shadow 0 0 W в макета; 0 = без */
void ui_glow(lv_obj_t *obj, float width_em, lv_opa_t opa)
{
    if (!CNC_UI_GLOW || width_em <= 0) {
        lv_obj_set_style_shadow_width(obj, 0, 0);
        return;
    }
    lv_obj_set_style_shadow_width(obj, EM(width_em), 0);
    lv_obj_set_style_shadow_color(obj, C_GLOW, 0);
    lv_obj_set_style_shadow_opa(obj, opa, 0);
    lv_obj_set_style_shadow_spread(obj, 0, 0);
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
    lv_color_t bg = C_SURF2, fg = C_FG, border = C_EDGE2;
    lv_opa_t bg_opa = LV_OPA_COVER;
    int32_t bw = 1;
    switch (kind) {
    case BTN_NORMAL: break;
    case BTN_ON: bg = C_ACC; bg_opa = 77; fg = C_FG; border = C_HI; break;   /* --acc-soft = 30 % */
    case BTN_GHOST: bg_opa = LV_OPA_TRANSP; break;
    case BTN_NEED: bg = C_AMB; bg_opa = 36; fg = C_AMB; border = C_AMB; break;   /* 14 % */
    case BTN_RST: bg = C_BAD; fg = lv_color_white(); border = C_BAD_RIM; break;
    case BTN_RST_HOMING: bg = C_AMB; fg = C_AMB_INK; border = C_AMB; break;
    case BTN_RST_OK: bg = C_OK; fg = lv_color_hex(0x04170d); border = C_OK; break;
    case BTN_ESTOP: bg = C_BAD; fg = lv_color_white(); border = C_BAD_RIM; bw = EM(0.15f); break;
    case BTN_GO: bg = C_OK; fg = lv_color_hex(0x04170d); border = C_OK; break;
    case BTN_HOLD: bg = C_AMB; fg = C_AMB_INK; border = C_AMB; break;
    case BTN_STOP: bg = C_BAD; fg = lv_color_white(); border = C_BAD; break;
    }
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, bg_opa, 0);
    lv_obj_set_style_border_color(b, border, 0);
    lv_obj_set_style_border_width(b, bw, 0);
    lv_obj_set_style_text_color(b, fg, 0);
    lv_obj_set_style_opa(b, LV_OPA_COVER, 0);
    /* включеният бутон свети (.btn.on), останалите са без сияние */
    ui_glow(b, kind == BTN_ON ? 1.2f : 0, 170);
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
    lv_obj_set_style_radius(b, EM(0.45f), 0);
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
    lv_obj_set_style_border_color(c, C_EDGE, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, EM(0.55f), 0);
    ui_glow(c, 1.1f, 90);   /* в LVGL сянката стига до width/2, затова 2 × blur от CSS */
    lv_obj_set_style_clip_corner(c, true, 0);
    ui_flex(c, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    if (!title) {   /* карта без заглавна лента (оверрайдите) */
        if (hdr_right) *hdr_right = NULL;
        return c;
    }

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

lv_obj_t *ui_btn_label(lv_obj_t *btn) { return lv_obj_get_child(btn, 0); }

void ui_seg_set(lv_obj_t **btns, int n, int on)
{
    for (int i = 0; i < n; i++)
        if (btns[i]) ui_btn_set_kind(btns[i], i == on ? BTN_ON : BTN_NORMAL);
}

lv_obj_t *ui_lab(lv_obj_t *parent, const char *txt)
{
    lv_obj_t *l = ui_label(parent, F.ui_xs, C_DIM, txt);
    lv_obj_set_style_text_letter_space(l, EM(0.1f), 0);
    return l;
}

lv_obj_t *ui_kv(lv_obj_t *parent, const char *key, lv_obj_t **val)
{
    lv_obj_t *r = ui_box(parent);
    lv_obj_set_width(r, LV_PCT(100));
    ui_flex(r, LV_FLEX_FLOW_ROW, 0.6f);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *k = ui_label(r, F.txt_sm, C_DIM, key);
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_width(k, 1);
    lv_label_set_long_mode(k, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *v = ui_label(r, F.num_sm, C_FG, "");
    if (val) *val = v;
    return r;
}

lv_obj_t *ui_led(lv_obj_t *parent, float size_em)
{
    lv_obj_t *l = ui_box(parent);
    lv_obj_set_size(l, EM(size_em), EM(size_em));
    lv_obj_set_style_radius(l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(l, C_SURF3, 0);
    return l;
}

void ui_led_set(lv_obj_t *led, lv_color_t c, bool on)
{
    lv_obj_set_style_bg_color(led, on ? c : C_SURF3, 0);
    if (on && CNC_UI_GLOW) {
        lv_obj_set_style_shadow_width(led, EM(0.6f), 0);
        lv_obj_set_style_shadow_color(led, c, 0);
        lv_obj_set_style_shadow_opa(led, 150, 0);
    } else {
        lv_obj_set_style_shadow_width(led, 0, 0);
    }
}

lv_obj_t *ui_body(lv_obj_t *card, float gap_em)
{
    lv_obj_t *b = ui_box(card);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_pad_all(b, EM(0.7f), 0);
    ui_flex(b, LV_FLEX_FLOW_COLUMN, gap_em);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return b;
}

void ui_grid(lv_obj_t *obj, const int32_t *cols, const int32_t *rows, float gap_em)
{
    lv_obj_set_layout(obj, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj, cols, rows);
    lv_obj_set_style_pad_gap(obj, EM(gap_em), 0);
}

void ui_cell(lv_obj_t *obj, int col, int cspan, int row, int rspan)
{
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, col, cspan, LV_GRID_ALIGN_STRETCH, row, rspan);
}

int32_t ui_text_w(const char *txt, const lv_font_t *font)
{
    lv_text_attributes_t a = {0};
    a.max_width = LV_COORD_MAX;
    return lv_text_get_width(txt, (uint32_t)strlen(txt), font, &a);
}

void ui_upper(char *d, size_t n, const char *s)
{
    size_t o = 0;
    while (*s && o + 3 < n) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) { d[o++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c); s++; continue; }
        if ((c & 0xE0) == 0xC0 && s[1]) {
            unsigned cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[1] & 0x3Fu);
            if (cp >= 0x430 && cp <= 0x44F) cp -= 0x20;
            else if (cp == 0x451) cp = 0x401;
            d[o++] = (char)(0xC0 | (cp >> 6));
            d[o++] = (char)(0x80 | (cp & 0x3F));
            s += 2;
            continue;
        }
        d[o++] = *s++;
    }
    d[o] = 0;
}
