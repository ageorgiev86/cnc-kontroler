/* Екранът „Програми“ (.files в макета): файловете от паметта, USB и мрежата, 3D преглед на избрания,
 * „Зареди“ и „Изпълни от ред…“.
 * Памет = вградените примери + папката с програми (CNC_PROG_DIR или ~/cnc-programi).
 * USB = /media/<потребител>/<памет> и /media/<памет>. Мрежа = /mnt/cnc-net, когато дялът е включен. */
#include <dirent.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include "config.h"
#include "program.h"
#include "raster.h"
#include "ui.h"
#include "ui_style.h"

#define FILES_MAX 200
typedef struct { char name[260], path[600], date[24]; int demo; long size; } fent_t;
static fent_t FL[FILES_MAX];
static int nfl, sel, src;            /* src: 0 памет, 1 USB, 2 мрежа */
static char loaded[600];             /* пътят (или demo:N) на заредената програма */
static program_t PV;                 /* прегледът */
static char pv_key[600];
static lv_obj_t *seg[3], *list, *warn, *warn_txt, *pv_name, *pv_size, *pv, *info[6], *btns;

static void pv_render(void);
static const uint32_t PAL[] = {0x59aee6, 0x3dbb7e, 0xe47ab3, 0xff9447, 0xa78bfa, 0x4fd1c5, 0xd9e45a, 0xf07167};

static const char *prog_dir(void)
{
    static char d[300];
    const char *e = getenv("CNC_PROG_DIR"), *h = getenv("HOME");
    if (e && *e) snprintf(d, sizeof d, "%s", e);
    else snprintf(d, sizeof d, "%s/cnc-programi", h ? h : ".");
    return d;
}

static bool gcode_ext(const char *n)
{
    const char *x = strrchr(n, '.');
    if (!x) return false;
    static const char *const E[] = {".nc", ".ngc", ".gcode", ".tap", ".cnc", ".txt"};
    for (unsigned i = 0; i < sizeof E / sizeof E[0]; i++)
        if (!strcasecmp(x, E[i])) return true;
    return false;
}

static void add_dir(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && nfl < FILES_MAX) {
        if (e->d_name[0] == '.') continue;
        char p[600];
        snprintf(p, sizeof p, "%.330s/%.255s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (depth > 0) add_dir(p, depth - 1); continue; }
        if (!gcode_ext(e->d_name)) continue;
        fent_t *f = &FL[nfl++];
        snprintf(f->name, sizeof f->name, "%s", e->d_name);
        snprintf(f->path, sizeof f->path, "%s", p);
        f->demo = -1;
        f->size = (long)st.st_size;
        struct tm tm;
        localtime_r(&st.st_mtime, &tm);
        strftime(f->date, sizeof f->date, "%d.%m.%Y %H:%M", &tm);
    }
    closedir(d);
}

static bool net_ready(void)
{
    struct stat st;
    return cfg_num("net.shOn") && !stat("/mnt/cnc-net", &st) && S_ISDIR(st.st_mode);
}

static void scan(void)
{
    nfl = 0;
    if (src == 0) {
        const char *nm, *txt;
        static const char *const DATE[4] = {"28.10.2026 14:12", "29.10.2026 09:40", "30.10.2026 16:05", "31.10.2026 11:27"};
        for (int i = 0; (txt = program_demo_text(i, &nm)); i++) {
            fent_t *f = &FL[nfl++];
            snprintf(f->name, sizeof f->name, "%s", nm);
            snprintf(f->path, sizeof f->path, "demo:%d", i);
            snprintf(f->date, sizeof f->date, "%s", DATE[i]);
            f->demo = i;
            f->size = (long)strlen(txt);
        }
        add_dir(prog_dir(), 1);
    } else if (src == 1) {
        const char *u = getenv("USER");
        char d[200];
        snprintf(d, sizeof d, "/media/%s", u ? u : "");
        add_dir(d, 2);
        add_dir("/media", 2);
        add_dir("/mnt/usb", 1);
    } else if (net_ready()) {
        add_dir("/mnt/cnc-net", 2);
    }
    if (sel >= nfl) sel = 0;
}

static void fsize(long n, char *b, size_t sz)
{
    if (n < 1024) snprintf(b, sz, "%ld B", n);
    else if (n < 1048576) snprintf(b, sz, "%.1f KB", n / 1024.0);
    else snprintf(b, sz, "%.1f MB", n / 1048576.0);
}

static char *file_text(const fent_t *f)
{
    if (f->demo >= 0) { const char *t = program_demo_text(f->demo, NULL); return t ? strdup(t) : NULL; }
    return program_read_file(f->path, NULL);
}

/* ---------- преглед ---------- */
static void preview(void)
{
    if (!nfl) {
        pv_key[0] = 0;
        program_free_into(&PV);
        ui_set_text(pv_name, "—");
        ui_set_text(pv_size, "");
        for (int i = 0; i < 6; i++) ui_set_text(info[i], "");
        pv_render();
        return;
    }
    fent_t *f = &FL[sel];
    if (!strcmp(pv_key, f->path)) return;
    snprintf(pv_key, sizeof pv_key, "%s", f->path);
    char *t = file_text(f);
    if (t) { program_parse_into(&PV, f->name, t); free(t); }
    else program_free_into(&PV);
    ui_set_text(pv_name, f->name);
    char b[96];
    fsize(f->size, b, sizeof b);
    ui_set_text(pv_size, b);
    snprintf(b, sizeof b, "Редове  %d", PV.nlines);
    ui_set_text(info[0], b);
    char tl[64] = "";
    for (int i = 0; i < PV.ntools; i++) {
        char x[16];
        snprintf(x, sizeof x, "%s#%06x T%d#", i ? " " : "", (unsigned)PAL[i % 8], PV.tools[i]);
        strncat(tl, x, sizeof tl - strlen(tl) - 1);
    }
    snprintf(b, sizeof b, "Инструменти  %s", PV.ntools ? tl : "—");
    ui_set_text(info[1], b);
    int s = (int)lroundf(PV.t_total);
    snprintf(b, sizeof b, "Време (прибл.)  %d:%02d:%02d", s / 3600, s % 3600 / 60, s % 60);
    ui_set_text(info[2], b);
    snprintf(b, sizeof b, "X  %.1f … %.1f", PV.x0, PV.x1); ui_set_text(info[3], b);
    snprintf(b, sizeof b, "Y  %.1f … %.1f", PV.y0, PV.y1); ui_set_text(info[4], b);
    snprintf(b, sizeof b, "Z  %.1f … %.1f", PV.z0, PV.z1); ui_set_text(info[5], b);
    pv_render();
}

/* 3D изглед на прегледа: рисува се веднъж в буфер (бързо и при стотици хиляди редове) */
static lv_obj_t *pv_cv;
static lv_draw_buf_t *pv_buf;

static void pv_render(void)
{
    if (!pv_buf) return;
    lv_canvas_fill_bg(pv_cv, C_SURF, LV_OPA_COVER);
    if (PV.nop) {
        float W = (float)pv_buf->header.w, H = (float)pv_buf->header.h;
        float yaw = -(float)M_PI / 6, pitch = (float)M_PI / 3, cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
        float x0 = fminf(PV.x0, 0), x1 = fmaxf(PV.x1, 0), y0 = fminf(PV.y0, 0), y1 = fmaxf(PV.y1, 0);
        float cx = (x0 + x1) / 2, cyy = (y0 + y1) / 2, cz = (PV.z0 + PV.z1) / 2;
#define PRJ(X, Y, Z, A, B) do { float dx = (X) - cx, dy = (Y) - cyy, dz = (Z) - cz; float x1_ = dx * cy - dy * sy, y1_ = dx * sy + dy * cy; \
    A = x1_; B = -(y1_ * cp + dz * sp); } while (0)
        float mx = 1e-6f, my = 1e-6f;
        const float xs[2] = {x0, x1}, ys[2] = {y0, y1}, zs[2] = {PV.z0, PV.z1};
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) { float a, b; PRJ(xs[i], ys[j], zs[k], a, b); mx = fmaxf(mx, fabsf(a)); my = fmaxf(my, fabsf(b)); }
        float sc = fminf(W / 2 / mx, H / 2 / my) * 0.86f;
        int lw = EM(0.12f) > 1 ? EM(0.12f) : 1, dash = EM(0.25f) > 1 ? EM(0.25f) : 2;
        for (int i = 0; i < PV.nop; i++) {
            const op_t *o = &PV.op[i];
            if (o->kind != OP_MOVE) continue;
            int ti = 0;
            for (int k = 0; k < PV.ntools; k++) if (PV.tools[k] == o->tool) ti = k;
            lv_color_t c = o->rapid ? lv_color_hex(0x5b6f8f) : lv_color_hex(PAL[ti % 8]);
            int32_t px0 = INT32_MIN, py0 = 0;
            for (int k = o->p0; k < o->p0 + o->np; k++) {
                float a, b;
                PRJ(PV.pt[k].x, PV.pt[k].y, PV.pt[k].z, a, b);
                int32_t qx = (int32_t)(W / 2 + a * sc), qy = (int32_t)(H / 2 + b * sc);
                if (px0 != INT32_MIN && (qx != px0 || qy != py0)) raster_line(pv_buf, px0, py0, qx, qy, c, o->rapid ? 1 : lw, LV_OPA_COVER, o->rapid ? dash : 0);
                if (px0 == INT32_MIN || qx != px0 || qy != py0) { px0 = qx; py0 = qy; }
            }
        }
#undef PRJ
    }
    lv_obj_invalidate(pv_cv);
}

static void on_pv_size(lv_event_t *e)
{
    int32_t w = lv_obj_get_content_width(pv), h = lv_obj_get_content_height(pv);
    if (w < 16 || h < 16 || (pv_buf && (int32_t)pv_buf->header.w == w && (int32_t)pv_buf->header.h == h)) return;
    if (pv_buf) lv_draw_buf_destroy(pv_buf);
    pv_buf = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_NATIVE, 0);
    lv_canvas_set_draw_buf(pv_cv, pv_buf);
    pv_render();
}

/* ---------- списък ---------- */
static void render_list(void);
static void on_sel(lv_event_t *e) { sel = (int)(intptr_t)lv_event_get_user_data(e); render_list(); preview(); }
static void on_src(lv_event_t *e) { src = (int)(intptr_t)lv_event_get_user_data(e); sel = 0; files_show(); }
static void on_back(lv_event_t *e) { ui_go(SCR_RUN); }
static void on_net(lv_event_t *e) { ui_go(SCR_SET); set_test(M.acc_level ? M.acc_level : 0, "net"); }
static void on_upload(lv_event_t *e)
{
    char t[500];
    snprintf(t, sizeof t, "Копирайте файловете в %s (SCP/SFTP през SSH или мрежовия дял) и натиснете „Памет“.", prog_dir());
    machine_toast(t, lv_tick_get(), 7000);
}

static void on_load_impl(void);
static bool load_sel(void)
{
    uint32_t now = lv_tick_get();
    if (!nfl) return false;
    if (M.state == MS_RUN || M.state == MS_HOLD) { machine_toast("Спрете текущата програма, преди да заредите нова.", now, 3500); return false; }
    fent_t *f = &FL[sel];
    char *t = file_text(f);
    if (!t) { machine_toast("Файлът не може да бъде прочетен", now, 3500); return false; }
    program_load_text(f->name, t);
    free(t);
    snprintf(loaded, sizeof loaded, "%s", f->path);
    ui_program_changed();
    return true;
}
void files_load(void) { on_load_impl(); }
static void on_load(lv_event_t *e) { on_load_impl(); }
static void on_load_impl(void)
{
    if (!load_sel()) return;
    char t[160];
    snprintf(t, sizeof t, "Заредена %s", P.name);
    machine_toast(t, lv_tick_get(), 3500);
    ui_go(SCR_RUN);
}
static void on_from(lv_event_t *e) { if (load_sel()) ui_run_from_line(); }

static void render_list(void)
{
    lv_obj_clean(list);
    ui_seg_set(seg, 3, src);
    bool nw = src == 2 && !net_ready();
    lv_obj_set_hidden(warn, !nw);
    if (nw) {
        ui_set_text(warn_txt, cfg_num("net.shOn") ? "Мрежовият дял не е свързан (/mnt/cnc-net). Настройва се от Настройки → Мрежа."
                                                  : "Мрежовият дял е изключен. Настройва се от Настройки → Мрежа.");
        return;
    }
    if (!nfl) {
        lv_obj_t *l = ui_label(list, F.txt, C_DIM, src == 1 ? "Няма USB памет с програми." : "Няма файлове.");
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
        return;
    }
    for (int i = 0; i < nfl; i++) {
        lv_obj_t *b = ui_btn(list, "", i == sel ? BTN_ON : BTN_NORMAL, false);
        lv_obj_delete(ui_btn_label(b));
        lv_obj_set_size(b, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(b, EM(0.45f), 0);
        ui_flex(b, LV_FLEX_FLOW_COLUMN, 0.1f);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t *r = ui_box(b);
        ui_flex(r, LV_FLEX_FLOW_ROW, 0.4f);
        lv_obj_t *n = ui_label(r, F.num_md, C_FG, FL[i].name);
        lv_obj_set_style_text_letter_space(n, 0, 0);
        if (!strcmp(FL[i].path, loaded)) {
            lv_obj_t *bd = ui_label(r, F.num_sm, C_ACC_INK, "заредена");
            lv_obj_set_style_bg_color(bd, C_ACC, 0);
            lv_obj_set_style_bg_opa(bd, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_hor(bd, EM(0.35f), 0);
            lv_obj_set_style_radius(bd, EM(0.25f), 0);
        }
        char s[64], z[24];
        fsize(FL[i].size, z, sizeof z);
        snprintf(s, sizeof s, "%s · %s", z, FL[i].date);
        lv_obj_t *sm = ui_label(b, F.num_sm, C_DIM, s);
        lv_obj_set_style_text_letter_space(sm, 0, 0);
        lv_obj_add_event_cb(b, on_sel, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

void files_show(void)
{
    if (!loaded[0] && P.nop) snprintf(loaded, sizeof loaded, "demo:0");   /* демото е заредено при старта */
    scan();
    render_list();
    pv_key[0] = 0;
    preview();
}

void files_create(lv_obj_t *scr)
{
    static int32_t cols[3], rows[3];
    if (g_portrait) {
        cols[0] = LV_GRID_FR(1); cols[1] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_FR(85); rows[1] = LV_GRID_FR(115); rows[2] = LV_GRID_TEMPLATE_LAST;
    } else {
        cols[0] = LV_GRID_FR(38); cols[1] = LV_GRID_FR(62); cols[2] = LV_GRID_TEMPLATE_LAST;
        rows[0] = LV_GRID_FR(1); rows[1] = LV_GRID_TEMPLATE_LAST;
    }
    ui_grid(scr, cols, rows, 0.6f);
    lv_obj_t *hr;
    lv_obj_t *c = ui_card(scr, "ПРОГРАМИ", &hr);
    ui_cell(c, 0, 1, 0, 1);
    lv_obj_add_event_cb(ui_btn(hr, "НАЗАД", BTN_NORMAL, true), on_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *b = ui_body(c, 0.5f);
    lv_obj_t *s = ui_box(b);
    lv_obj_set_width(s, LV_PCT(100));
    ui_flex(s, LV_FLEX_FLOW_ROW, 0.25f);
    static const char *const SN[3] = {"ПАМЕТ", "USB", "МРЕЖА"};
    for (int i = 0; i < 3; i++) {
        seg[i] = ui_btn(s, SN[i], i == 0 ? BTN_ON : BTN_NORMAL, true);
        lv_obj_set_flex_grow(seg[i], 1);
        lv_obj_add_event_cb(seg[i], on_src, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    warn = ui_box(b);
    lv_obj_set_width(warn, LV_PCT(100));
    ui_flex(warn, LV_FLEX_FLOW_COLUMN, 0.5f);
    lv_obj_set_flex_align(warn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    warn_txt = ui_label(warn, F.txt, C_WARN, "");
    lv_obj_set_width(warn_txt, LV_PCT(100));
    lv_label_set_long_mode(warn_txt, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_add_event_cb(ui_btn(warn, "КЪМ МРЕЖОВИТЕ НАСТРОЙКИ", BTN_ON, false), on_net, LV_EVENT_CLICKED, NULL);
    list = ui_box(b);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_scrollable(list, true);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    ui_flex(list, LV_FLEX_FLOW_COLUMN, 0.3f);
    lv_obj_t *up = ui_btn(b, "КАЧИ ФАЙЛ ОТ КОМПЮТЪРА", BTN_NORMAL, false);
    lv_obj_set_width(up, LV_PCT(100));
    lv_obj_add_event_cb(up, on_upload, LV_EVENT_CLICKED, NULL);

    lv_obj_t *p = ui_card(scr, "", &hr);
    if (g_portrait) ui_cell(p, 0, 1, 1, 1); else ui_cell(p, 1, 1, 0, 1);
    pv_name = lv_obj_get_child(lv_obj_get_child(p, 0), 0);
    lv_obj_set_style_text_font(pv_name, F.num_md, 0);
    lv_obj_set_style_text_color(pv_name, C_FG, 0);
    lv_obj_set_style_text_letter_space(pv_name, 0, 0);
    pv_size = ui_label(hr, F.num_sm, C_DIM, "");
    pv = ui_box(p);
    lv_obj_set_width(pv, LV_PCT(100));
    lv_obj_set_flex_grow(pv, 1);
    pv_cv = lv_canvas_create(pv);
    lv_obj_add_event_cb(pv, on_pv_size, LV_EVENT_SIZE_CHANGED, NULL);
    static int32_t c3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, r2[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *ig = ui_box(p);
    lv_obj_set_width(ig, LV_PCT(100));
    lv_obj_set_style_pad_hor(ig, EM(0.75f), 0);
    lv_obj_set_style_pad_ver(ig, EM(0.5f), 0);
    lv_obj_set_style_border_side(ig, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(ig, C_LINE, 0);
    lv_obj_set_style_border_width(ig, 1, 0);
    ui_grid(ig, c3, r2, 0.35f);
    for (int i = 0; i < 6; i++) {
        info[i] = ui_label(ig, F.num_sm, C_FG, "");
        lv_label_set_recolor(info[i], true);
        lv_obj_set_grid_cell(info[i], LV_GRID_ALIGN_START, i % 3, 1, LV_GRID_ALIGN_CENTER, i / 3, 1);
    }
    btns = ui_box(p);
    lv_obj_set_width(btns, LV_PCT(100));
    lv_obj_set_style_pad_all(btns, EM(0.6f), 0);
    ui_flex(btns, LV_FLEX_FLOW_ROW, 0.5f);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(ui_btn(btns, "ИЗПЪЛНИ ОТ РЕД…", BTN_NORMAL, false), on_from, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ld = ui_btn(btns, "ЗАРЕДИ", BTN_GO, false);
    lv_obj_set_style_min_width(ld, EM(10), 0);
    lv_obj_add_event_cb(ld, on_load, LV_EVENT_CLICKED, NULL);
}

void files_update(uint32_t now) { (void)now; }

/* за снимките от симулатора */
void files_test(int s, int i)
{
    src = s;
    sel = i;
    ui_go(SCR_FILES);
    files_show();
}

/* при излизане от екрана: прегледът се освобождава (при голяма програма това са десетки MB) */
void files_hide(void)
{
    program_free_into(&PV);
    pv_key[0] = 0;
}
