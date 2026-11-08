/* Основният екран „Работа“ (по ui/maket.html):
 *   горна лента
 *   позиции | програма (G-code)
 *   траектория | програма
 *   оверрайди и напредък
 *   бутоните на цикъла
 *   навигация между екраните */
#include "ui.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "program.h"
#include "ui_style.h"

static topbar_t tb;
static dro_t dro;
static traj_t traj;
static gcode_t gc;
static ctrl_t ctrl;

int g_screen = SCR_RUN;
static lv_obj_t *scr_box[SCR_N], *nav_btn[7];
static struct { bool has; int col, cspan, row, rspan; } tslot[SCR_N];
static const int NAV_SCR[7] = {SCR_RUN, SCR_JOG, SCR_MDI, SCR_TOOLS, SCR_PROBE, SCR_DIAG, SCR_SET};

#define UPDATE_MS 50

lv_obj_t *ui_traj_card(void) { return traj.card; }

static void on_line(double v, void *ud)
{
    int n = (int)lround(v), k = 0;
    while (k < P.nop - 1 && (P.op[k].line < n - 1 || P.op[k].kind != OP_MOVE)) k++;
    /* инструментът и шпинделът преди реда се възстановяват (последният OP_TOOL/OP_SPINDLE преди него) */
    int tool = -1, sp = -1;
    for (int i = 0; i < k; i++) {
        if (P.op[i].kind == OP_TOOL) tool = P.op[i].tool;
        if (P.op[i].kind == OP_SPINDLE) sp = P.op[i].s;
    }
    if (tool > 0 && tool != M.tool) machine_select_tool(tool);
    if (sp >= 0) { M.sp_cmd = sp; M.sp_on = sp > 0; }
    M.op = 0; M.prog_done = false;
    M.start_op = k;
    M.start_line = n;
    M.line = P.op[k].line;
    char t[96];
    snprintf(t, sizeof t, "Старт от ред %d с ЦИКЪЛ СТАРТ (T%d, S%d)", n, M.tool, M.sp_cmd);
    machine_toast(t, lv_tick_get(), 5000);
    ui_go(SCR_RUN);
}

void ui_run_from_line(void)
{
    uint32_t now = lv_tick_get();
    if (M.state != MS_IDLE) { machine_toast(M.state == MS_HOLD ? "Програмата е на пауза. Натиснете ЦИКЪЛ СТАРТ или СТОП." : "Машината работи", now, 3500); return; }
    if (!P.nop) { machine_toast("Няма заредена програма", now, 3000); return; }
    dlg_num("Изпълни от ред", "", "Преди този ред се възстановяват инструментът и оборотите; машината отива направо до началото на хода.",
            M.line >= 0 ? M.line + 1 : 1, 1, P.nlines, 0, on_line, NULL);
}

void ui_program_changed(void)
{
    M.op = 0; M.op_s = 0; M.line = -1; M.prog_done = false; M.done_t = 0; M.run_time = 0; M.t_total = 0;
    traj_reload(&traj);
    gcode_reload(&gc);
}

void ui_traj_slot(int scr, int col, int cspan, int row, int rspan)
{
    tslot[scr].has = true;
    tslot[scr].col = col; tslot[scr].cspan = cspan;
    tslot[scr].row = row; tslot[scr].rspan = rspan;
}

/* надписите на навигацията: ако не се побират, по-дребен шрифт и без разредка */
static void nav_fit(void)
{
    lv_obj_update_layout(lv_screen_active());
    for (int i = 0; i < 7; i++) {
        lv_obj_t *b = nav_btn[i], *l = ui_btn_label(b);
        const lv_font_t *f[3] = {lv_obj_get_style_text_font(b, 0), F.ui_sm, F.ui_xs};
        for (int k = 0; k < 3; k++) {
            int32_t ls = k == 0 ? lv_obj_get_style_text_letter_space(b, 0) : 0;
            lv_obj_set_style_text_font(b, f[k], 0);
            lv_obj_set_style_text_letter_space(b, ls, 0);
            lv_obj_update_layout(b);
            if (lv_obj_get_width(l) <= lv_obj_get_content_width(b)) break;
        }
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
    }
}

static void update_cb(lv_timer_t *t)
{
    static uint32_t last;
    static bool fitted;
    if (!fitted) { nav_fit(); fitted = true; }
    uint32_t now = lv_tick_get();
    machine_tick(now, last ? now - last : 0);
    last = now;
    topbar_update(&tb, now);
    switch (g_screen) {
    case SCR_RUN:
        dro_update(&dro, now);
        gcode_update(&gc, now);
        ctrl_update(&ctrl, now);
        break;
    case SCR_JOG: jog_update(now); break;
    case SCR_MDI: mdi_update(now); break;
    case SCR_TOOLS: tools_update(now); break;
    case SCR_PROBE: probe_update(now); break;
    case SCR_DIAG: diag_update(now); break;
    case SCR_SET: set_update(now); break;
    case SCR_FILES: files_update(now); break;
    }
    if (tslot[g_screen].has) traj_update(&traj, now);
}

void ui_go(int s)
{
    if (s < 0 || s >= SCR_N) return;
    if (tslot[s].has && s != g_screen) {
        lv_obj_set_parent(traj.card, scr_box[s]);
        lv_obj_set_grid_cell(traj.card, LV_GRID_ALIGN_STRETCH, tslot[s].col, tslot[s].cspan, LV_GRID_ALIGN_STRETCH, tslot[s].row, tslot[s].rspan);
        /* в портрет заглавието се скрива само в „Работа“ (.ttl в макета) */
        lv_obj_set_hidden(lv_obj_get_child(lv_obj_get_child(traj.card, 0), 0), g_portrait && s == SCR_RUN);
    }
    if (g_screen == SCR_FILES && s != SCR_FILES) files_hide();
    for (int i = 0; i < SCR_N; i++) lv_obj_set_hidden(scr_box[i], i != s);
    for (int i = 0; i < 7; i++) ui_btn_set_kind(nav_btn[i], NAV_SCR[i] == s ? BTN_ON : BTN_NORMAL);
    g_screen = s;
    if (s == SCR_FILES) files_show();
    if (s == SCR_SET) set_show();
    update_cb(NULL);
}

static void on_nav(lv_event_t *e) { ui_go((int)(intptr_t)lv_event_get_user_data(e)); }

/* навигация (.navbar) */
static void navbar_create(lv_obj_t *parent)
{
    lv_obj_t *n = ui_box(parent);
    lv_obj_set_width(n, LV_PCT(100));
    lv_obj_set_style_bg_color(n, C_PANEL, 0);
    lv_obj_set_style_bg_opa(n, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(n, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(n, C_EDGE2, 0);
    lv_obj_set_style_border_width(n, 1, 0);
    lv_obj_set_style_pad_ver(n, EM(0.45f), 0);
    lv_obj_set_style_pad_hor(n, EM(0.6f), 0);
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(n, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(n, cols, rows);
    lv_obj_set_style_pad_column(n, EM(0.35f), 0);
    static const char *const NAV[7] = {"РАБОТА", "РЪЧНО", "MDI", "ИНСТРУМЕНТИ", "ОФСЕТИ", "ДИАГНОСТИКА", "НАСТРОЙКИ"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t *b = nav_btn[i] = ui_btn(n, NAV[i], i == 0 ? BTN_ON : BTN_NORMAL, false);
        lv_obj_set_height(b, EM(3.3f));
        lv_obj_set_style_text_font(b, g_portrait ? F.ui_sm : F.ui_md, 0);
        lv_obj_set_style_text_letter_space(b, EM(g_portrait ? 0.02f : 0.07f), 0);
        lv_obj_set_style_pad_hor(b, EM(0.2f), 0);
        lv_obj_add_event_cb(b, on_nav, LV_EVENT_CLICKED, (void *)(intptr_t)NAV_SCR[i]);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
}

/* екранът „Работа“ (.screens + .run): две колони (63/37, в портрет 57/43), четири реда */
static void run_create(lv_obj_t *run)
{
    static int32_t cols[3], rows[5];
    cols[0] = LV_GRID_FR((g_portrait ? 57 : 63));  /* LV_GRID_FR не слага скоби */
    cols[1] = LV_GRID_FR((g_portrait ? 43 : 37));
    cols[2] = LV_GRID_TEMPLATE_LAST;
    rows[0] = LV_GRID_CONTENT;
    rows[1] = LV_GRID_FR(1);
    rows[2] = LV_GRID_CONTENT;
    rows[3] = LV_GRID_CONTENT;
    rows[4] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_layout(run, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(run, cols, rows);

    dro_create(&dro, run, DRO_FULL);
    lv_obj_set_grid_cell(dro.card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);

    traj_create(&traj, run);
    ui_traj_slot(SCR_RUN, 0, 1, 1, 1);
    lv_obj_set_grid_cell(traj.card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    gcode_create(&gc, run);
    lv_obj_set_grid_cell(gc.card, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 2);

    lv_obj_t *c = ctrl_create(&ctrl, run);
    lv_obj_set_grid_cell(c, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 2, 1);

    lv_obj_t *cb = cbtns_create(run);
    lv_obj_set_grid_cell(cb, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 3, 1);
}

void ui_create(lv_display_t *disp)
{
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    ui_style_init(w, h);
    machine_init();
    if (!P.nop) program_load_demo();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_style_all(scr);
    lv_obj_set_scrollable(scr, false);
    lv_obj_set_style_bg_color(scr, C_PANEL, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, C_FG, 0);
    lv_obj_set_style_text_font(scr, F.ui_sm, 0);
    ui_flex(scr, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    topbar_create(&tb, scr);

    lv_obj_t *screens = ui_box(scr);
    lv_obj_set_width(screens, LV_PCT(100));
    lv_obj_set_flex_grow(screens, 1);
    for (int i = 0; i < SCR_N; i++) {
        lv_obj_t *b = scr_box[i] = ui_box(screens);
        lv_obj_set_size(b, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_pad_all(b, EM(0.6f), 0);
        lv_obj_set_style_pad_gap(b, EM(0.6f), 0);
        lv_obj_set_hidden(b, i != SCR_RUN);
    }
    run_create(scr_box[SCR_RUN]);
    jog_create(scr_box[SCR_JOG]);
    mdi_create(scr_box[SCR_MDI]);
    tools_create(scr_box[SCR_TOOLS]);
    probe_create(scr_box[SCR_PROBE]);
    diag_create(scr_box[SCR_DIAG]);
    set_create(scr_box[SCR_SET]);
    files_create(scr_box[SCR_FILES]);

    navbar_create(scr);

    update_cb(NULL);
    lv_timer_create(update_cb, UPDATE_MS, NULL);
}

/* Настройките се пазят в $CNC_UI_CONF или ~/.config/cnc-ui.conf (записва ги „Запиши“) */
const char *ui_config_path(void)
{
    static char p[400];
    const char *e = getenv("CNC_UI_CONF"), *h = getenv("HOME");
    if (e && *e) snprintf(p, sizeof p, "%s", e);
    else {
        snprintf(p, sizeof p, "%s/.config", h ? h : ".");
        mkdir(p, 0755);
        snprintf(p, sizeof p, "%s/.config/cnc-ui.conf", h ? h : ".");
    }
    return p;
}

void ui_load_config(const char *path)
{
    char p[400];
    if (!path) {
        const char *e = getenv("CNC_UI_CONF"), *h = getenv("HOME");
        if (e && *e) snprintf(p, sizeof p, "%s", e);
        else {
            snprintf(p, sizeof p, "%s/.config", h ? h : ".");
            mkdir(p, 0755);
            snprintf(p, sizeof p, "%s/.config/cnc-ui.conf", h ? h : ".");
        }
        path = p;
    }
    cfg_set_file(path);
    machine_apply_cfg();
}

/* ---------- физическа клавиатура ----------
 * Клавишите стигат до невидим обект в група (LV_EVENT_KEY), а оттам до ui_key:
 * диалог > MDI > преки пътища (F1–F7 екраните, в „Ръчно“ стрелките и Page Up/Down движат осите). */
#define KEY_F(n) (0xF000u + (n))   /* F1–F12 от драйвера на клавиатурата */

void ui_key(uint32_t key)
{
    uint32_t now = lv_tick_get();
    if (dlg_key(key)) return;
    if (key >= KEY_F(1) && key <= KEY_F(7)) {
        static const int S[7] = {SCR_RUN, SCR_JOG, SCR_MDI, SCR_TOOLS, SCR_PROBE, SCR_DIAG, SCR_SET};
        ui_go(S[key - KEY_F(1)]);
        return;
    }
    if (g_screen == SCR_MDI && mdi_key(key)) return;
    if (g_screen == SCR_JOG) {
        int ax = -1, dir = 0;
        if (key == LV_KEY_LEFT) ax = AX_X, dir = -1;
        else if (key == LV_KEY_RIGHT) ax = AX_X, dir = 1;
        else if (key == LV_KEY_UP) ax = AX_Y, dir = 1;
        else if (key == LV_KEY_DOWN) ax = AX_Y, dir = -1;
        else if (key == LV_KEY_PREV || key == KEY_F(20)) ax = AX_Z, dir = 1;    /* Page Up */
        else if (key == LV_KEY_NEXT || key == KEY_F(21)) ax = AX_Z, dir = -1;   /* Page Down */
        if (ax >= 0) {
            double inc = M.jog_inc;
            if (inc <= 0) M.jog_inc = 1;   /* при „Непрек.“ клавишът дава стъпка 1 мм */
            if (M.state == MS_IDLE) machine_jog(ax, dir, now);
            M.jog_inc = inc;
            return;
        }
    }
}

static void on_catch_key(lv_event_t *e) { ui_key(lv_event_get_key(e)); }

void ui_keys_attach(lv_indev_t *kb)
{
    static lv_group_t *g;
    if (!g) {
        g = lv_group_create();
        lv_obj_t *c = ui_box(lv_layer_sys());   /* невидим получател на клавишите */
        lv_obj_set_size(c, 1, 1);
        lv_obj_add_event_cb(c, on_catch_key, LV_EVENT_KEY, NULL);
        lv_group_add_obj(g, c);
        lv_group_focus_obj(c);
    }
    lv_indev_set_group(kb, g);
}

static const char *const SCR_NAME[SCR_N] = {"run", "jog", "mdi", "tools", "probe", "diag", "set", "files"};

int ui_go_name(const char *name)
{
    for (int i = 0; i < SCR_N; i++)
        if (!strcmp(name, SCR_NAME[i])) { ui_go(i); return 0; }
    return -1;
}

/* Тестови действия за снимките от симулатора: име[:аргумент] */
/* Колелцето превърта областта под курсора (а не последната натисната, както прави LVGL): първата
 * област от ui_scroll_v() нагоре по дървото, която има накъде да се движи. При отворен
 * диалог курсорът е върху фона му и екранът отдолу не се превърта. */
/* най-вътрешният видим обект под точката (lv_indev_search_obj търси само такива, които се натискат) */
static lv_obj_t *obj_at(lv_obj_t *o, const lv_point_t *p)
{
    if (lv_obj_is_hidden(o)) return NULL;
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    if (p->x < a.x1 || p->x > a.x2 || p->y < a.y1 || p->y > a.y2) return NULL;
    for (int i = (int)lv_obj_get_child_count(o) - 1; i >= 0; i--) {
        lv_obj_t *r = obj_at(lv_obj_get_child(o, i), p);
        if (r) return r;
    }
    return o;
}

void ui_wheel(int32_t x, int32_t y, int notches)
{
    if (!notches) return;
    lv_display_t *d = lv_display_get_default();
    lv_point_t p = {x, y};
    lv_obj_t *top = lv_display_get_layer_top(d);
    lv_obj_t *o = obj_at(top, &p);
    if (!o || o == top) o = obj_at(lv_screen_active(), &p);   /* самият слой: няма диалог */
    for (; o; o = lv_obj_get_parent(o)) {
        /* само областите от ui_scroll_v (посока точно VER); картите по подразбиране са LV_DIR_ALL */
        if (!lv_obj_is_scrollable(o) || lv_obj_get_scroll_dir(o) != LV_DIR_VER) continue;
        int32_t room = notches > 0 ? lv_obj_get_scroll_top(o) : lv_obj_get_scroll_bottom(o);
        if (room <= 0) continue;
        lv_obj_scroll_by_bounded(o, 0, EM(5) * notches, LV_ANIM_ON);
        return;
    }
}

void ui_test(const char *act)
{
    uint32_t now = lv_tick_get();
    const char *arg = strchr(act, ':');
    arg = arg ? arg + 1 : "";
    if (!strncmp(act, "screen:", 7)) ui_go_name(arg);
    else if (!strncmp(act, "setpos", 6)) dlg_setpos(AX_X);
    else if (!strncmp(act, "mdi:", 4)) machine_mdi(arg, now);
    else if (!strncmp(act, "jog:", 4)) machine_jog(arg[0] - 'X', arg[1] == '-' ? -1 : 1, now);
    else if (!strncmp(act, "home", 4)) machine_home(-1, now);
    else if (!strncmp(act, "gozero", 6)) machine_goto_zero(now);
    else if (!strncmp(act, "start", 5)) machine_start(now);
    else if (!strncmp(act, "tc:", 3)) machine_tool_change(atoi(arg), now);
    else if (!strncmp(act, "measure", 7)) machine_measure_tool(now);
    else if (!strncmp(act, "cyc:", 4)) machine_probe_cycle(arg, 0, now);
    else if (!strncmp(act, "hmapscan", 8)) hmap_scan(now);
    else if (!strncmp(act, "hmap", 4)) hmap_open();
    else if (!strncmp(act, "set:", 4)) { ui_go(SCR_SET); set_test(atoi(arg), strchr(arg, ':') ? strchr(arg, ':') + 1 : "mach"); }
    else if (!strncmp(act, "edit:", 5)) set_test_edit(arg);
    else if (!strncmp(act, "setsave", 7)) set_test_save();
    else if (!strncmp(act, "dscroll:", 8)) diag_test_scroll(atoi(arg));
    else if (!strncmp(act, "wheel:", 6)) {   /* wheel:x,y,щраквания (− = надолу) */
        int x = 0, y = 0, n = 0;
        if (sscanf(arg, "%d,%d,%d", &x, &y, &n) == 3) ui_wheel(x, y, n);
    }
    else if (!strncmp(act, "load", 4)) files_load();
    else if (!strncmp(act, "keys:", 5)) for (const char *k = arg; *k; k++) ui_key(*k == '|' ? LV_KEY_ENTER : (uint32_t)*k);
    else if (!strncmp(act, "file:", 5)) files_test(arg[0] - '0', atoi(arg + 2));
    else if (!strncmp(act, "part:", 5)) machine_wcs_toggle(atoi(arg), now);
    else if (!strncmp(act, "cfg:", 4)) {   /* cfg:ключ=стойност — направо в записаните настройки */
        char k[64];
        snprintf(k, sizeof k, "%s", arg);
        char *eq = strchr(k, '=');
        if (eq) { *eq = 0; drf_set(k, eq + 1); cfg_commit_key(k); machine_apply_cfg(); }
    }
    else fprintf(stderr, "Непознато тестово действие: %s\n", act);
}

/* 32-битов BMP отгоре надолу; достатъчно за снимки от симулатора */
int ui_save_bmp(const char *path)
{
    lv_draw_buf_t *db = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_XRGB8888);
    if (!db) return -1;
    uint32_t w = db->header.w, h = db->header.h, stride = db->header.stride;
    /* диалозите са в горния слой: смесват се отгоре по алфа */
    if (lv_obj_get_child_count(lv_layer_top())) {
        lv_draw_buf_t *top = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888);
        if (top && top->header.w == w && top->header.h == h) {
            for (uint32_t y = 0; y < h; y++) {
                uint8_t *d = db->data + y * stride, *t = top->data + y * top->header.stride;
                for (uint32_t x = 0; x < w; x++, d += 4, t += 4)
                    for (int c = 0; c < 3; c++) d[c] = (uint8_t)((t[c] * t[3] + d[c] * (255 - t[3])) / 255);
            }
        }
        if (top) lv_draw_buf_destroy(top);
    }
    FILE *f = fopen(path, "wb");
    if (!f) { lv_draw_buf_destroy(db); return -1; }
    uint32_t img = w * h * 4, fsz = 54 + img;
    uint8_t hd[54] = {'B', 'M'};
#define PUT32(o, v) do { uint32_t _v = (uint32_t)(v); hd[o] = _v; hd[o + 1] = _v >> 8; hd[o + 2] = _v >> 16; hd[o + 3] = _v >> 24; } while (0)
    PUT32(2, fsz);
    PUT32(10, 54);
    PUT32(14, 40);
    PUT32(18, w);
    PUT32(22, -(int32_t)h);
    hd[26] = 1;
    hd[28] = 32;
    PUT32(34, img);
#undef PUT32
    fwrite(hd, 1, sizeof hd, f);
    for (uint32_t y = 0; y < h; y++) fwrite(db->data + y * stride, 1, w * 4, f);
    fclose(f);
    lv_draw_buf_destroy(db);
    return 0;
}
