/* Цветове, мащаб, шрифтове и общи елементи — съответствие с ui/maket.html.
 * Всички размери са в em, както в макета: em = min(W,H)/800 * 16 px. */
#ifndef CNC_UI_STYLE_H
#define CNC_UI_STYLE_H

#include "lvgl.h"

/* Палитра (:root в макета): тъмно синьо, сини рамки със сияние */
#define C_BG      lv_color_hex(0x03060c)
#define C_PANEL   lv_color_hex(0x060b15)
#define C_SURF    lv_color_hex(0x0a1322)
#define C_SURF2   lv_color_hex(0x0e1a2e)
#define C_SURF3   lv_color_hex(0x1b2b45)
#define C_LINE    lv_color_hex(0x1a2f52)   /* вътрешни разделители */
#define C_EDGE    lv_color_hex(0x2a6ee0)   /* рамка на картите */
#define C_EDGE2   lv_color_hex(0x21498a)   /* рамка на бутоните и чиповете */
#define C_GLOW    lv_color_hex(0x2d7dff)   /* сияние (shadow) */
#define C_FG      lv_color_hex(0xf2f6ff)
#define C_DIM     lv_color_hex(0x8b9bb6)
#define C_FAINT   lv_color_hex(0x4e5e7a)
#define C_HI      lv_color_hex(0x8cc8ff)   /* стойности, букви на осите */
#define C_ACC     lv_color_hex(0x3d8bff)
#define C_ACC_INK lv_color_hex(0xffffff)
#define C_AMB     lv_color_hex(0xf2b632)   /* внимание: пауза, реферирване */
#define C_AMB_INK lv_color_hex(0x1d1500)
#define C_OK      lv_color_hex(0x22d37a)
#define C_WARN    lv_color_hex(0xff9f43)
#define C_BAD     lv_color_hex(0xff4d5a)
#define C_INFO    lv_color_hex(0x5ab4ff)
#define C_BAD_RIM lv_color_hex(0xffb3b5)

/* Сиянието струва процесорно време при рисуване (софтуерно, без GPU).
 * Ползва се само по статичните рамки; 0 го изключва. */
#ifndef CNC_UI_GLOW
#define CNC_UI_GLOW 1
#endif
void ui_glow(lv_obj_t *obj, float width_em, lv_opa_t opa);

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
    const lv_font_t *num_gc; /* редовете на G-code                   (JetBrains Mono 400) */
    const lv_font_t *num_ov; /* стойностите на оверрайдите           (JetBrains Mono 500) */
    const lv_font_t *num_pg; /* таймерите на програмата              (JetBrains Mono 500) */
    const lv_font_t *txt_sm; /* легенда и подсказки                  (Roboto 500) */
    const lv_font_t *ui_lg;  /* бутоните за ръчно движение           (Roboto Condensed 700) */
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
    BTN_GO, BTN_HOLD, BTN_STOP,              /* бутоните на цикъла (.btn.go/.hold/.stop) */
} btn_kind_t;

lv_obj_t *ui_box(lv_obj_t *parent);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *txt);
lv_obj_t *ui_btn(lv_obj_t *parent, const char *txt, btn_kind_t kind, bool small);
void ui_btn_set_kind(lv_obj_t *btn, btn_kind_t kind);
lv_obj_t *ui_card(lv_obj_t *parent, const char *title, lv_obj_t **hdr_right);
void ui_flex(lv_obj_t *obj, lv_flex_flow_t flow, float gap_em);
void ui_set_text(lv_obj_t *label, const char *txt);
lv_obj_t *ui_btn_label(lv_obj_t *btn);
void ui_seg_set(lv_obj_t **btns, int n, int on);   /* един включен бутон от група (.seg) */
lv_obj_t *ui_lab(lv_obj_t *parent, const char *txt);   /* .lab: малък надпис над група */
lv_obj_t *ui_kv(lv_obj_t *parent, const char *key, lv_obj_t **val);   /* .kv: ключ вляво, стойност вдясно */
lv_obj_t *ui_led(lv_obj_t *parent, float size_em);
void ui_led_set(lv_obj_t *led, lv_color_t c, bool on);
lv_obj_t *ui_body(lv_obj_t *card, float gap_em);   /* .card-b: тялото на картата, колона */
void ui_grid(lv_obj_t *obj, const int32_t *cols, const int32_t *rows, float gap_em);
void ui_cell(lv_obj_t *obj, int col, int cspan, int row, int rspan);
void ui_upper(char *dst, size_t n, const char *src);   /* главни букви (латиница и кирилица) */
int32_t ui_text_w(const char *txt, const lv_font_t *font);   /* ширина на текст в пиксели */   /* STRETCH в клетката */

#endif
