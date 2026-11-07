/* Картата с програмата: име, „ред / общо“, „Отвори“ и G-code списъкът (.gc-list в макета).
 * Списъкът е виртуален: обекти има само за видимите редове, текстът им се сменя при превъртане,
 * затова и програма с десетки хиляди редове не тежи. По време на работа текущият ред е в горната трета. */
#include <stdio.h>
#include <string.h>

#include "program.h"
#include "ui.h"
#include "ui_style.h"

static void on_open(lv_event_t *e) { ui_go(SCR_FILES); }

/* оцветяване като в макета: G синьо, M оранжево, T жълто, коментарите бледо */
static void colorize(const char *src, char *out, size_t n, bool *plain)
{
    const char *s = src;
    while (*s == ' ') s++;
    if (*s == '(' || *s == ';' || !*s) { snprintf(out, n, "%s", src); *plain = true; return; }
    *plain = false;
    size_t o = 0;
    for (const char *p = src; *p && o + 24 < n;) {
        char c = *p;
        if ((c == 'G' || c == 'M' || c == 'T') && (p == src || p[-1] == ' ') && p[1] >= '0' && p[1] <= '9') {
            const char *col = c == 'G' ? "5ab4ff" : c == 'M' ? "ff9f43" : "f2b632";
            o += (size_t)snprintf(out + o, n - o, "#%s ", col);
            while (*p && *p != ' ' && o + 4 < n) out[o++] = *p++;
            out[o++] = '#';
            continue;
        }
        if (c == '#') { out[o++] = '#'; out[o++] = '#'; p++; continue; }   /* буквален # */
        out[o++] = *p++;
    }
    out[o] = 0;
}

static void fill(gcode_t *g)
{
    char buf[512], num[16];
    for (int i = 0; i < g->nrows; i++) {
        int li = g->first + i;
        bool show = li < P.nlines;
        lv_obj_set_hidden(g->row[i], !show);
        if (!show) continue;
        bool plain;
        colorize(P.line[li], buf, sizeof buf, &plain);
        lv_label_set_recolor(g->code[i], !plain);
        lv_obj_set_style_text_color(g->code[i], plain ? C_DIM : C_FG, 0);
        ui_set_text(g->code[i], buf);
        snprintf(num, sizeof num, "%d", li + 1);
        ui_set_text(g->num[i], num);
        bool cur = li == g->cur;
        lv_obj_set_style_bg_opa(g->row[i], cur ? 77 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(g->num[i], cur ? C_HI : C_FAINT, 0);
    }
}

static void build_rows(gcode_t *g)
{
    int32_t h = lv_obj_get_content_height(g->list);
    int n = h / g->row_h + 1;
    if (n > GC_MAX_ROWS) n = GC_MAX_ROWS;
    if (n == g->nrows) return;
    for (int i = n; i < g->nrows; i++) lv_obj_set_hidden(g->row[i], true);
    for (int i = g->nrows; i < n; i++) {
        if (!g->row[i]) {
            lv_obj_t *r = g->row[i] = ui_box(g->list);
            lv_obj_set_size(r, LV_PCT(100), g->row_h);
            lv_obj_set_pos(r, 0, i * g->row_h);
            lv_obj_set_style_bg_color(r, C_ACC, 0);
            lv_obj_t *nm = g->num[i] = ui_label(r, F.num_gc, C_FAINT, "");
            lv_obj_set_width(nm, EM(3.4f * 0.82f));
            lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(nm, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_t *c = g->code[i] = ui_label(r, F.num_gc, C_FG, "");
            lv_label_set_long_mode(c, LV_LABEL_LONG_MODE_CLIP);
            lv_obj_set_width(c, LV_PCT(80));
            lv_obj_align(c, LV_ALIGN_LEFT_MID, EM(3.4f * 0.82f) + EM(0.7f * 0.82f), 0);
        }
    }
    g->nrows = n;
    g->first = -1;
}

static void on_size(lv_event_t *e) { gcode_t *g = lv_event_get_user_data(e); build_rows(g); }

/* плъзгане при спряна машина превърта програмата */
static void on_drag(lv_event_t *e)
{
    gcode_t *g = lv_event_get_user_data(e);
    lv_point_t v;
    lv_indev_get_vect(lv_indev_active(), &v);
    g->scroll -= v.y;
    int max = (P.nlines - g->nrows + 1) * g->row_h;
    if (g->scroll < 0) g->scroll = 0;
    if (g->scroll > max) g->scroll = max > 0 ? max : 0;
}

void gcode_create(gcode_t *g, lv_obj_t *parent)
{
    memset(g, 0, sizeof *g);
    g->cur = -2;
    g->row_h = EM(1.7f * 0.82f);
    lv_obj_t *hr;
    g->card = ui_card(parent, "", &hr);
    /* името на файла е на мястото на заглавието */
    lv_obj_t *h = lv_obj_get_child(g->card, 0), *t = lv_obj_get_child(h, 0);
    lv_obj_set_style_text_font(t, F.num_md, 0);
    lv_obj_set_style_text_color(t, C_FG, 0);
    lv_obj_set_style_text_letter_space(t, 0, 0);
    g->name = t;
    g->pos = ui_label(hr, F.num_sm, C_DIM, "");
    lv_obj_add_event_cb(ui_btn(hr, "ОТВОРИ", BTN_NORMAL, true), on_open, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = g->list = ui_box(g->card);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_style_pad_ver(l, EM(0.3f), 0);
    lv_obj_set_clickable(l, true);
    lv_obj_add_event_cb(l, on_size, LV_EVENT_SIZE_CHANGED, g);
    lv_obj_add_event_cb(l, on_drag, LV_EVENT_PRESSING, g);
}

void gcode_update(gcode_t *g, uint32_t now)
{
    char buf[48];
    ui_set_text(g->name, P.name[0] ? P.name : "—");
    snprintf(buf, sizeof buf, "%d / %d", M.line >= 0 ? M.line + 1 : 0, P.nlines);
    ui_set_text(g->pos, buf);
    if (!g->nrows) return;
    int first;
    bool run = M.state == MS_RUN || M.state == MS_HOLD;
    if (run || (M.line >= 0 && g->scroll == 0)) {
        first = M.line - g->nrows / 3;
        g->scroll = 0;
    } else {
        first = g->scroll / g->row_h;
    }
    int max = P.nlines - g->nrows + 1;
    if (first > max) first = max;
    if (first < 0) first = 0;
    if (first != g->first || M.line != g->cur) {
        g->first = first;
        g->cur = M.line;
        fill(g);
    }
}

void gcode_reload(gcode_t *g)
{
    g->scroll = 0;
    g->first = -1;
    g->cur = -2;
}
