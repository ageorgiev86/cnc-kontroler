/* Тестове на планера: по позициите на всеки такт се проверяват лимитите и точността. */
#include "cnc/planner.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, total;
static bool zslow;   /* Z с по-ниски лимити (както на рутер) */
#define CHECK(c, ...) do { total++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static pl_config_t cfg(void)
{
    pl_config_t c;
    memset(&c, 0, sizeof c);
    for (int k = 0; k < GC_NAX; k++) { c.vmax[k] = 100; c.amax[k] = 800; c.jmax[k] = 20000; }
    if (zslow) { c.vmax[2] = 40; c.amax[2] = 300; c.jmax[2] = 8000; }
    c.tol = 0.02;
    c.arc_tol = 0.002;
    c.dt = 0.001;
    return c;
}

static gc_cmd_t mv(gc_kind_t k, double x, double y, double z, double f)
{
    gc_cmd_t c;
    memset(&c, 0, sizeof c);
    c.kind = k;
    c.tgt[0] = x; c.tgt[1] = y; c.tgt[2] = z;
    c.feed = f;
    return c;
}

typedef struct {
    double vmax_path, amax_path, jmax_path;   /* най-голямото видяно */
    double vaxis_max;                         /* най-голямата скорост по ос */
    double vax[GC_NAX];                       /* по всяка ос */
    double end[GC_NAX];
    long ticks, events, waits;
    double min_v_mid;                         /* най-малката скорост в средата (без началото и края) */
    double rmin, rmax;                        /* за окръжността: разстояние от центъра */
    double cx, cy;
    bool circle;
    double hold_at, release_at, ovr_at, ovr;  /* s */
    long stop_ticks;                          /* такта с v = 0 в движение (без събития) */
} res_t;

/* подава командите постепенно (по stream наведнъж, 0 = всички) и върти до края */
static res_t run(const gc_cmd_t *cmds, int n, int stream, res_t r)
{
    pl_config_t c = cfg();
    double start[GC_NAX] = {0};
    pl_t *p = pl_create(&c, start);
    int i = 0;
    double prev[GC_NAX] = {0}, pa = 0;
    r.vmax_path = r.amax_path = r.jmax_path = r.vaxis_max = 0;
    r.min_v_mid = 1e9;
    r.rmin = 1e9; r.rmax = 0;
    long t = 0;
    for (;;) {
        int lim = stream ? (i + stream < n ? i + stream : n) : n;
        while (i < lim && pl_push(p, &cmds[i]) == 0) i++;
        if (i >= n) pl_input_end(p);
        double now = t * c.dt;
        if (r.hold_at > 0 && now >= r.hold_at && now < r.release_at) pl_hold(p, true);
        if (r.hold_at > 0 && now >= r.release_at) pl_hold(p, false);
        if (r.ovr_at > 0 && now >= r.ovr_at) pl_override(p, r.ovr, 1);
        pl_sample_t s;
        int rc = pl_tick(p, &s);
        if (rc == PL_DONE) break;
        if (rc == PL_STARVED) { if (i >= n) break; continue; }
        if (rc == PL_WAIT) { r.waits++; pl_continue(p); continue; }
        t++;
        if (s.has_ev) r.events++;
        double v[GC_NAX];
        for (int k = 0; k < GC_NAX; k++) {
            v[k] = (s.pos[k] - prev[k]) / c.dt;
            if (fabs(v[k]) > r.vaxis_max) r.vaxis_max = fabs(v[k]);
            if (fabs(v[k]) > r.vax[k]) r.vax[k] = fabs(v[k]);
            prev[k] = s.pos[k];
        }
        if (s.v > r.vmax_path) r.vmax_path = s.v;
        if (fabs(s.a) > r.amax_path) r.amax_path = fabs(s.a);
        double jj = fabs(s.a - pa) / c.dt;
        if (jj > r.jmax_path) r.jmax_path = jj;
        pa = s.a;
        if (r.circle && t > 1) {
            double d = hypot(s.pos[0] - r.cx, s.pos[1] - r.cy);
            if (d < r.rmin) r.rmin = d;
            if (d > r.rmax) r.rmax = d;
        }
        if (t > 300 && pl_ahead(p) > 5 && s.v < r.min_v_mid) r.min_v_mid = s.v;   /* без началото и края */
        if (t > 20000000) { printf("FAIL: безкраен цикъл\n"); fails++; break; }
    }
    memcpy(r.end, prev, sizeof r.end);
    r.ticks = t;
    pl_destroy(p);
    return r;
}

/* k: ускорението и jerk по пътя могат да са до k пъти над тези на оста (k = 1 / най-голямата
 * съставка на посоката: по диагонал оста носи само част от тях) */
static void limits_k(const res_t *r, const char *name, double vlim, double k)
{
    pl_config_t c = cfg();
    CHECK(r->vaxis_max <= c.vmax[0] * 1.001 + 1e-6, "%s: скорост по ос %.3f > %.0f", name, r->vaxis_max, c.vmax[0]);
    CHECK(r->vmax_path <= vlim * 1.001 + 1e-6, "%s: скорост по пътя %.3f > %.3f", name, r->vmax_path, vlim);
    CHECK(r->amax_path <= k * c.amax[0] * 1.001 + 1e-6, "%s: ускорение %.1f > %.0f", name, r->amax_path, k * c.amax[0]);
    CHECK(r->jmax_path <= k * c.jmax[0] * 1.001 + 1e-3, "%s: jerk %.0f > %.0f", name, r->jmax_path, k * c.jmax[0]);
}

static void limits(const res_t *r, const char *name, double vlim) { limits_k(r, name, vlim, 1); }

static void end_at(const res_t *r, const char *name, double x, double y, double z)
{
    CHECK(fabs(r->end[0] - x) < 1e-6 && fabs(r->end[1] - y) < 1e-6 && fabs(r->end[2] - z) < 1e-6,
          "%s: краят е %.6f %.6f %.6f вместо %.3f %.3f %.3f", name, r->end[0], r->end[1], r->end[2], x, y, z);
}

int main(void)
{
    res_t z;
    memset(&z, 0, sizeof z);

    /* 1. дълга линия F3000 (50 мм/s) */
    gc_cmd_t l1[] = {mv(GC_FEED, 100, 0, 0, 3000)};
    res_t r = run(l1, 1, 0, z);
    limits(&r, "линия", 50);
    end_at(&r, "линия", 100, 0, 0);
    CHECK(r.vmax_path > 49.9, "линия: не стига F (%.3f)", r.vmax_path);
    printf("линия 100 мм F3000: %.3f s\n", r.ticks / 1000.0);
    CHECK(r.ticks > 2000 && r.ticks < 2200, "линия: времето %ld ms", r.ticks);

    /* 2. къса линия: не стига F */
    gc_cmd_t l2[] = {mv(GC_FEED, 1, 0, 0, 6000)};
    r = run(l2, 1, 0, z);
    limits(&r, "къса линия", 100);
    end_at(&r, "къса линия", 1, 0, 0);

    /* 3. квадрат 20 × 20 с G64 (допуск 0,02) и с G61 */
    gc_cmd_t sq[] = {mv(GC_FEED, 20, 0, 0, 6000), mv(GC_FEED, 20, 20, 0, 6000), mv(GC_FEED, 0, 20, 0, 6000), mv(GC_FEED, 0, 0, 0, 6000)};
    r = run(sq, 4, 0, z);
    limits(&r, "квадрат G64", 100);
    end_at(&r, "квадрат G64", 0, 0, 0);
    long t64 = r.ticks;
    gc_cmd_t g61 = {0};
    g61.kind = GC_PATH_MODE; g61.ival = 610;
    gc_cmd_t sq61[] = {g61, sq[0], sq[1], sq[2], sq[3]};
    r = run(sq61, 5, 0, z);
    limits(&r, "квадрат G61", 100);
    end_at(&r, "квадрат G61", 0, 0, 0);
    printf("квадрат: G64 %.3f s, G61 %.3f s\n", t64 / 1000.0, r.ticks / 1000.0);
    CHECK(r.ticks > t64, "G61 трябва да е по-бавно от G64");

    /* 4. окръжност R10, F6000: хордите в допуска, скорост до sqrt(A·R) */
    gc_cmd_t c0 = mv(GC_FEED, 10, 0, 0, 6000);
    gc_cmd_t arc = mv(GC_ARC, 10, 0, 0, 6000);
    arc.ctr[0] = 0; arc.ctr[1] = 0; arc.plane = GC_PLANE_XY; arc.cw = false;
    gc_cmd_t circ[] = {c0, arc};
    res_t rc0 = z;
    rc0.circle = true;
    r = run(circ, 2, 0, rc0);
    double vc = sqrt(800 * 10);
    limits_k(&r, "окръжност", 100, M_SQRT2);
    CHECK(r.vmax_path <= vc * 1.001, "окръжност: %.2f > sqrt(A·R) = %.2f", r.vmax_path, vc);
    end_at(&r, "окръжност", 10, 0, 0);
    /* след подхода по X точките са на окръжността (хордите са вътре, до 0,002 мм) */
    CHECK(r.rmax <= 10 + 1e-6, "окръжност: точка извън R (%.6f)", r.rmax);

    /* 5. 2000 къси сегмента (0,05 мм, като ArtCAM): скоростта не пада до 0 между тях */
    static gc_cmd_t poly[2000];
    for (int i = 0; i < 2000; i++) {
        double a = 2 * M_PI * (i + 1) / 2000.0;
        poly[i] = mv(GC_FEED, 15 * cos(a) - 15, 15 * sin(a), 0, 3000);
    }
    r = run(poly, 2000, 0, z);
    limits_k(&r, "къси сегменти", 50, M_SQRT2);
    printf("2000 сегмента по 0,047 мм (окръжност R15, F3000): %.3f s, най-ниска скорост в средата %.2f мм/s\n", r.ticks / 1000.0, r.min_v_mid);
    CHECK(r.min_v_mid > 40, "къси сегменти: скоростта пада до %.2f", r.min_v_mid);

    /* 6. пауза на подаването и продължаване */
    res_t rh = z;
    rh.hold_at = 0.6; rh.release_at = 1.5;
    r = run(l1, 1, 0, rh);
    limits(&r, "пауза", 50);
    end_at(&r, "пауза", 100, 0, 0);

    /* 7. оверрайд 100 → 40 % в движение */
    res_t ro = z;
    ro.ovr_at = 0.8; ro.ovr = 0.4;
    r = run(l1, 1, 0, ro);
    limits(&r, "оверрайд", 50);
    end_at(&r, "оверрайд", 100, 0, 0);

    /* 8. събития: G4 0,5 s и шпиндел (спиране на място) */
    gc_cmd_t dw = {0};
    dw.kind = GC_DWELL; dw.val = 0.5;
    gc_cmd_t sp = {0};
    sp.kind = GC_SPINDLE; sp.ival = 1; sp.val = 10000;
    gc_cmd_t ev[] = {mv(GC_FEED, 10, 0, 0, 3000), dw, mv(GC_FEED, 20, 0, 0, 3000), sp, mv(GC_FEED, 30, 0, 0, 3000)};
    r = run(ev, 5, 0, z);
    limits(&r, "събития", 50);
    end_at(&r, "събития", 30, 0, 0);
    CHECK(r.waits == 1, "събития: шпинделът трябва да спре и да чака (%ld)", r.waits);

    /* 9. командите идват по една (както от интерпретатора): краят на буфера е спиране */
    r = run(sq, 4, 1, z);
    limits(&r, "по една", 100);
    end_at(&r, "по една", 0, 0, 0);

    /* 10. бърз ход по диагонал: всяка ос до своя лимит */
    gc_cmd_t rp[] = {mv(GC_RAPID, 200, 100, -50, 0)};
    r = run(rp, 1, 0, z);
    limits_k(&r, "бърз ход", 1e9, sqrt(200 * 200 + 100 * 100 + 50 * 50) / 200);
    end_at(&r, "бърз ход", 200, 100, -50);

    /* 11. бърз ход по диагонал и спускане по по-бавната Z: всяка ос в своя лимит и на връзката */
    zslow = true;
    gc_cmd_t pz[] = {mv(GC_RAPID, 0, 0, 15, 0), mv(GC_RAPID, 22, 22, 15, 0), mv(GC_FEED, 22, 22, -1, 150), mv(GC_FEED, 60, 22, -1, 2000),
                     mv(GC_RAPID, 60, 22, 15, 0), mv(GC_RAPID, 0, 0, 15, 0)};
    r = run(pz, 6, 0, z);
    CHECK(r.vax[0] <= 100.1 && r.vax[1] <= 100.1 && r.vax[2] <= 40.04, "бавна Z: скорост по осите %.2f %.2f %.2f", r.vax[0], r.vax[1], r.vax[2]);
    end_at(&r, "бавна Z", 0, 0, 15);
    zslow = false;

    printf("%d проверки, %d неуспешни\n", total, fails);
    return fails ? 1 : 0;
}
