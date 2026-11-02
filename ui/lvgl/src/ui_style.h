/* Цветове, мащаб, шрифтове и общи елементи — съответствие с ui/maket.html.
 * Всички размери са в em, както в макета: em = min(W,H)/800 * 16 px. */
#ifndef CNC_UI_STYLE_H
#define CNC_UI_STYLE_H

#include "lvgl.h"

/* Палитра (:root в макета) */
#define C_BG      lv_color_hex(0x0d1114)
#define C_PANEL   lv_color_hex(0x11161a)
#define C_SURF    lv_color_hex(0x171e24)
#define C_SURF2   lv_color_hex(0x1f282f)
#define C_SURF3   lv_color_hex(0x2a353e)
#define C_LINE    lv_color_hex(0x29343c)
#define C_FG      lv_color_hex(0xe3e9ed)
#define C_DIM     lv_color_hex(0x8a99a4)
#define C_FAINT   lv_color_hex(0x56636c)
#define C_ACC     lv_color_hex(0xf2b632)
#define C_ACC_INK lv_color_hex(0x1d1500)
#define C_OK      lv_color_hex(0x3dbb7e)
#define C_WARN    lv_color_hex(0xff9447)
#define C_BAD     lv_color_hex(0xec4f54)
#define C_INFO    lv_color_hex(0x59aee6)
#define C_BAD_RIM lv_color_hex(0xffb3b5)

/* Шрифтове по роля (виж fonts/gen_fonts.sh) */
typedef struct {
    const lv_font_t *ui_xs;  /* надписи на карти, мерни единици   (Roboto Condensed 600) */
    const lv_font_t *ui_sm;  /* чипове, малки бутони               (Roboto Condensed 600) */
    const lv_font_t *ui_md;  /* състояние, RESET, авариен стоп      (Roboto Condensed 700) */
    const lv_font_t *ui_ax;  /* буквите на осите                     (Roboto Condensed 700) */
    const lv_font_t *txt;    /* съобщения                            (Roboto 500) */
    const lv_font_t *num_sm; /* малки числа                          (JetBrains Mono 400) */
    const lv_font_t *num_md; /* час                                  (JetBrains Mono 500) */
    const lv_font_t *num_lg; /* позициите                            (JetBrains Mono 500) */
} ui_fonts_t;

extern ui_fonts_t F;
extern float g_em;
extern bool g_portrait;

void ui_style_init(int32_t w, int32_t h);
int32_t EM(float x);

/* Видове бутони (класовете .btn в макета) */
typedef enum {
    BTN_NORMAL, BTN_ON, BTN_GHOST, BTN_NEED, /* NEED = .homebtn.need */
    BTN_RST, BTN_RST_HOMING, BTN_RST_OK, BTN_ESTOP,
} btn_kind_t;

lv_obj_t *ui_box(lv_obj_t *parent);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *txt);
lv_obj_t *ui_btn(lv_obj_t *parent, const char *txt, btn_kind_t kind, bool small);
void ui_btn_set_kind(lv_obj_t *btn, btn_kind_t kind);
lv_obj_t *ui_card(lv_obj_t *parent, const char *title, lv_obj_t **hdr_right);
void ui_flex(lv_obj_t *obj, lv_flex_flow_t flow, float gap_em);
void ui_set_text(lv_obj_t *label, const char *txt);

#endif
