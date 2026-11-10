/* Картата с програмата: име, „ред / общо“, „Отвори“ и G-code списъкът (.gc-list в макета).
 * Списъкът е виртуален: обекти има само за видимите редове, текстът им се сменя при превъртане,
 * затова и програма с десетки хиляди редове не тежи. По време на работа текущият ред е в горната трета. */
#include <stdio.h>
#include <string.h>

#include "program.h"
#include "ui.h"
#include "ui_style.h"

static void on_open(lv_event_t *e) { ui_go(SCR_FILES); }

/* ---------- редакция на програмата (бутонът „Редактирай“, после докосване на ред) ---------- */
static gcode_t *ED;
static int ed_line;
static bool ed_ins;   /* редът е нов: добавя се чак при „Готово“ */
static bool ed_now;   /* програмата се презарежда заради редакция */
static const char *const ED_HINT = "Промяната важи веднага за заредената програма. „Запиши“ я записва в паметта; файлът на USB или в мрежата не се променя.";

static void ed_open(void);

static void ed_apply(const char *txt)
{
    int top = ED->scroll;
    if (ed_ins) program_edit(ed_line, PE_INSERT, txt);
    else if (ed_line < P.nlines && strcmp(P.line[ed_line], txt)) program_edit(ed_line, PE_REPLACE, txt);
    else return;
    ed_now = true;
    ui_program_changed();
    ed_now = false;
    ED->scroll = top;
    ED->dirty = true;
    lv_obj_set_hidden(ED->save, false);
}

static int ed_ok(const char *txt, void *ud) { ed_apply(txt); return 0; }

static void ed_line_fn(int act, const char *txt, void *ud)
{
    if (act == 0) {   /* изтрий реда */
        if (ed_ins) return;
        int top = ED->scroll;
        program_edit(ed_line, PE_DELETE, NULL);
        ed_now = true;
        ui_program_changed();
        ed_now = false;
        ED->scroll = top;
        ED->dirty = true;
        lv_obj_set_hidden(ED->save, false);
        return;
    }
    ed_apply(txt);   /* нов ред след: първо се приема текущият */
    ed_line++;
    ed_ins = true;
    ed_open();
}

static void ed_open(void)
{
    char t[48];
    snprintf(t, sizeof t, ed_ins ? "Нов ред %d" : "Ред %d", ed_line + 1);
    dlg_text_line(t, ed_ins ? "" : P.line[ed_line], ED_HINT, ed_ok, ed_line_fn, NULL);
}

static void edit_set(gcode_t *g, bool on)
{
    g->editing = on;
    ui_btn_set_kind(g->edit, on ? BTN_ON : BTN_NORMAL);
}

static void on_edit(lv_event_t *e)
{
    gcode_t *g = lv_event_get_user_data(e);
    uint32_t now = lv_tick_get();
    if (!P.nlines) { machine_toast("Няма заредена програма", now, 3000); return; }
    if (M.state != MS_IDLE) { machine_toast("Програмата се редактира, когато машината е спряна", now, 3500); return; }
    edit_set(g, !g->editing);
    if (g->editing) machine_toast("Докоснете ред, за да го промените", now, 3000);
}

static void on_save_impl(gcode_t *g)
{
    char t[160];
    if (files_save_loaded() == 0) {
        g->dirty = false;
        lv_obj_set_hidden(g->save, true);
        snprintf(t, sizeof t, "Записана в паметта: %s", P.name);
    } else snprintf(t, sizeof t, "Програмата не може да бъде записана");
    machine_toast(t, lv_tick_get(), 4000);
}

static void on_save(lv_event_t *e) { on_save_impl(lv_event_get_user_data(e)); }

static void on_press(lv_event_t *e) { gcode_t *g = lv_event_get_user_data(e); g->drag = 0; }

static void on_tap(lv_event_t *e)
{
    gcode_t *g = lv_event_get_user_data(e);
    if (!g->editing || g->drag > g->row_h / 2) return;
    if (M.state != MS_IDLE) { edit_set(g, false); return; }
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_area_t a;
    lv_obj_get_content_coords(g->list, &a);
    int i = (p.y - a.y1) / g->row_h, li = g->first + i;
    if (i < 0 || i >= g->nrows || li < 0 || li >= P.nlines) return;
    ED = g;
    ed_line = li;
    ed_ins = false;
    ed_open();
}

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
    g->drag += v.y < 0 ? -v.y : v.y;
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
    g->edit = ui_btn(hr, "РЕДАКТИРАЙ", BTN_NORMAL, true);
    lv_obj_add_event_cb(g->edit, on_edit, LV_EVENT_CLICKED, g);
    g->save = ui_btn(hr, "ЗАПИШИ", BTN_GO, true);
    lv_obj_add_event_cb(g->save, on_save, LV_EVENT_CLICKED, g);
    lv_obj_set_hidden(g->save, true);
    lv_obj_add_event_cb(ui_btn(hr, "ОТВОРИ", BTN_NORMAL, true), on_open, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = g->list = ui_box(g->card);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_style_pad_ver(l, EM(0.3f), 0);
    lv_obj_set_clickable(l, true);
    lv_obj_add_event_cb(l, on_size, LV_EVENT_SIZE_CHANGED, g);
    lv_obj_add_event_cb(l, on_drag, LV_EVENT_PRESSING, g);
    lv_obj_add_event_cb(l, on_press, LV_EVENT_PRESSED, g);
    lv_obj_add_event_cb(l, on_tap, LV_EVENT_CLICKED, g);
}

void gcode_update(gcode_t *g, uint32_t now)
{
    char buf[48];
    ui_set_text(g->name, P.name[0] ? P.name : "—");
    snprintf(buf, sizeof buf, "%d / %d", M.line >= 0 ? M.line + 1 : 0, P.nlines);
    if (g->editing && M.state != MS_IDLE) edit_set(g, false);
    lv_obj_set_hidden(g->pos, g->editing || g->dirty);   /* мястото остава за името на файла */
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
    if (!ed_now) {   /* нова програма (не редакция): режимът и „Запиши“ се нулират */
        g->dirty = false;
        if (g->save) lv_obj_set_hidden(g->save, true);
        if (g->edit) edit_set(g, false);
    }
    g->scroll = 0;
    g->first = -1;
    g->cur = -2;
}

/* тестове от симулатора: act -1 = отваря ред line (от 1) за редакция; 0/1 = бутоните на диалога; 2 = „Запиши“ */
void gcode_test(gcode_t *g, int line, int act)
{
    if (act == 2) { if (g->dirty) on_save_impl(g); return; }
    if (act >= 0) {
        if (ED != g) return;
        dlg_close();
        ed_line_fn(act, ed_ins || ed_line >= P.nlines ? "" : P.line[ed_line], NULL);
        return;
    }
    if (line < 1 || line > P.nlines || M.state != MS_IDLE) return;
    edit_set(g, true);
    ED = g;
    ed_line = line - 1;
    ed_ins = false;
    ed_open();
}
