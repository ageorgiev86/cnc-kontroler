/* gcsim: G-code → команди на интерпретатора (засега), по-късно и сегменти на планера.
 *   gcsim [--haas] [--axes XYZA] [--block-delete] [--summary] [--tool N:диаметър]… файл.nc
 * Изходът е по един ред на команда (или обобщение с --summary); грешката е на stderr с ред и колона. */
#include "cnc/gcode.h"
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
                                     "TLO", "ORIENT", "OVERRIDE", "PATH", "WCS", "SET_WCS", "SET_TOOL", "HOME", "PAUSE", "END"};
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

int main(int argc, char **argv)
{
    sim_t sim;
    memset(&sim, 0, sizeof sim);
    for (int a = 0; a < 3; a++) { sim.lo[a] = 1e30; sim.hi[a] = -1e30; }
    gc_config_t cfg = {GC_DIALECT_MACH3, 0x7, 0, false, on_cmd, &sim};
    const char *path = NULL;
    gc_tables_t *tab = calloc(1, sizeof *tab);
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--haas")) cfg.dialect = GC_DIALECT_HAAS;
        else if (!strcmp(argv[i], "--block-delete")) cfg.block_delete = true;
        else if (!strcmp(argv[i], "--summary")) sim.summary = true;
        else if (!strcmp(argv[i], "--axes") && i + 1 < argc) cfg.axes = gc_axes_mask(argv[++i], &cfg.rotary);
        else if (!strcmp(argv[i], "--tool") && i + 1 < argc) {   /* --tool 3:3.17 = T3 с диаметър 3,17 мм */
            int t; double d;
            if (sscanf(argv[++i], "%d:%lf", &t, &d) != 2 || t < 0 || t >= GC_TOOL_N) { fprintf(stderr, "--tool N:диаметър\n"); return 2; }
            tab->tool[t].dia = d;
        }
        else if (argv[i][0] != '-') path = argv[i];
        else { fprintf(stderr, "непозната опция %s\n", argv[i]); return 2; }
    }
    if (!path) { fprintf(stderr, "употреба: gcsim [--haas] [--axes XYZA] [--block-delete] [--summary] [--tool N:диаметър] файл.nc\n"); return 2; }
    char *text = gc_read_file(path);
    if (!text) { perror(path); return 2; }
    sim.axes = cfg.axes;
    gc_interp_t in;
    gc_err_t e;
    gc_interp_init(&in, &cfg, tab, NULL);
    int r = gc_run_text(&in, text, &e);
    if (sim.summary) {
        int nl = 0;
        for (const char *p = text; *p; p++) nl += *p == '\n';
        print_summary(&sim, nl);
        if (r && e.col >= 0) printf("ГРЕШКА: ред %d, колона %d: %s\n", e.line + 1, e.col + 1, e.msg);
        else if (r) printf("ГРЕШКА: ред %d: %s\n", e.line + 1, e.msg);
    } else if (r) {
        if (e.col >= 0) fprintf(stderr, "%s:%d:%d: %s\n", path, e.line + 1, e.col + 1, e.msg);
        else fprintf(stderr, "%s:%d: %s\n", path, e.line + 1, e.msg);
    }
    free(text);
    free(tab);
    return r ? 1 : 0;
}
