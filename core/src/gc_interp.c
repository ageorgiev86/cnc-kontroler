/* Интерпретатор: блок → прости команди с машинни координати (стъпка 1, docs/interpretator-planer.md). */
#include "cnc/gcode.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ARC_TOL_ABS 0.002     /* мм: допустима разлика в радиуса в началото и в края */
#define ARC_TOL_REL 0.001

static const char *const AXN[GC_NAX] = {"X", "Y", "Z", "A", "B", "C"};
static const char AXL[GC_NAX] = {'X', 'Y', 'Z', 'A', 'B', 'C'};
const char *gc_axis_name(int ax) { return ax >= 0 && ax < GC_NAX ? AXN[ax] : "?"; }

static int fail(gc_err_t *e, int col, const char *fmt, ...)
{
    e->col = col;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);
    return -1;
}

/* G-код за съобщенията: 382 → „38.2“ */
static const char *gname(int g, char *buf)
{
    if (g % 10) sprintf(buf, "%d.%d", g / 10, g % 10);
    else sprintf(buf, "%d", g / 10);
    return buf;
}

/* ---------- модални групи ---------- */
enum { GR_NONE = -1, GR_0 = 0, GR_MOTION = 1, GR_PLANE = 2, GR_DIST = 3, GR_ARCDIST = 4, GR_FEEDMODE = 5,
       GR_UNITS = 6, GR_CUTTER = 7, GR_TLO = 8, GR_CYCLE = 9, GR_RETURN = 10, GR_WCS = 12, GR_PATH = 13, GR_N = 14 };

static int group_of(int g)
{
    switch (g) {
    case 0: case 10: case 20: case 30: return GR_MOTION;
    case 40: case 100: case 280: case 281: case 300: case 301: case 530: case 920: case 921: case 922: case 1030: return GR_0;
    case 170: case 180: case 190: return GR_PLANE;
    case 900: case 910: return GR_DIST;
    case 901: case 911: return GR_ARCDIST;
    case 940: return GR_FEEDMODE;
    case 200: case 210: return GR_UNITS;
    case 400: return GR_CUTTER;
    case 430: case 490: return GR_TLO;
    case 800: return GR_CYCLE;
    case 980: case 990: return GR_RETURN;
    case 540: case 541: case 550: case 560: case 570: case 580: case 590: case 1540: return GR_WCS;
    case 610: case 611: case 640: case 1870: return GR_PATH;
    default: return GR_NONE;
    }
}

/* Познат код, който идва в по-късна стъпка: краткото име на стъпката, иначе NULL */
static const char *later_g(int g)
{
    switch (g) {
    case 730: case 810: case 820: case 830: case 850: case 860: case 870: case 880: case 890:
        return "идва в стъпка 2 (цикли)";
    case 50: case 51: case 52: case 120: case 130: case 150: case 160: case 410: case 411: case 420: case 421:
    case 431: case 500: case 510: case 520: case 680: case 690: case 700: case 710: case 720: case 930: case 1500:
        return "идва в стъпка 5";
    case 382: case 383: case 384: case 385:
        return "идва в стъпка 6 (пробник)";
    case 330: case 331: case 740: case 760: case 840: case 950: case 960: case 970:
        return "идва със синхрона с енкодера на шпиндела";
    case 70: case 80:
        return "идва с екрана за струг";
    case 650: case 660: case 670:
        return "идва с макросите";
    default:
        return NULL;
    }
}

/* ---------- координати ---------- */
static double unit(const gc_interp_t *in, int ax)
{
    if (in->cfg.rotary >> ax & 1u) return 1;
    return in->inch ? 25.4 : 1;
}

static double offset(const gc_interp_t *in, int ax)
{
    double o = in->tab->wcs[in->wcs][ax] + in->g92[ax];
    if (ax == GC_Z && in->tlo_on) o += in->tlo;
    return o;
}

double gc_prog_pos(const gc_interp_t *in, int ax) { return in->pos[ax] - offset(in, ax); }

static void emit(gc_interp_t *in, gc_cmd_t *c, int line)
{
    c->line = line;
    if (in->cfg.emit) in->cfg.emit(in->cfg.ctx, c);
}

static void emit_simple(gc_interp_t *in, gc_kind_t k, int ival, double val, int line)
{
    gc_cmd_t c;
    memset(&c, 0, sizeof c);
    c.kind = k;
    c.ival = ival;
    c.val = val;
    emit(in, &c, line);
}

static void move_to(gc_interp_t *in, const double tgt[GC_NAX], bool rapid, int line)
{
    bool same = true;
    for (int a = 0; a < GC_NAX; a++)
        if (fabs(tgt[a] - in->pos[a]) > 1e-9) same = false;
    if (same) return;
    gc_cmd_t c;
    memset(&c, 0, sizeof c);
    c.kind = rapid ? GC_RAPID : GC_FEED;
    memcpy(c.tgt, tgt, sizeof c.tgt);
    c.feed = rapid ? 0 : in->feed;
    emit(in, &c, line);
    memcpy(in->pos, tgt, sizeof in->pos);
}

/* целта в машинни координати от думите на осите (програмни, G90/G91) */
static void axis_target(const gc_interp_t *in, const gc_block_t *b, double tgt[GC_NAX])
{
    for (int a = 0; a < GC_NAX; a++) {
        tgt[a] = in->pos[a];
        if (!GC_HAS(b, AXL[a])) continue;
        double v = GC_VAL(b, AXL[a]) * unit(in, a);
        double prog = in->incr ? gc_prog_pos(in, a) + v : v;
        tgt[a] = prog + offset(in, a);
    }
}

static bool has_axes(const gc_block_t *b)
{
    for (int a = 0; a < GC_NAX; a++)
        if (GC_HAS(b, AXL[a])) return true;
    return false;
}

/* ---------- инициализация ---------- */
void gc_tables_init(gc_tables_t *t) { memset(t, 0, sizeof *t); }

static void reset_modal(gc_interp_t *in)
{
    in->motion = -1;
    in->cycle = 0;
    in->plane = 17;
    in->incr = false;
    in->arc_abs = false;
    in->retract_r = false;
    in->wcs = 1;
    memset(in->g92, 0, sizeof in->g92);
    in->sp_dir = 0;
    in->cool = 0;
    in->path_mode = 640;
    in->path_tol = 0;
    in->ovr_en = true;
}

void gc_interp_init(gc_interp_t *in, const gc_config_t *cfg, gc_tables_t *tab, const double start[GC_NAX])
{
    memset(in, 0, sizeof *in);
    in->cfg = *cfg;
    in->tab = tab;
    reset_modal(in);
    in->inch = false;
    in->tlo_on = false;
    in->feed = 0;
    in->s = 0;
    in->tool = 0;
    in->tool_next = -1;
    if (start) memcpy(in->pos, start, sizeof in->pos);
}

/* ---------- дъга ---------- */
static int do_arc(gc_interp_t *in, const gc_block_t *b, bool cw, int line, gc_err_t *e)
{
    int a0, a1, lin;
    char o0, o1;
    switch (in->plane) {
    case 18: a0 = GC_Z; a1 = GC_X; lin = GC_Y; o0 = 'K'; o1 = 'I'; break;
    case 19: a0 = GC_Y; a1 = GC_Z; lin = GC_X; o0 = 'J'; o1 = 'K'; break;
    default: a0 = GC_X; a1 = GC_Y; lin = GC_Z; o0 = 'I'; o1 = 'J'; break;
    }
    if (in->feed <= 0) return fail(e, -1, "Липсва подаване F за дъгата");
    double tgt[GC_NAX];
    axis_target(in, b, tgt);
    double s0 = gc_prog_pos(in, a0), s1 = gc_prog_pos(in, a1);
    double e0 = tgt[a0] - offset(in, a0), e1 = tgt[a1] - offset(in, a1);
    bool hr = GC_HAS(b, 'R'), ho = GC_HAS(b, o0) || GC_HAS(b, o1);
    double c0, c1;
    if (hr && ho) return fail(e, b->col['R' - 'A'], "Дъгата има едновременно R и %c/%c", o0, o1);
    if (!hr && !ho) return fail(e, -1, "Дъгата няма нито %c/%c, нито R", o0, o1);
    if (hr) {
        double R = GC_VAL(b, 'R') * unit(in, a0);
        double dx = e0 - s0, dy = e1 - s1, d = hypot(dx, dy);
        if (d < 1e-9) return fail(e, b->col['R' - 'A'], "С R не може да се опише пълна окръжност (използвайте %c/%c)", o0, o1);
        if (fabs(R) < 1e-9) return fail(e, b->col['R' - 'A'], "Радиусът R е нула");
        double hh = R * R - d * d / 4;
        if (hh < -1e-6 * R * R && d - 2 * fabs(R) > ARC_TOL_ABS)
            return fail(e, b->col['R' - 'A'], "Радиусът R%g е по-малък от половината разстояние до края (%.4f мм)", GC_VAL(b, 'R'), d / 2);
        double h = sqrt(fmax(0, hh)), sg = (cw ? -1 : 1) * (R > 0 ? 1 : -1);
        c0 = (s0 + e0) / 2 - sg * h * dy / d;
        c1 = (s1 + e1) / 2 + sg * h * dx / d;
    } else {
        double i0 = GC_HAS(b, o0) ? GC_VAL(b, o0) * unit(in, a0) : 0;
        double i1 = GC_HAS(b, o1) ? GC_VAL(b, o1) * unit(in, a1) : 0;
        if (in->arc_abs && in->cfg.dialect != GC_DIALECT_HAAS) { c0 = i0; c1 = i1; }
        else { c0 = s0 + i0; c1 = s1 + i1; }
        double r0 = hypot(s0 - c0, s1 - c1), r1 = hypot(e0 - c0, e1 - c1);
        if (r0 < 1e-6) return fail(e, -1, "Радиусът на дъгата е нула");
        double dr = fabs(r1 - r0);
        if (dr > ARC_TOL_ABS && dr > ARC_TOL_REL * r0)
            return fail(e, -1, "Радиусът в края на дъгата (%.4f) се различава от началния (%.4f) с %.4f мм", r1, r0, dr);
    }
    int turns = 0;
    if (GC_HAS(b, 'P')) {
        double p = GC_VAL(b, 'P');
        if (p < 1 || fabs(p - lround(p)) > 1e-9) return fail(e, b->col['P' - 'A'], "P в дъгата трябва да е цял брой обороти ≥ 1");
        turns = (int)lround(p) - 1;
    }
    gc_cmd_t c;
    memset(&c, 0, sizeof c);
    c.kind = GC_ARC;
    memcpy(c.tgt, tgt, sizeof c.tgt);
    c.ctr[a0] = c0 + offset(in, a0);
    c.ctr[a1] = c1 + offset(in, a1);
    c.ctr[lin] = in->pos[lin];
    c.plane = in->plane;
    c.cw = cw;
    c.turns = turns;
    c.feed = in->feed;
    emit(in, &c, line);
    memcpy(in->pos, tgt, sizeof in->pos);
    return 0;
}

/* ---------- G10 ---------- */
static int wcs_from_p(const gc_interp_t *in, const gc_block_t *b, int maxp, int add, gc_err_t *e)
{
    if (!GC_HAS(b, 'P')) return fail(e, -1, "G10 изисква P (номер на нулата)");
    double p = GC_VAL(b, 'P');
    int n = (int)lround(p);
    if (fabs(p - n) > 1e-9 || n < 0 || n > maxp) return fail(e, b->col['P' - 'A'], "Невалиден номер на нулата P%g", p);
    if (n == 0) return in->wcs;
    return n + add;
}

static int do_g10(gc_interp_t *in, const gc_block_t *b, int line, gc_err_t *e)
{
    if (!GC_HAS(b, 'L')) return fail(e, -1, "G10 изисква L");
    int L = (int)lround(GC_VAL(b, 'L'));
    bool haas = in->cfg.dialect == GC_DIALECT_HAAS;
    int lc = b->col['L' - 'A'];

    if (L == 2 || L == 20) {
        int w;
        bool direct;   /* стойностите са самата нула (иначе: текущата позиция да стане дадената) */
        if (haas) {
            w = L == 2 ? wcs_from_p(in, b, 6, 0, e) : wcs_from_p(in, b, 99, 6, e);
            direct = true;
        } else {
            w = wcs_from_p(in, b, GC_WCS_N - 1, 0, e);
            direct = L == 2;
        }
        if (w < 0) return -1;
        if (w < 1 || w >= GC_WCS_N) return fail(e, b->col['P' - 'A'], "Невалиден номер на нулата");
        for (int a = 0; a < GC_NAX; a++) {
            if (!GC_HAS(b, AXL[a])) continue;
            double v = GC_VAL(b, AXL[a]) * unit(in, a);
            double *o = &in->tab->wcs[w][a];
            if (direct) *o = (haas && in->incr) ? *o + v : v;
            else *o = in->pos[a] - in->g92[a] - (a == GC_Z && in->tlo_on ? in->tlo : 0) - v;
        }
        emit_simple(in, GC_SET_WCS, w, 0, line);
        return 0;
    }

    bool tool_l = (L == 1 && !haas) || (haas && L >= 10 && L <= 13);
    if (!tool_l) return fail(e, lc, "G10 L%d не се поддържа (%s)", L, haas ? "Haas: L2, L20, L10–L13" : "Mach3: L1, L2, L20");
    if (!GC_HAS(b, 'P')) return fail(e, -1, "G10 L%d изисква P (номер на инструмента)", L);
    int t = (int)lround(GC_VAL(b, 'P'));
    if (t < 0 || t >= GC_TOOL_N) return fail(e, b->col['P' - 'A'], "Невалиден инструмент P%d", t);
    gc_tool_t *tt = &in->tab->tool[t];
    double un = in->inch ? 25.4 : 1;
    if (L == 1) {
        if (GC_HAS(b, 'Z')) tt->len = GC_VAL(b, 'Z') * un;
        if (GC_HAS(b, 'R')) tt->dia = 2 * GC_VAL(b, 'R') * un;
    } else {
        if (!GC_HAS(b, 'R')) return fail(e, -1, "G10 L%d изисква R (стойността)", L);
        double v = GC_VAL(b, 'R') * un;
        double *f = L == 10 ? &tt->len : L == 11 ? &tt->wear : L == 12 ? &tt->dia : &tt->wear_dia;
        *f = in->incr ? *f + v : v;
    }
    emit_simple(in, GC_SET_TOOL, t, 0, line);
    return 0;
}

/* ---------- G28 / G30 ---------- */
static int do_g28(gc_interp_t *in, const gc_block_t *b, int g, int line, gc_err_t *e)
{
    double *ref = g == 280 ? in->tab->g28 : in->tab->g30;
    double tgt[GC_NAX];
    if (!has_axes(b)) {   /* всички оси направо до точката */
        memcpy(tgt, in->pos, sizeof tgt);
        for (int a = 0; a < GC_NAX; a++)
            if (in->cfg.axes >> a & 1u) tgt[a] = ref[a];
        move_to(in, tgt, true, line);
        return 0;
    }
    axis_target(in, b, tgt);       /* междинна точка */
    move_to(in, tgt, true, line);
    for (int a = 0; a < GC_NAX; a++)
        if (GC_HAS(b, AXL[a])) tgt[a] = ref[a];
    move_to(in, tgt, true, line);
    return 0;
}

/* ---------- един ред ---------- */
static bool has_m(const gc_block_t *b, int m)
{
    for (int k = 0; k < b->nm; k++)
        if (b->m[k] == m) return true;
    return false;
}

int gc_interp_line(gc_interp_t *in, const char *text, int line, gc_err_t *e)
{
    gc_block_t blk, *b = &blk;
    e->line = line;
    e->col = -1;
    e->msg[0] = 0;
    if (gc_parse_line(text, b, e)) return -1;
    if (b->block_delete && in->cfg.block_delete) return 0;
    if (b->empty || b->percent) return 0;
    if (in->ended) return 0;
    bool haas = in->cfg.dialect == GC_DIALECT_HAAS;
    char gb[16];

    /* --- проверка на G-кодовете и модалните групи --- */
    int grp[GR_N];
    for (int k = 0; k < GR_N; k++) grp[k] = -1;
    int non_modal = -1;
    for (int k = 0; k < b->ng; k++) {
        int g = b->g[k], gr = group_of(g);
        if (g == 281 && haas) gr = GR_NONE;
        if (gr == GR_NONE) {
            const char *st = later_g(g);
            if (st) return fail(e, b->gcol[k], "G%s още не се поддържа: %s", gname(g, gb), st);
            return fail(e, b->gcol[k], "Непознат G-код G%s", gname(g, gb));
        }
        if (grp[gr] >= 0) {
            char gb2[16];
            return fail(e, b->gcol[k], "G%s и G%s са от една група и не може да са в един ред", gname(grp[gr], gb2), gname(g, gb));
        }
        grp[gr] = g;
        if (gr == GR_0 && g != 530 && g != 1030) non_modal = g;
    }
    /* G53 е заедно с движението; с G0/G1 от същия ред или модалния */
    bool g53 = grp[GR_0] == 530;
    if (g53) {
        for (int k = 0; k < b->ng; k++)
            if (group_of(b->g[k]) == GR_0 && b->g[k] != 530 && b->g[k] != 1030) return fail(e, b->gcol[k], "G53 не може да е заедно с G%s", gname(b->g[k], gb));
    }

    /* --- проверка на M-кодовете --- */
    for (int k = 0; k < b->nm; k++) {
        int m = b->m[k];
        for (int j = 0; j < k; j++)
            if (b->m[j] == m) return fail(e, b->mcol[k], "M%d се повтаря в реда", m);
        switch (m) {
        case 0: case 1: case 2: case 30: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
        case 19: case 41: case 42: case 48: case 49: case 88: case 89:
            break;
        case 97: case 98: case 99:
            return fail(e, b->mcol[k], "M%d (подпрограми) още не се поддържа: идва в стъпка 2", m);
        case 62: case 63: case 64: case 65: case 66:
            return fail(e, b->mcol[k], "M%d (входове и изходи) още не се поддържа: идва в стъпка 6", m);
        default:
            if (m >= 100 && m <= 199) return fail(e, b->mcol[k], "Макросът M%d не е дефиниран", m);
            return fail(e, b->mcol[k], "Непознат M-код M%d", m);
        }
    }
    int nstop = has_m(b, 0) + has_m(b, 1) + has_m(b, 2) + has_m(b, 30);
    if (nstop > 1) return fail(e, -1, "Повече от един от M0, M1, M2, M30 в реда");
    if (has_m(b, 3) + has_m(b, 4) + has_m(b, 5) > 1) return fail(e, -1, "M3, M4 и M5 не може да са в един ред");

    /* --- осите --- */
    for (int a = 0; a < GC_NAX; a++)
        if (GC_HAS(b, AXL[a]) && !(in->cfg.axes >> a & 1u)) {
            bool g4x = haas && a == GC_X && non_modal == 40;
            if (!g4x) return fail(e, b->col[AXL[a] - 'A'], "Оста %s е изключена", AXN[a]);
        }

    /* --- 1. мерни единици (преди F и координатите) --- */
    if (grp[GR_UNITS] >= 0) in->inch = grp[GR_UNITS] == 200;

    /* --- 2. F, S, T --- */
    if (GC_HAS(b, 'F')) {
        double f = GC_VAL(b, 'F');
        if (f < 0) return fail(e, b->col['F' - 'A'], "Отрицателно подаване F");
        in->feed = f * (in->inch ? 25.4 : 1);
    }
    if (GC_HAS(b, 'S')) {
        double s = GC_VAL(b, 'S');
        if (s < 0) return fail(e, b->col['S' - 'A'], "Отрицателни обороти S");
        in->s = s;
        if (in->sp_dir && !has_m(b, 3) && !has_m(b, 4) && !has_m(b, 5)) emit_simple(in, GC_SPINDLE, in->sp_dir, in->s, line);
    }
    if (GC_HAS(b, 'T')) {
        double t = GC_VAL(b, 'T');
        int ti = (int)lround(t);
        if (fabs(t - ti) > 1e-9 || ti < 0 || ti >= GC_TOOL_N) return fail(e, b->col['T' - 'A'], "Невалиден инструмент T%g", t);
        in->tool_next = ti;
        emit_simple(in, GC_TOOL_PREP, ti, 0, line);
    }

    /* --- 3. M6, шпиндел, охлаждане, оверрайди --- */
    if (has_m(b, 6)) {
        if (in->tool_next < 0) return fail(e, -1, "M6 без избран инструмент T");
        in->tool = in->tool_next;
        emit_simple(in, GC_TOOL_CHANGE, in->tool, 0, line);
    }
    if (has_m(b, 3) || has_m(b, 4)) {
        in->sp_dir = has_m(b, 3) ? 1 : -1;
        emit_simple(in, GC_SPINDLE, in->sp_dir, in->s, line);
    } else if (has_m(b, 5)) {
        in->sp_dir = 0;
        emit_simple(in, GC_SPINDLE, 0, 0, line);
    }
    if (has_m(b, 19)) {
        double ang = GC_HAS(b, 'R') ? GC_VAL(b, 'R') : (GC_HAS(b, 'P') && non_modal != 40 ? GC_VAL(b, 'P') : 0);
        emit_simple(in, GC_ORIENT, 0, ang, line);
    }
    int cool = in->cool;
    if (has_m(b, 9)) cool = 0;
    if (has_m(b, 89)) cool &= ~GC_COOL_TSC;
    if (has_m(b, 7)) cool |= GC_COOL_MIST;
    if (has_m(b, 8)) cool |= GC_COOL_FLOOD;
    if (has_m(b, 88)) cool |= GC_COOL_TSC;
    if (cool != in->cool || has_m(b, 9)) {
        in->cool = cool;
        emit_simple(in, GC_COOLANT, cool, 0, line);
    }
    if (has_m(b, 48) || has_m(b, 49)) {
        in->ovr_en = has_m(b, 48);
        emit_simple(in, GC_OVR_ENABLE, in->ovr_en, 0, line);
    }

    /* --- 4. пауза G4 --- */
    if (non_modal == 40) {
        double t;
        if (GC_HAS(b, 'P')) {
            t = GC_VAL(b, 'P');
            if (haas && !(b->dot >> ('P' - 'A') & 1u)) t /= 1000.0;   /* Haas: P без точка = ms */
        } else if (haas && GC_HAS(b, 'X')) {
            t = GC_VAL(b, 'X');
        } else {
            return fail(e, -1, "G4 изисква P (време)");
        }
        if (t < 0) return fail(e, -1, "Отрицателно време на паузата G4");
        emit_simple(in, GC_DWELL, 0, t, line);
    }

    /* --- 5. модални: равнина, корекции, нули, режим на пътя, разстояния --- */
    if (grp[GR_PLANE] >= 0) in->plane = grp[GR_PLANE] / 10;
    if (grp[GR_TLO] == 490) {
        if (in->tlo_on) { in->tlo_on = false; in->tlo = 0; in->tlo_h = 0; emit_simple(in, GC_TLO, 0, 0, line); }
    } else if (grp[GR_TLO] == 430) {
        int h = in->tool;
        if (GC_HAS(b, 'H')) {
            double hv = GC_VAL(b, 'H');
            h = (int)lround(hv);
            if (fabs(hv - h) > 1e-9 || h < 0 || h >= GC_TOOL_N) return fail(e, b->col['H' - 'A'], "Невалиден номер H%g", hv);
        }
        in->tlo_on = true;
        in->tlo_h = h;
        in->tlo = in->tab->tool[h].len + in->tab->tool[h].wear;
        emit_simple(in, GC_TLO, h, in->tlo, line);
    }
    if (grp[GR_WCS] >= 0) {
        int g = grp[GR_WCS], w;
        if (g == 541 || g == 1540) {
            if (!GC_HAS(b, 'P')) return fail(e, -1, "G%s изисква P (1–99)", gname(g, gb));
            int p = (int)lround(GC_VAL(b, 'P'));
            if (p < 1 || p > 99) return fail(e, b->col['P' - 'A'], "G%s P трябва да е от 1 до 99", gname(g, gb));
            w = 6 + p;
        } else if (g == 590 && !haas && GC_HAS(b, 'P') && non_modal < 0) {   /* Mach3: G59 P7 … */
            int p = (int)lround(GC_VAL(b, 'P'));
            if (p < 1 || p >= GC_WCS_N) return fail(e, b->col['P' - 'A'], "G59 P трябва да е от 1 до %d", GC_WCS_N - 1);
            w = p;
        } else {
            w = (g - 540) / 10 + 1;
        }
        if (w != in->wcs) { in->wcs = w; emit_simple(in, GC_WCS_SELECT, w, 0, line); }
    }
    if (grp[GR_PATH] >= 0) {
        int g = grp[GR_PATH];
        double tol = 0;
        int mode = g == 1870 ? 640 : g;
        if (g == 640 && GC_HAS(b, 'P')) tol = GC_VAL(b, 'P') * (in->inch ? 25.4 : 1);
        if (g == 1870) {
            if (GC_HAS(b, 'E')) tol = GC_VAL(b, 'E') * (in->inch ? 25.4 : 1);
            else if (GC_HAS(b, 'P')) {
                int p = (int)lround(GC_VAL(b, 'P'));
                tol = p == 1 ? 0.1 : p == 2 ? 0.05 : p == 3 ? 0.01 : 0;
            }
        }
        if (tol < 0) return fail(e, -1, "Отрицателен допуск на пътя");
        in->path_mode = mode;
        in->path_tol = tol;
        emit_simple(in, GC_PATH_MODE, mode, tol, line);
    }
    if (grp[GR_DIST] >= 0) in->incr = grp[GR_DIST] == 910;
    if (grp[GR_ARCDIST] >= 0) in->arc_abs = grp[GR_ARCDIST] == 901;
    if (grp[GR_RETURN] >= 0) in->retract_r = grp[GR_RETURN] == 990;
    if (grp[GR_CYCLE] == 800) in->cycle = 0;

    /* --- 6. не-модални: G10, G28, G30, G92 --- */
    bool axes_used = false;
    switch (non_modal) {
    case 100:
        if (do_g10(in, b, line, e)) return -1;
        axes_used = true;
        break;
    case 280: case 300:
        if (do_g28(in, b, non_modal, line, e)) return -1;
        axes_used = true;
        break;
    case 281: case 301:
        if (!haas && non_modal == 281) {   /* Mach3: реферирване на дадените оси */
            unsigned mask = 0;
            for (int a = 0; a < GC_NAX; a++)
                if (GC_HAS(b, AXL[a])) mask |= 1u << a;
            if (!mask) mask = in->cfg.axes;
            emit_simple(in, GC_HOME, (int)mask, 0, line);
        } else {
            memcpy(non_modal == 281 ? in->tab->g28 : in->tab->g30, in->pos, sizeof in->pos);
        }
        axes_used = true;
        break;
    case 920:
        if (!has_axes(b)) return fail(e, -1, "G92 изисква поне една ос");
        for (int a = 0; a < GC_NAX; a++) {
            if (!GC_HAS(b, AXL[a])) continue;
            double v = GC_VAL(b, AXL[a]) * unit(in, a);
            in->g92[a] = in->pos[a] - in->tab->wcs[in->wcs][a] - (a == GC_Z && in->tlo_on ? in->tlo : 0) - v;
        }
        axes_used = true;
        break;
    case 921: case 922:
        memset(in->g92, 0, sizeof in->g92);
        break;
    case 40:
        if (haas && GC_HAS(b, 'X')) axes_used = true;
        break;
    default:
        break;
    }

    /* --- 7. движение --- */
    if (grp[GR_MOTION] >= 0) {
        in->motion = grp[GR_MOTION];
        in->cycle = 0;
    }
    if (g53) {
        int mo = in->motion;
        if (mo != 0 && mo != 10) return fail(e, -1, "G53 изисква G0 или G1");
        if (mo == 10 && in->feed <= 0) return fail(e, -1, "Липсва подаване F");
        double tgt[GC_NAX];
        memcpy(tgt, in->pos, sizeof tgt);
        for (int a = 0; a < GC_NAX; a++)
            if (GC_HAS(b, AXL[a])) tgt[a] = GC_VAL(b, AXL[a]) * unit(in, a);
        move_to(in, tgt, mo == 0, line);
    } else if (has_axes(b) && !axes_used) {
        double tgt[GC_NAX];
        switch (in->motion) {
        case 0:
            axis_target(in, b, tgt);
            move_to(in, tgt, true, line);
            break;
        case 10:
            if (in->feed <= 0) return fail(e, -1, "Липсва подаване F");
            axis_target(in, b, tgt);
            move_to(in, tgt, false, line);
            break;
        case 20: case 30:
            if (do_arc(in, b, in->motion == 20, line, e)) return -1;
            break;
        default:
            return fail(e, -1, "Има координати, но не е избрано движение (G0, G1, G2 или G3)");
        }
    }

    /* --- 8. спиране и край --- */
    if (has_m(b, 0) || has_m(b, 1)) emit_simple(in, GC_PAUSE, has_m(b, 1), 0, line);
    if (has_m(b, 2) || has_m(b, 30)) {
        reset_modal(in);
        in->ended = true;
        emit_simple(in, GC_END, 0, 0, line);
    }
    return 0;
}

int gc_run_text(gc_interp_t *in, const char *text, gc_err_t *e)
{
    int line = 0;
    bool code = false, pct = false;
    const char *s = text;
    while (*s) {
        const char *nl = strchr(s, '\n');
        gc_block_t b;
        if (gc_parse_line(s, &b, e) == 0 && b.percent) {
            if (code || pct) break;   /* вторият % е краят на програмата */
            pct = true;
        } else {
            if (gc_interp_line(in, s, line, e)) return -1;
            if (!b.empty) code = true;
            if (in->ended) return 0;
        }
        if (!nl) break;
        s = nl + 1;
        line++;
    }
    if (!in->ended) {
        in->ended = true;
        emit_simple(in, GC_END, 0, 0, line);
    }
    return 0;
}
