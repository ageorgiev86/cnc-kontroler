/* Компенсация на радиуса на фрезата G41/G42 в равнина XY (docs/interpretator-planer.md).
 *
 * Програмираният път идва като команди от интерпретатора. Всеки XY сегмент (линия или дъга) чака
 * следващия, защото краят му зависи от ъгъла между двата:
 *   - продължение без ъгъл: краят е отместен перпендикулярно;
 *   - външен ъгъл: сегментът свършва перпендикулярно отместен, после дъга с радиуса на фрезата
 *     около върха до началото на следващия;
 *   - вътрешен ъгъл: двата отместени сегмента се отрязват до пресечната си точка; ако такава няма
 *     в рамките на сегментите, фрезата би врязала: грешка.
 * Влизането (първото XY движение след G41/G42) е линия от сегашната позиция до отместения ѝ край.
 * При G40 последният сегмент свършва перпендикулярно отместен и следващите движения тръгват оттам.
 * Командите без XY движение (чисто Z, шпиндел, пауза …) чакат и излизат в същия ред. */
#include "gc_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define EPS 1e-6

typedef struct { double x, y; } v2;

static v2 v(double x, double y) { v2 r = {x, y}; return r; }
static v2 add(v2 a, v2 b) { return v(a.x + b.x, a.y + b.y); }
static v2 sub(v2 a, v2 b) { return v(a.x - b.x, a.y - b.y); }
static v2 mul(v2 a, double k) { return v(a.x * k, a.y * k); }
static double dot(v2 a, v2 b) { return a.x * b.x + a.y * b.y; }
static double cross(v2 a, v2 b) { return a.x * b.y - a.y * b.x; }
static double len(v2 a) { return hypot(a.x, a.y); }
static v2 norm(v2 a) { double l = len(a); return l > 0 ? mul(a, 1 / l) : v(0, 0); }
static v2 left(v2 t) { return v(-t.y, t.x); }

/* сегментът в XY: начало, край, център (дъга) */
typedef struct {
    bool arc, cw;
    v2 s, e, c;
} seg_t;

static bool is_xy_motion(const gc_cmd_t *c, const double *start)
{
    if (c->kind == GC_ARC) return true;
    if (c->kind != GC_RAPID && c->kind != GC_FEED) return false;
    return fabs(c->tgt[GC_X] - start[GC_X]) > EPS || fabs(c->tgt[GC_Y] - start[GC_Y]) > EPS;
}

static seg_t mkseg(const gc_cmd_t *c, const double *start)
{
    seg_t s;
    s.arc = c->kind == GC_ARC;
    s.cw = c->cw;
    s.s = v(start[GC_X], start[GC_Y]);
    s.e = v(c->tgt[GC_X], c->tgt[GC_Y]);
    s.c = v(c->ctr[GC_X], c->ctr[GC_Y]);
    return s;
}

/* посока на движение в точка p от сегмента */
static v2 tangent(const seg_t *s, v2 p)
{
    if (!s->arc) return norm(sub(s->e, s->s));
    v2 r = sub(p, s->c);
    return norm(s->cw ? v(r.y, -r.x) : v(-r.y, r.x));
}

static void comp_fail(gc_interp_t *in, int line, const char *msg)
{
    if (in->comp_err[0]) return;   /* първата грешка */
    in->comp_err_line = line;
    snprintf(in->comp_err, sizeof in->comp_err, "%s", msg);
}

/* радиусът на отместената дъга (G41 отляво: при CCW навътре, при CW навън) */
static double arc_off_r(const gc_interp_t *in, const seg_t *s)
{
    double R = len(sub(s->s, s->c));
    return s->cw ? R + in->comp_side * in->comp_r : R - in->comp_side * in->comp_r;
}

/* изпраща сегмента с отместения край e (XY), а останалите оси както са програмирани */
static void send_seg(gc_interp_t *in, const gc_cmd_t *c, v2 e)
{
    gc_cmd_t o = *c;
    o.tgt[GC_X] = e.x;
    o.tgt[GC_Y] = e.y;
    gc_out(in, &o);
}

static void flush_queue(gc_interp_t *in)
{
    for (int i = 0; i < in->comp_nq; i++) {
        gc_cmd_t *c = &in->comp_q[i];
        if (c->kind == GC_RAPID || c->kind == GC_FEED) {   /* чисто Z / ABC: XY остава там, където е фрезата */
            c->tgt[GC_X] = in->comp_out[GC_X];
            c->tgt[GC_Y] = in->comp_out[GC_Y];
        }
        gc_out(in, c);
    }
    in->comp_nq = 0;
}

/* пресечни точки на права (точка p, посока d) с окръжност (c, R) */
static int line_circle(v2 p, v2 d, v2 c, double R, v2 *out)
{
    v2 f = sub(p, c);
    double b = dot(f, d), cc = dot(f, f) - R * R, disc = b * b - cc;
    if (disc < -EPS) return 0;
    if (disc < 0) disc = 0;
    double s = sqrt(disc);
    out[0] = add(p, mul(d, -b - s));
    out[1] = add(p, mul(d, -b + s));
    return 2;
}

static int circle_circle(v2 c1, double r1, v2 c2, double r2, v2 *out)
{
    v2 d = sub(c2, c1);
    double D = len(d);
    if (D < EPS || D > r1 + r2 + EPS || D < fabs(r1 - r2) - EPS) return 0;
    double a = (r1 * r1 - r2 * r2 + D * D) / (2 * D), h2 = r1 * r1 - a * a;
    double h = h2 > 0 ? sqrt(h2) : 0;
    v2 u = mul(d, 1 / D), m = add(c1, mul(u, a));
    out[0] = add(m, mul(left(u), h));
    out[1] = sub(m, mul(left(u), h));
    return 2;
}

/* Чакащият сегмент a (със старт astart в изхода) свършва при програмираната точка P, където
 * започва b. Изпраща a (и дъгата на външния ъгъл) и връща началото на отместения b. */
static v2 corner(gc_interp_t *in, const seg_t *a, const gc_cmd_t *ac, v2 astart, const seg_t *b, const gc_cmd_t *bc)
{
    int sd = in->comp_side;
    double r = in->comp_r;
    v2 P = a->e, ta = tangent(a, P), tb = tangent(b, b->s);
    v2 ea = add(P, mul(left(ta), sd * r)), sb = add(P, mul(left(tb), sd * r));
    if (len(sub(ea, sb)) < 1e-4 || r < EPS) {   /* без ъгъл */
        send_seg(in, ac, ea);
        return ea;
    }
    double turn = cross(ta, tb);
    bool outside = sd * turn < 0 || (fabs(turn) < 1e-9 && dot(ta, tb) < 0);
    if (outside) {
        send_seg(in, ac, ea);
        gc_cmd_t arc = *ac;
        arc.kind = GC_ARC;
        arc.tgt[GC_X] = sb.x;
        arc.tgt[GC_Y] = sb.y;
        arc.ctr[GC_X] = P.x;
        arc.ctr[GC_Y] = P.y;
        arc.ctr[GC_Z] = ac->tgt[GC_Z];
        arc.plane = GC_PLANE_XY;
        arc.cw = sd > 0;   /* G41: около върха по часовника, G42: обратно */
        arc.turns = 0;
        arc.feed = bc->kind == GC_RAPID ? ac->feed : bc->feed;
        if (arc.feed <= 0) {   /* два бързи хода: права линия вместо дъга */
            arc.kind = GC_RAPID;
            arc.feed = 0;
        }
        gc_out(in, &arc);
        return sb;
    }
    /* вътрешен ъгъл: пресечната точка на двата отместени сегмента */
    v2 cand[2];
    int n = 0;
    double Ra = 0, Rb = 0;
    if (a->arc) { Ra = arc_off_r(in, a); if (Ra <= EPS) { comp_fail(in, ac->line, "Радиусът на дъгата е по-малък от радиуса на фрезата"); return ea; } }
    if (b->arc) { Rb = arc_off_r(in, b); if (Rb <= EPS) { comp_fail(in, bc->line, "Радиусът на дъгата е по-малък от радиуса на фрезата"); return sb; } }
    /* права на влизането: от реалното начало до отместения край */
    v2 pa = astart, da = a->arc ? ta : norm(sub(ea, astart));
    if (!a->arc && len(sub(ea, astart)) < EPS) da = ta;
    if (!a->arc && !b->arc) {
        double den = cross(da, tb);
        if (fabs(den) > 1e-12) {
            double u = cross(sub(sb, pa), tb) / den;
            cand[n++] = add(pa, mul(da, u));
        }
    } else if (!a->arc && b->arc) {
        n = line_circle(pa, da, b->c, Rb, cand);
    } else if (a->arc && !b->arc) {
        n = line_circle(sb, tb, a->c, Ra, cand);
    } else {
        n = circle_circle(a->c, Ra, b->c, Rb, cand);
    }
    if (!n) {
        comp_fail(in, ac->line, "Компенсацията не може да намери ъгъла (фрезата е твърде голяма за контура)");
        send_seg(in, ac, ea);
        return sb;
    }
    v2 X = cand[0];
    if (n > 1 && len(sub(cand[1], P)) < len(sub(X, P))) X = cand[1];
    /* проверка срещу врязване: точката да е в рамките на двата сегмента */
    bool ok = len(sub(X, P)) < 50 * r + 1e-3;
    if (!a->arc) ok = ok && dot(sub(X, astart), da) > -1e-4;
    if (!b->arc) {
        v2 eb = add(b->e, mul(left(tb), sd * r));
        ok = ok && dot(sub(eb, X), tb) > -1e-4;
    }
    if (!ok) {
        char m[200];
        snprintf(m, sizeof m, "Фрезата Ø%.3f не минава във вътрешния ъгъл (сегментът е по-къс от радиуса)", 2 * r);
        comp_fail(in, bc->line, m);
    }
    send_seg(in, ac, X);
    return X;
}

void gc_comp_start(gc_interp_t *in, int side, double r)
{
    in->comp_side = side;
    in->comp_r = r;
    in->comp_entry = true;
    in->comp_have = false;
    in->comp_nq = 0;
}

void gc_comp_push(gc_interp_t *in, const gc_cmd_t *c)
{
    if (!is_xy_motion(c, in->pos)) {
        if (!in->comp_have) {   /* нищо не чака: направо, със сегашното XY на фрезата */
            gc_cmd_t o = *c;
            if (o.kind == GC_RAPID || o.kind == GC_FEED) {
                o.tgt[GC_X] = in->comp_out[GC_X];
                o.tgt[GC_Y] = in->comp_out[GC_Y];
            }
            gc_out(in, &o);
            return;
        }
        if (in->comp_nq >= GC_COMP_Q) { comp_fail(in, c->line, "Твърде много редове без XY движение при компенсация G41/G42"); return; }
        in->comp_q[in->comp_nq++] = *c;
        return;
    }
    if (in->comp_entry && c->kind == GC_ARC) {
        comp_fail(in, c->line, "Влизането в компенсацията G41/G42 трябва да е с линия (G0 или G1), не с дъга");
        return;
    }
    if (c->kind == GC_ARC) {   /* дъга, по-малка от фрезата от страната на отместването */
        seg_t s = mkseg(c, in->pos);
        if (arc_off_r(in, &s) <= EPS) {
            char m[200];
            snprintf(m, sizeof m, "Радиусът на дъгата (%.3f) е по-малък от радиуса на фрезата (%.3f)", len(sub(s.s, s.c)), in->comp_r);
            comp_fail(in, c->line, m);
            return;
        }
    }
    if (in->comp_have) {
        seg_t a = mkseg(&in->comp_seg, in->comp_s), b = mkseg(c, in->pos);
        v2 astart = v(in->comp_out[GC_X], in->comp_out[GC_Y]);
        corner(in, &a, &in->comp_seg, astart, &b, c);
        flush_queue(in);
    }
    in->comp_seg = *c;
    memcpy(in->comp_s, in->pos, sizeof in->comp_s);
    in->comp_have = true;
    in->comp_entry = false;
}

void gc_comp_finish(gc_interp_t *in)
{
    if (in->comp_have) {
        seg_t a = mkseg(&in->comp_seg, in->comp_s);
        v2 ta = tangent(&a, a.e);
        send_seg(in, &in->comp_seg, add(a.e, mul(left(ta), in->comp_side * in->comp_r)));
    }
    in->comp_have = false;
    flush_queue(in);
    in->comp_side = 0;
    in->comp_entry = false;
}
