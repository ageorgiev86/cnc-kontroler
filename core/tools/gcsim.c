/* gcsim: G-code → команди на интерпретатора или (с --plan) позициите на планера на всеки такт.
 *   gcsim [--haas] [--axes XYZA] [--block-delete] [--summary] [--tool N:диаметър]…
 *         [--parts 1,2,3] [--from ред[:детайл]] [--safe-z Z]
 *         [--plan] [--limits V,A,J[,VZ,AZ,JZ]] [--tol мм] [--csv файл] файл.nc
 * --parts: нулите на детайлите (1 = G54); --from: „Изпълни от ред“ (редът и детайлът от 1).
 * --plan: през планера; отчита истинското време и проверява лимитите на всяка ос по позициите;
 *         --limits: мм/s, мм/s², мм/s³ за XY (и за Z); --csv: t,x,y,z,v,a,ред на всеки такт.
 * Изходът е по един ред на команда (или обобщение с --summary); грешката е на stderr с ред и колона. */
#include "cnc/gcode.h"
#include "cnc/planner.h"
#include "gc_test_opts.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned axes;
    bool summary;
    long n[GC_END + 1];        /* брой команди по вид */
    double pos[GC_NAX];        /* текуща позиция (за дължината на дъгите) */
    double lo[3], hi[3];       /* граници на рязането (подаване и дъги) */
    double len_feed, len_rapid, t_feed;   /* мм, мм, мин */
    int tools[64], ntools;
    bool plan;
    gc_cmd_t *cmds;            /* за --plan: всички команди */
    long ncmds, capcmds;
} sim_t;

static void bound(sim_t *s, const double *p)
{
    for (int a = 0; a < 3; a++) {
        if (p[a] < s->lo[a]) s->lo[a] = p[a];
        if (p[a] > s->hi[a]) s->hi[a] = p[a];
    }
}

/* дължина на дъгата (със спиралата и допълнителните обороти) */
static double arc_len(const double *st, const gc_cmd_t *c)
{
    int a0 = GC_X, a1 = GC_Y, lin = GC_Z;
    if (c->plane == GC_PLANE_ZX) { a0 = GC_Z; a1 = GC_X; lin = GC_Y; }
    if (c->plane == GC_PLANE_YZ) { a0 = GC_Y; a1 = GC_Z; lin = GC_X; }
    double r = hypot(st[a0] - c->ctr[a0], st[a1] - c->ctr[a1]);
    double t0 = atan2(st[a1] - c->ctr[a1], st[a0] - c->ctr[a0]);
    double t1 = atan2(c->tgt[a1] - c->ctr[a1], c->tgt[a0] - c->ctr[a0]);
    double sw = c->cw ? t0 - t1 : t1 - t0;
    while (sw <= 1e-9) sw += 2 * M_PI;
    sw += 2 * M_PI * c->turns;
    return hypot(r * sw, c->tgt[lin] - st[lin]);
}

static void on_cmd(void *ctx, const gc_cmd_t *c)
{
    sim_t *s = ctx;
    if (!s->summary) {
        char buf[256];
        gc_cmd_format(c, s->axes, buf, sizeof buf);
        puts(buf);
    }
    s->n[c->kind]++;
    if (s->plan) {
        if (s->ncmds == s->capcmds) {
            s->capcmds = s->capcmds ? 2 * s->capcmds : 4096;
            gc_cmd_t *nc = realloc(s->cmds, s->capcmds * sizeof *nc);
            if (!nc) { fprintf(stderr, "няма памет\n"); exit(2); }
            s->cmds = nc;
        }
        s->cmds[s->ncmds++] = *c;
    }
    if (c->kind == GC_RAPID || c->kind == GC_FEED || c->kind == GC_ARC) {
        double L = c->kind == GC_ARC ? arc_len(s->pos, c)
                                     : sqrt(pow(c->tgt[0] - s->pos[0], 2) + pow(c->tgt[1] - s->pos[1], 2) + pow(c->tgt[2] - s->pos[2], 2));
        if (c->kind == GC_RAPID) s->len_rapid += L;
        else {
            s->len_feed += L;
            if (c->feed > 0) s->t_feed += L / c->feed;
            bound(s, s->pos);
            bound(s, c->tgt);
        }
        memcpy(s->pos, c->tgt, sizeof s->pos);
    }
    if (c->kind == GC_TOOL_CHANGE && s->ntools < 64) {
        bool seen = false;
        for (int i = 0; i < s->ntools; i++) seen |= s->tools[i] == c->ival;
        if (!seen) s->tools[s->ntools++] = c->ival;
    }
}

static void print_summary(const sim_t *s, int nlines)
{
    static const char *const KN[] = {"RAPID", "FEED", "ARC", "DWELL", "SPINDLE", "COOLANT", "TOOL_PREP", "TOOL_CHANGE",
                                     "TLO", "ORIENT", "OVERRIDE", "PATH", "WCS", "SET_WCS", "SET_TOOL", "HOME", "PAUSE", "PART", "END"};
    printf("редове: %d\n", nlines);
    printf("команди:");
    for (int k = 0; k <= GC_END; k++)
        if (s->n[k]) printf(" %s=%ld", KN[k], s->n[k]);
    printf("\nинструменти:");
    for (int i = 0; i < s->ntools; i++) printf(" T%d", s->tools[i]);
    if (s->lo[0] <= s->hi[0])
        printf("\nрязане: X %.3f … %.3f  Y %.3f … %.3f  Z %.3f … %.3f\n", s->lo[0], s->hi[0], s->lo[1], s->hi[1], s->lo[2], s->hi[2]);
    else
        printf("\nрязане: няма\n");
    printf("път: подаване %.1f мм, бърз ход %.1f мм\n", s->len_feed, s->len_rapid);
    int t = (int)lround(s->t_feed * 60);
    printf("време на подаването (без ускорения и бързи ходове): %d:%02d:%02d\n", t / 3600, t / 60 % 60, t % 60);
    printf("край: X %.3f Y %.3f Z %.3f\n", s->pos[0], s->pos[1], s->pos[2]);
}

/* --plan: командите през планера; проверка по позициите (както ги получава STM32) */
static int run_plan(const sim_t *s, const pl_config_t *pc, const char *csv)
{
    double start[GC_NAX] = {0};
    pl_t *p = pl_create(pc, start);
    FILE *f = csv ? fopen(csv, "w") : NULL;
    if (csv && !f) { perror(csv); pl_destroy(p); return 2; }
    if (f) fprintf(f, "t,x,y,z,v,a,line\n");
    double prev[GC_NAX] = {0}, pv[GC_NAX] = {0}, vmax_ax[GC_NAX] = {0}, amax_ax[GC_NAX] = {0}, vpath = 0;
    long i = 0, t = 0, waits = 0, bad = 0, idle = 0;
    int bad_line = 0, bad_ax = 0;
    double bad_v = 0;
    for (;;) {
        while (i < s->ncmds && pl_push(p, &s->cmds[i]) == 0) i++;
        if (i >= s->ncmds) pl_input_end(p);
        pl_sample_t o;
        int rc = pl_tick(p, &o);
        if (rc == PL_DONE) break;
        if (rc == PL_STARVED) { if (i >= s->ncmds) break; continue; }
        if (rc == PL_WAIT) { waits++; pl_continue(p); continue; }
        t++;
        if (o.v <= 0) idle++;
        if (o.v > vpath) vpath = o.v;
        for (int k = 0; k < GC_NAX; k++) {
            double v = (o.pos[k] - prev[k]) / pc->dt, a = (v - pv[k]) / pc->dt;
            if (fabs(v) > vmax_ax[k]) vmax_ax[k] = fabs(v);
            if (fabs(a) > amax_ax[k]) amax_ax[k] = fabs(a);
            if (fabs(v) > pc->vmax[k] * 1.001 + 1e-6) {
                if (!bad) { bad_line = o.line; bad_ax = k; bad_v = v; }
                bad++;
            }
            prev[k] = o.pos[k];
            pv[k] = v;
        }
        if (f) fprintf(f, "%.3f,%.4f,%.4f,%.4f,%.3f,%.1f,%d\n", t * pc->dt, o.pos[0], o.pos[1], o.pos[2], o.v, o.a, o.line + 1);
    }
    if (f) fclose(f);
    pl_destroy(p);
    int sec = (int)lround(t * pc->dt);
    printf("планер: %d:%02d:%02d (%ld такта, %ld на място), спирания за събития %ld\n", sec / 3600, sec / 60 % 60, sec % 60, t, idle, waits);
    printf("най-голяма скорост: по пътя %.1f мм/s; X %.1f  Y %.1f  Z %.1f мм/s\n", vpath, vmax_ax[0], vmax_ax[1], vmax_ax[2]);
    printf("най-голямо ускорение по ос (от позициите, с ъглите): X %.0f  Y %.0f  Z %.0f мм/s²\n", amax_ax[0], amax_ax[1], amax_ax[2]);
    double e = 0;
    for (int k = 0; k < 3; k++) e = fmax(e, fabs(prev[k] - s->pos[k]));
    printf("край: X %.3f Y %.3f Z %.3f (разлика с програмата %.6f мм)\n", prev[0], prev[1], prev[2], e);
    if (bad) printf("ГРЕШКА: %ld такта над скоростта на оста, първият на ред %d, ос %c: %.2f мм/s\n", bad, bad_line + 1, "XYZABC"[bad_ax], bad_v);
    if (e > 1e-6) printf("ГРЕШКА: планерът не стига края на програмата\n");
    return bad || e > 1e-6;
}

int main(int argc, char **argv)
{
    sim_t sim;
    memset(&sim, 0, sizeof sim);
    for (int a = 0; a < 3; a++) { sim.lo[a] = 1e30; sim.hi[a] = -1e30; }
    gc_config_t cfg = {GC_DIALECT_MACH3, 0x7, 0, false, on_cmd, &sim};
    const char *path = NULL;
    int parts[8];
    gc_job_t job = {parts, 0, -1, 0, 0, 5};
    gc_tables_t *tab = calloc(1, sizeof *tab);
    /* по подразбиране: рутер, XY 10 м/мин, Z 5 м/мин */
    pl_config_t pc = {.tol = 0.02, .arc_tol = 0.002, .dt = 0.001};
    for (int k = 0; k < GC_NAX; k++) { pc.vmax[k] = 166.7; pc.amax[k] = 1000; pc.jmax[k] = 50000; }
    pc.vmax[GC_Z] = 83.3; pc.amax[GC_Z] = 500; pc.jmax[GC_Z] = 25000;
    const char *csv = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--haas")) cfg.dialect = GC_DIALECT_HAAS;
        else if (!strcmp(argv[i], "--block-delete")) cfg.block_delete = true;
        else if (!strcmp(argv[i], "--summary")) sim.summary = true;
        else if (!strcmp(argv[i], "--plan")) sim.plan = sim.summary = true;
        else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv = argv[++i];
        else if (!strcmp(argv[i], "--tol") && i + 1 < argc) pc.tol = atof(argv[++i]);
        else if (!strcmp(argv[i], "--limits") && i + 1 < argc) {
            double l[6];
            int n = sscanf(argv[++i], "%lf,%lf,%lf,%lf,%lf,%lf", &l[0], &l[1], &l[2], &l[3], &l[4], &l[5]);
            if (n != 3 && n != 6) { fprintf(stderr, "--limits V,A,J[,VZ,AZ,JZ]\n"); return 2; }
            for (int k = 0; k < GC_NAX; k++) {
                const double *q = k == GC_Z && n == 6 ? l + 3 : l;
                pc.vmax[k] = q[0]; pc.amax[k] = q[1]; pc.jmax[k] = q[2];
            }
        }
        else if (!strcmp(argv[i], "--axes") && i + 1 < argc) cfg.axes = gc_axes_mask(argv[++i], &cfg.rotary);
        else if (!strcmp(argv[i], "--parts") && i + 1 < argc) job.nparts = gc_parse_parts(argv[++i], parts, 8);
        else if (!strcmp(argv[i], "--from") && i + 1 < argc) gc_parse_from(argv[++i], &job.start_line, &job.start_part);
        else if (!strcmp(argv[i], "--safe-z") && i + 1 < argc) job.safe_z = atof(argv[++i]);
        else if (!strcmp(argv[i], "--tool") && i + 1 < argc) {   /* --tool 3:3.17 = T3 с диаметър 3,17 мм */
            int t; double d;
            if (sscanf(argv[++i], "%d:%lf", &t, &d) != 2 || t < 0 || t >= GC_TOOL_N) { fprintf(stderr, "--tool N:диаметър\n"); return 2; }
            tab->tool[t].dia = d;
        }
        else if (argv[i][0] != '-') path = argv[i];
        else { fprintf(stderr, "непозната опция %s\n", argv[i]); return 2; }
    }
    if (!path) { fprintf(stderr, "употреба: gcsim [--haas] [--axes XYZA] [--block-delete] [--summary] [--tool N:диаметър] [--parts 1,2,3] [--from ред[:детайл]] [--safe-z Z] [--plan] [--limits V,A,J[,VZ,AZ,JZ]] [--tol мм] [--csv файл] файл.nc\n"); return 2; }
    char *text = gc_read_file(path);
    if (!text) { perror(path); return 2; }
    sim.axes = cfg.axes;
    gc_interp_t in;
    gc_err_t e;
    gc_interp_init(&in, &cfg, tab, NULL);
    int r = gc_run_job(&in, text, &job, &e);
    if (sim.summary) {
        int nl = 0;
        for (const char *p = text; *p; p++) nl += *p == '\n';
        print_summary(&sim, nl);
        if (r && e.col >= 0) printf("ГРЕШКА: ред %d, колона %d: %s\n", e.line + 1, e.col + 1, e.msg);
        else if (r) printf("ГРЕШКА: ред %d: %s\n", e.line + 1, e.msg);
        if (!r && sim.plan) r = run_plan(&sim, &pc, csv);
    } else if (r) {
        if (e.col >= 0) fprintf(stderr, "%s:%d:%d: %s\n", path, e.line + 1, e.col + 1, e.msg);
        else fprintf(stderr, "%s:%d: %s\n", path, e.line + 1, e.msg);
    }
    free(text);
    free(tab);
    free(sim.cmds);
    return r ? 1 : 0;
}
