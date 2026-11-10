/* Планер: look-ahead и интерполатор 1 ms с ограничен jerk (docs/interpretator-planer.md, стъпка 3).
 *
 * Буферът е пръстен от елементи: движения (линии; дъгите се делят на хорди) и събития.
 * За всяко движение: дължина, единичен вектор, скорост от F, граница от осите (vaxis),
 * от радиуса на дъгата (vcurve), ускорение и jerk по пътя (от най-строгата ос).
 * За всяка връзка между две движения: най-голямата скорост по допуска на ъгъла (vj).
 * Събитията, които искат спиране (пауза, смяна, шпиндел, M0 …), правят скоростта на връзката 0.
 *
 * Интерполаторът движи разстоянието по пътя s с ограничен jerk към F × оверрайда. Тактът е на
 * NSUB части (jerk се сменя и вътре в такта, затова спирането пада точно). На всяка част
 * проверява дали от новото състояние може да спре (със S-крива) до всяко следващо ограничение
 * (връзка, спиране, краят на буфера). Ако не може, спира. Така паузата и смяната на оверрайда
 * са плавни, без преизчисляване на целия буфер. */
#include "cnc/planner.h"
#include "cnc/hmap.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CAP 16384
#define EPS 1e-9
#define SNAP 1e-4     /* мм: толкова близо до спиране = там */
#define NSUB 10       /* части на такта */

typedef struct {
    bool move;
    double p0[GC_NAX], u[GC_NAX], len;
    double f;          /* мм/s; за бърз ход: INFINITY */
    bool rapid;
    double vaxis, vcurve, A, J;
    double vj;         /* скоростта на връзката с следващото движение (0 = спиране) */
    int line;
    bool ovr;          /* оверрайдът на подаването важи (M48) */
    double s0;         /* пътят от началото до него (за единичния блок) */
    gc_cmd_t ev;
    bool stop;
} item_t;

struct pl {
    pl_config_t cfg;
    item_t *q;
    int head, n;
    double last[GC_NAX];   /* краят на последното прието движение (с корекцията от картата) */
    double raw[GC_NAX];    /* същото без корекцията (както е в програмата) */
    double total;          /* общият път на приетите движения */
    const hm_map_t *hm;    /* картата на височините (NULL = без) */
    bool ovr_on;
    int last_move;         /* индексът му в пръстена (-1 = няма) */
    bool open;             /* към него може да се върже следващото движение (без спиране между тях) */
    double tol;
    bool exact;
    bool input_end;
    /* интерполатор */
    double s, v, a, pos[GC_NAX];
    bool waiting, hold, done, dwelling;
    double dwell_left;
    double fov, rov;
    bool single;           /* единичен блок */
    double single_from;    /* спира в края на ред само след тази точка от пътя (докъдето може да спре) */
};

static item_t *at(pl_t *p, int i) { return &p->q[(p->head + i) % CAP]; }
static const item_t *cat(const pl_t *p, int i) { return &p->q[(p->head + i) % CAP]; }

pl_t *pl_create(const pl_config_t *cfg, const double start[GC_NAX])
{
    pl_t *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->q = calloc(CAP, sizeof *p->q);
    if (!p->q) { free(p); return NULL; }
    p->cfg = *cfg;
    if (p->cfg.dt <= 0) p->cfg.dt = 0.001;
    if (start) memcpy(p->last, start, sizeof p->last);
    memcpy(p->raw, p->last, sizeof p->raw);
    memcpy(p->pos, p->last, sizeof p->pos);
    p->ovr_on = true;
    p->last_move = -1;
    p->tol = cfg->tol;
    p->fov = p->rov = 1;
    return p;
}

void pl_destroy(pl_t *p)
{
    if (!p) return;
    free(p->q);
    free(p);
}

int pl_space(const pl_t *p) { return CAP - p->n; }
void pl_input_end(pl_t *p) { p->input_end = true; }
void pl_continue(pl_t *p) { p->waiting = false; }
void pl_hold(pl_t *p, bool on) { p->hold = on; }
void pl_override(pl_t *p, double feed, double rapid)
{
    p->fov = feed < 0 ? 0 : feed > 2 ? 2 : feed;
    p->rov = rapid < 0 ? 0 : rapid > 1 ? 1 : rapid;
}

void pl_set_hmap(pl_t *p, const hm_map_t *m) { p->hm = m && hm_check(m) == 0 ? m : NULL; }

static bool stop_event(gc_kind_t k)
{
    return k == GC_DWELL || k == GC_SPINDLE || k == GC_TOOL_CHANGE || k == GC_PAUSE || k == GC_HOME ||
           k == GC_ORIENT || k == GC_END;
}

/* най-голямата скорост в ъгъла между посоките u1 и u2 при допуск tol и ускорение A:
 * фрезата минава по дъга, която се отклонява от върха с tol */
static double junction(const double *u1, const double *u2, double tol, double A)
{
    double c = 0;
    for (int k = 0; k < GC_NAX; k++) c -= u1[k] * u2[k];
    if (c < -0.999999) return INFINITY;   /* права линия */
    if (c > 0.999999 || tol <= 0) return 0;   /* обръщане или точен стоп */
    double sh = sqrt(0.5 * (1 - c));
    return sqrt(A * tol * sh / (1 - sh));
}

static void push_item(pl_t *p, const item_t *it)
{
    p->q[(p->head + p->n) % CAP] = *it;
    p->n++;
}

static void push_move(pl_t *p, const double *p1, bool rapid, double f, double vcurve, int line)
{
    item_t it;
    memset(&it, 0, sizeof it);
    double d[GC_NAX], lin = 0, rot = 0;
    for (int k = 0; k < GC_NAX; k++) d[k] = p1[k] - p->last[k];
    for (int k = 0; k < 3; k++) lin += d[k] * d[k];
    for (int k = 3; k < GC_NAX; k++) rot += d[k] * d[k];
    it.len = lin > EPS * EPS ? sqrt(lin) : sqrt(rot);   /* подаването е по XYZ; само въртене: в ° */
    if (it.len < 1e-7) { memcpy(p->last, p1, sizeof p->last); return; }
    it.move = true;
    memcpy(it.p0, p->last, sizeof it.p0);
    it.vaxis = it.A = it.J = INFINITY;
    for (int k = 0; k < GC_NAX; k++) {
        it.u[k] = d[k] / it.len;
        double au = fabs(it.u[k]);
        if (au < 1e-12) continue;
        if (p->cfg.vmax[k] / au < it.vaxis) it.vaxis = p->cfg.vmax[k] / au;
        if (p->cfg.amax[k] / au < it.A) it.A = p->cfg.amax[k] / au;
        if (p->cfg.jmax[k] / au < it.J) it.J = p->cfg.jmax[k] / au;
    }
    it.rapid = rapid;
    it.f = rapid ? INFINITY : f;
    it.vcurve = vcurve;
    it.line = line;
    it.ovr = p->ovr_on;
    it.s0 = p->total;
    p->total += it.len;
    it.vj = 0;
    if (p->open && p->last_move >= 0) {
        item_t *pr = &p->q[p->last_move];
        pr->vj = p->exact ? 0 : junction(pr->u, it.u, p->tol, fmin(pr->A, it.A));
    }
    p->last_move = (p->head + p->n) % CAP;
    p->open = true;
    push_item(p, &it);
    memcpy(p->last, p1, sizeof p->last);
}

/* на колко части се дели XY движение от p->raw до p1 заради картата (резолюция / 4) */
static int hm_pieces(const pl_t *p, const double *p1)
{
    if (!p->hm) return 1;
    double dxy = hypot(p1[GC_X] - p->raw[GC_X], p1[GC_Y] - p->raw[GC_Y]);
    int n = (int)ceil(dxy / (p->hm->res / 4) - 1e-9);
    if (n > CAP / 4) n = CAP / 4;
    return n < 1 ? 1 : n;
}

/* рязане (G1 или хорда на дъга): с картата се дели и към Z се добавя корекцията */
static void push_cut(pl_t *p, const double *p1, double f, double vcurve, int line)
{
    int n = hm_pieces(p, p1);
    double st[GC_NAX];
    memcpy(st, p->raw, sizeof st);
    for (int k = 1; k <= n; k++) {
        double q[GC_NAX];
        for (int a = 0; a < GC_NAX; a++) q[a] = k == n ? p1[a] : st[a] + (p1[a] - st[a]) * k / n;
        if (p->hm) q[GC_Z] += hm_corr(p->hm, q[GC_X], q[GC_Y]);
        push_move(p, q, false, f, vcurve, line);
    }
    memcpy(p->raw, p1, sizeof p->raw);
}

/* брой хорди за дъгата */
static int arc_chords(const pl_t *p, const gc_cmd_t *c, double *sweep, double *R, int ax[3])
{
    int a0 = GC_X, a1 = GC_Y, lin = GC_Z;
    if (c->plane == GC_PLANE_ZX) { a0 = GC_Z; a1 = GC_X; lin = GC_Y; }
    if (c->plane == GC_PLANE_YZ) { a0 = GC_Y; a1 = GC_Z; lin = GC_X; }
    ax[0] = a0; ax[1] = a1; ax[2] = lin;
    double s0 = p->raw[a0] - c->ctr[a0], s1 = p->raw[a1] - c->ctr[a1];
    double e0 = c->tgt[a0] - c->ctr[a0], e1 = c->tgt[a1] - c->ctr[a1];
    double t0 = atan2(s1, s0), t1 = atan2(e1, e0);
    double sw = c->cw ? t0 - t1 : t1 - t0;
    while (sw <= 1e-9) sw += 2 * M_PI;
    sw += 2 * M_PI * c->turns;
    *sweep = c->cw ? -sw : sw;
    *R = hypot(s0, s1);
    double tol = p->cfg.arc_tol > 0 ? p->cfg.arc_tol : 0.002;
    double dth = *R > tol ? 2 * acos(1 - tol / *R) : M_PI / 2;
    if (dth > M_PI / 4) dth = M_PI / 4;
    int n = (int)ceil(sw / dth);
    if (n > CAP / 4) n = CAP / 4;   /* много обороти: по-дълги хорди, за да се събере в буфера */
    return n < 1 ? 1 : n;
}

int pl_push(pl_t *p, const gc_cmd_t *c)
{
    if (c->kind == GC_RAPID) {   /* без корекция: бързите ходове са над детайла (и G53 до горе) */
        if (pl_space(p) < 1) return -1;
        push_move(p, c->tgt, true, 0, INFINITY, c->line);
        memcpy(p->raw, c->tgt, sizeof p->raw);
        return 0;
    }
    if (c->kind == GC_FEED) {
        if (pl_space(p) < hm_pieces(p, c->tgt)) return -1;
        push_cut(p, c->tgt, c->feed / 60.0, INFINITY, c->line);
        return 0;
    }
    if (c->kind == GC_ARC) {
        double sw, R;
        int ax[3];
        int n = arc_chords(p, c, &sw, &R, ax);
        int per = p->hm ? (int)ceil(fabs(sw) * R / n / (p->hm->res / 4)) + 1 : 1;   /* части на хорда (горна граница) */
        if (pl_space(p) < n * per) return -1;
        double st[GC_NAX], t0 = atan2(p->raw[ax[1]] - c->ctr[ax[1]], p->raw[ax[0]] - c->ctr[ax[0]]);
        double R1 = hypot(c->tgt[ax[0]] - c->ctr[ax[0]], c->tgt[ax[1]] - c->ctr[ax[1]]);
        double acurve = fmin(p->cfg.amax[ax[0]], p->cfg.amax[ax[1]]);
        double vcurve = sqrt(acurve * (R < R1 ? R : R1));
        memcpy(st, p->raw, sizeof st);
        for (int k = 1; k <= n; k++) {
            double f = (double)k / n, th = t0 + sw * f, r = R + (R1 - R) * f, q[GC_NAX];
            for (int a = 0; a < GC_NAX; a++) q[a] = st[a] + (c->tgt[a] - st[a]) * f;
            q[ax[0]] = c->ctr[ax[0]] + r * cos(th);
            q[ax[1]] = c->ctr[ax[1]] + r * sin(th);
            if (k == n) memcpy(q, c->tgt, sizeof q);
            push_cut(p, q, c->feed / 60.0, vcurve, c->line);
        }
        return 0;
    }
    if (pl_space(p) < 1) return -1;
    item_t it;
    memset(&it, 0, sizeof it);
    it.ev = *c;
    it.line = c->line;
    it.stop = stop_event(c->kind);
    if (c->kind == GC_OVR_ENABLE) p->ovr_on = c->ival != 0;   /* M48 / M49 за следващите движения */
    if (c->kind == GC_PATH_MODE) {   /* режимът на ъглите за следващите връзки */
        p->exact = c->ival == 610 || c->ival == 611;
        p->tol = c->val > 0 ? c->val : p->cfg.tol;
    }
    if (it.stop) p->open = false;
    push_item(p, &it);
    return 0;
}

double pl_ahead(const pl_t *p)
{
    double d = 0;
    for (int i = 0; i < p->n; i++)
        if (cat(p, i)->move) d += cat(p, i)->len;
    return d - p->s;
}

static double veff(const pl_t *p, const item_t *it)
{
    double v = it->rapid ? it->vaxis * p->rov : fmin(it->f * (it->ovr ? p->fov : 1), it->vaxis);
    return fmin(v, it->vcurve);
}

/* най-краткият път, за да стигне скорост vf с ускорение 0, от скорост v0 и ускорение a0,
 * с jerk J и ускорение до A (S-крива: jerk −J, постоянно −Ap, jerk +J) */
static double stopdist(double v0, double a0, double vf, double A, double J)
{
    if (vf < 0) vf = 0;
    double vpeak = v0 + (a0 > 0 ? a0 * a0 / (2 * J) : -a0 * a0 / (2 * J));
    if (vpeak <= vf) {   /* ускорението вече стига: само фазата с jerk +J до скорост vf */
        if (v0 <= vf) return 0;
        double t = (-a0 - sqrt(fmax(a0 * a0 - 2 * J * (v0 - vf), 0))) / J;
        return v0 * t + 0.5 * a0 * t * t + J * t * t * t / 6;
    }
    double ap = sqrt(fmax(J * (v0 - vf) + a0 * a0 / 2, 0));
    if (ap > A) ap = A;
    if (ap < -a0) ap = -a0;
    double t1 = (a0 + ap) / J;
    double v1 = v0 + a0 * t1 - 0.5 * J * t1 * t1;
    double d1 = v0 * t1 + 0.5 * a0 * t1 * t1 - J * t1 * t1 * t1 / 6;
    double t3 = ap / J, t2 = ap > 0 ? (v1 - ap * ap / (2 * J) - vf) / ap : 0;
    if (t2 < 0) t2 = 0;
    double v2 = v1 - ap * t2, d2 = v1 * t2 - 0.5 * ap * t2 * t2;
    double d3 = v2 * t3 - 0.5 * ap * t3 * t3 + J * t3 * t3 * t3 / 6;
    return d1 + d2 + d3;
}

/* следващото движение след елемент i (през събития без спиране); -1 = спиране или край */
static int next_move(const pl_t *p, int i)
{
    for (int k = i + 1; k < p->n; k++) {
        const item_t *it = cat(p, k);
        if (it->move) return k;
        if (it->stop) return -1;
    }
    return -1;
}

/* единичен блок: спиране в края на реда (след точката, докъдето може да спре при включването) */
static bool line_stop(const pl_t *p, const item_t *it, int nx)
{
    return p->single && nx >= 0 && cat(p, nx)->line != it->line && it->s0 + it->len >= p->single_from - 1e-9;
}

void pl_single(pl_t *p, bool on)
{
    if (on && !p->single) {
        const item_t *cur = p->n && at(p, 0)->move ? at(p, 0) : NULL;
        double here = cur ? cur->s0 + p->s : p->total;
        double A = cur ? cur->A : 1, J = cur ? cur->J : 1;
        p->single_from = cur ? here + stopdist(p->v, p->a, 0, 0.5 * A, 0.5 * J) + p->v * p->cfg.dt : here;
    }
    p->single = on;
}

/* може ли от (v, a) на разстояние s в текущото движение да спази всички следващи ограничения */
static bool feasible(const pl_t *p, double v, double a, double s, double A, double J)
{
    double d = cat(p, 0)->len - s, reach = stopdist(v, a, 0, A, J);
    int i = 0;
    for (;;) {
        const item_t *it = cat(p, i);
        int nx = next_move(p, i);
        /* до връзката се спира с границите и на следващото движение: влиза в него в тях */
        if (nx >= 0 && (cat(p, nx)->A < A || cat(p, nx)->J < J)) {
            A = fmin(A, cat(p, nx)->A);
            J = fmin(J, cat(p, nx)->J);
            reach = stopdist(v, a, 0, A, J);
        }
        double c = nx < 0 || line_stop(p, it, nx) ? 0 : fmin(it->vj, veff(p, cat(p, nx)));
        if (d < 0) {   /* тази връзка се минава в същия такт: скоростта трябва вече да е под границата */
            if (nx < 0 || v > c + 1e-9) return false;
        } else {
            if (stopdist(v, a, c, A, J) > d + 1e-9) return false;
            if (nx < 0 || d > reach) return true;
        }
        i = nx;
        d += cat(p, i)->len;
    }
}

static void pop(pl_t *p)
{
    p->head = (p->head + 1) % CAP;
    p->n--;
    if (p->last_move >= 0 && p->n == 0) p->last_move = -1;
}

static void add_ev(pl_sample_t *s, const gc_cmd_t *c)
{
    s->has_ev = true;
    s->ev = *c;
}

/* една част от такта (h = dt / NSUB): jerk може да се сменя и между тактовете, затова
 * ускорението в края на такта се мени най-много с J·dt и спирането пада точно */
static bool substep(pl_t *p, double h, pl_sample_t *out)
{
    item_t *cur = at(p, 0);
    int nx = next_move(p, 0);
    /* ускорението и jerk по пътя зависят от посоката. Пред по-строго движение ускорението
     * се намалява навреме (с jerk J), за да влезе в него в границата му */
    double A = cur->A, J = cur->J, rem = cur->len - p->s;
    if (nx >= 0) {
        const item_t *n2 = cat(p, nx);
        double ex = fabs(p->a) - n2->A;
        if (n2->J < J && rem < p->v * 0.01) J = n2->J;
        if (ex > 0 && rem < p->v * (ex / fmin(J, n2->J) + 2 * h) + 1e-6) A = n2->A;
    }
    double Ae = fmax(A, fabs(p->a) - J * h);   /* над границата: слиза към нея с jerk J */
    double vt = p->hold ? 0 : veff(p, cur);
    if (p->hold && p->v <= 0 && fabs(p->a) < 1e-9) return false;   /* стои на паузата */

    /* желаното ускорение: стига vt точно с jerk J (смята се от скоростта в края на частта) */
    double vn = p->v + p->a * h, dv = vt - vn;
    double ades = (dv >= 0 ? 1 : -1) * sqrt(2 * J * fabs(dv));
    if (ades > A) ades = A;
    if (ades < -A) ades = -A;
    double jd = (ades - p->a) / h;
    if (jd > J) jd = J;
    if (jd < -J) jd = -J;
    /* най-големият jerk ≤ желания, при който от новото състояние може да се спре навреме */
    double j = jd, a1, v1, s1;
#define STATE(jj) do { a1 = p->a + (jj) * h; if (a1 > Ae) a1 = Ae; if (a1 < -Ae) a1 = -Ae; \
                       v1 = p->v + 0.5 * (p->a + a1) * h; if (v1 < 0) { v1 = 0; if (a1 < 0) a1 = 0; } \
                       s1 = p->s + p->v * h + (2 * p->a + a1) * h * h / 6; } while (0)
    STATE(j);
    if (!feasible(p, v1, a1, s1, A, J)) {
        double lo = -J, hi = jd;
        STATE(lo);
        if (feasible(p, v1, a1, s1, A, J)) {
            for (int it = 0; it < 12; it++) {
                double mid = 0.5 * (lo + hi);
                STATE(mid);
                if (feasible(p, v1, a1, s1, A, J)) lo = mid; else hi = mid;
            }
        }
        j = lo;
        STATE(j);
    }
#undef STATE
    double ds = s1 - p->s;
    if (ds < 0) ds = 0;
    p->v = v1;
    p->a = a1;
    /* спряло на по-малко от 1 µm преди спиране: там е (иначе би пълзяло безкрайно) */
    if (p->v <= 0 && fabs(p->a) < 1e-9 && (nx < 0 || cur->vj <= 0 || line_stop(p, cur, nx)) && cur->len - p->s < 1e-3) ds = cur->len - p->s;

    /* напред по пътя, през връзките */
    for (;;) {
        cur = at(p, 0);
        double left = cur->len - p->s;
        int n2 = next_move(p, 0);
        bool stop_end = n2 < 0, lstop = line_stop(p, cur, n2), zero = stop_end || cur->vj <= 0 || lstop;
        if (zero && (ds >= left || (left - ds < SNAP && p->v < 0.5 * J * h * h))) {   /* стига до спирането: точно там */
            p->s = 0;
            p->v = p->a = 0;
            for (int k = 0; k < GC_NAX; k++) p->pos[k] = cur->p0[k] + cur->u[k] * cur->len;
            pop(p);
            if (lstop && !stop_end) p->waiting = true;   /* единичен блок: чака Старт (pl_continue) */
            return false;   /* остатъкът от такта е на място (без скок в ускорението); събитията са в следващия */
        }
        if (ds < left) {
            p->s += ds;
            for (int k = 0; k < GC_NAX; k++) p->pos[k] = cur->p0[k] + cur->u[k] * p->s;
            return true;
        }
        ds -= left;
        pop(p);
        while (p->n && !at(p, 0)->move) {   /* събития без спиране по пътя */
            add_ev(out, &at(p, 0)->ev);
            pop(p);
        }
        p->s = 0;
    }
}

int pl_tick(pl_t *p, pl_sample_t *out)
{
    memset(out, 0, sizeof *out);
    memcpy(out->pos, p->pos, sizeof out->pos);
    if (p->done) return PL_DONE;
    if (p->waiting) return PL_WAIT;
    double dt = p->cfg.dt;
    if (p->dwelling) {
        p->dwell_left -= dt;
        if (p->dwell_left <= 1e-12) p->dwelling = false;
        return PL_SAMPLE;
    }
    /* събитията пред нас (машината е спряла, ако някое иска спиране) */
    while (p->n && !at(p, 0)->move) {
        item_t it = *at(p, 0);
        pop(p);
        out->line = it.line;
        add_ev(out, &it.ev);
        if (it.stop) { p->v = p->a = 0; }
        if (it.ev.kind == GC_END) { p->done = true; return PL_DONE; }
        if (it.ev.kind == GC_DWELL) { p->dwelling = it.ev.val > 0; p->dwell_left = it.ev.val; return PL_SAMPLE; }
        if (it.stop) { p->waiting = true; return PL_WAIT; }
        return PL_SAMPLE;   /* без спиране: по едно на такт (стоят между движенията, при v = 0 само на старта) */
    }
    if (!p->n) {
        p->v = p->a = 0;
        if (p->input_end) { p->done = true; return PL_DONE; }
        return PL_STARVED;
    }
    out->line = at(p, 0)->line;
    for (int k = 0; k < NSUB; k++)
        if (!p->n || !at(p, 0)->move || !substep(p, dt / NSUB, out)) break;
    memcpy(out->pos, p->pos, sizeof out->pos);
    out->v = p->v;
    out->a = p->a;
    return PL_SAMPLE;
}
