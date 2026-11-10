#include "program.h"

#include "cnc/gcode.h"
#include "config.h"
#include "machine.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

program_t P;
static program_t *G = &P;   /* програмата, в която се чете */

#define RAPID_FEED 6000.0f   /* мм/мин, за номиналното време */
#define TOOL_TIME 8.0f       /* s за смяна на инструмент */
#define SPINDLE_TIME 2.0f
#define ARC_STEP_DEG 4.0f
#define DIAL_HAAS() (!strcmp(cfg_str("mach.gDial"), "haas"))

/* растящи масиви */
#define GROW(arr, n, cap)                                              \
    do {                                                               \
        if ((n) >= (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 64;                            \
            void *nw = realloc((arr), (size_t)(cap) * sizeof *(arr));  \
            if (!nw) abort();                                          \
            (arr) = nw;                                                \
        }                                                              \
    } while (0)


static void program_free_g(void);
void program_free_into(program_t *g)
{
    program_t *sv = G;
    G = g;
    program_free_g();
    G = sv;
}

static void program_free_g(void)
{
    free(G->text);
    free(G->line);
    free(G->pt);
    free(G->cum);
    free(G->op);
    memset(G, 0, sizeof *G);
}

static int add_pt(p3_t p, float cum)
{
    if (G->npt >= G->cap_pt) {
        G->cap_pt = G->cap_pt ? G->cap_pt * 2 : 256;
        p3_t *a = realloc(G->pt, (size_t)G->cap_pt * sizeof *a);
        float *c = realloc(G->cum, (size_t)G->cap_pt * sizeof *c);
        if (!a || !c) abort();
        G->pt = a;
        G->cum = c;
    }
    G->pt[G->npt] = p;
    G->cum[G->npt] = cum;
    return G->npt++;
}

static op_t *add_op(op_kind_t k, int line)
{
    GROW(G->op, G->nop, G->cap_op);
    op_t *o = &G->op[G->nop++];
    memset(o, 0, sizeof *o);
    o->kind = k;
    o->line = line;
    return o;
}

static void note_tool(int t)
{
    for (int i = 0; i < G->ntools; i++)
        if (G->tools[i] == t) return;
    if (G->ntools < PROG_MAX_TOOLS) G->tools[G->ntools++] = t;
}

static void bound(p3_t p)
{
    if (p.x < G->x0) G->x0 = p.x;
    if (p.x > G->x1) G->x1 = p.x;
    if (p.y < G->y0) G->y0 = p.y;
    if (p.y > G->y1) G->y1 = p.y;
    if (p.z < G->z0) G->z0 = p.z;
    if (p.z > G->z1) G->z1 = p.z;
}

/* права линия като отделен ход */
static void line_op(int li, p3_t a, p3_t b, bool rapid, float feed, int tool, float *t)
{
    float len = hypotf(hypotf(b.x - a.x, b.y - a.y), b.z - a.z);
    if (len <= 0) return;
    op_t *o = add_op(OP_MOVE, li);
    o->rapid = rapid;
    o->feed = feed;
    o->tool = tool;
    o->p0 = add_pt(a, 0);
    add_pt(b, len);
    if (!rapid) { bound(a); bound(b); }   /* бързите ходове (и G53 до горе) не разширяват границите */
    o->np = 2;
    o->len = len;
    o->t0 = *t;
    *t += len / ((rapid ? RAPID_FEED : (feed > 1 ? feed : 1)) / 60.0f);
    o->t1 = *t;
}

/* ---------- командите на ядрото (core/: интерпретаторът) → ходове за изгледа ---------- */
static struct {
    double pos[GC_NAX];    /* машинни координати (както идват от ядрото) */
    int tool, wcs;         /* wcs: действащата нула (1 = G54 … 6 = G59; другите без отместване) */
    float t;
} st;

/* машинна → работна точка за изгледа (по нулата, избрана в програмата) */
static p3_t p3(const double *a)
{
    const double *o = st.wcs >= 1 && st.wcs <= 6 ? M.wcs_off[st.wcs] : NULL;
    return (p3_t){(float)(a[0] - (o ? o[0] : 0)), (float)(a[1] - (o ? o[1] : 0)), (float)(a[2] - (o ? o[2] : 0))};
}

static void arc_op(const gc_cmd_t *c)
{
    int a0 = GC_X, a1 = GC_Y, lin = GC_Z;
    if (c->plane == GC_PLANE_ZX) { a0 = GC_Z; a1 = GC_X; lin = GC_Y; }
    if (c->plane == GC_PLANE_YZ) { a0 = GC_Y; a1 = GC_Z; lin = GC_X; }
    double r = hypot(st.pos[a0] - c->ctr[a0], st.pos[a1] - c->ctr[a1]);
    double r1 = hypot(c->tgt[a0] - c->ctr[a0], c->tgt[a1] - c->ctr[a1]);
    double t0 = atan2(st.pos[a1] - c->ctr[a1], st.pos[a0] - c->ctr[a0]);
    double t1 = atan2(c->tgt[a1] - c->ctr[a1], c->tgt[a0] - c->ctr[a0]);
    double sw = c->cw ? t0 - t1 : t1 - t0;
    while (sw <= 1e-9) sw += 2 * M_PI;
    sw += 2 * M_PI * c->turns;
    if (c->cw) sw = -sw;
    int steps = (int)ceil(fabs(sw) * 180 / M_PI / ARC_STEP_DEG);
    if (steps < 2) steps = 2;
    op_t *o = add_op(OP_MOVE, c->line);
    o->feed = (float)c->feed;
    o->tool = st.tool;
    o->p0 = add_pt(p3(st.pos), 0);
    bound(p3(st.pos));
    float len = 0;
    p3_t prev = p3(st.pos);
    for (int k = 1; k <= steps; k++) {
        double f = (double)k / steps, g = t0 + sw * f, rr = r + (r1 - r) * f, q[GC_NAX];
        memcpy(q, st.pos, sizeof q);
        q[a0] = c->ctr[a0] + rr * cos(g);
        q[a1] = c->ctr[a1] + rr * sin(g);
        q[lin] = st.pos[lin] + (c->tgt[lin] - st.pos[lin]) * f;
        if (k == steps) memcpy(q, c->tgt, sizeof q);
        p3_t pq = p3(q);
        len += hypotf(hypotf(pq.x - prev.x, pq.y - prev.y), pq.z - prev.z);
        add_pt(pq, len);
        bound(pq);
        prev = pq;
    }
    o->np = G->npt - o->p0;
    o->len = len;
    o->t0 = st.t;
    st.t += len / ((c->feed > 1 ? (float)c->feed : 1) / 60.0f);
    o->t1 = st.t;
}

static void on_cmd(void *ctx, const gc_cmd_t *c)
{
    (void)ctx;
    op_t *o;
    switch (c->kind) {
    case GC_RAPID: case GC_FEED:
        line_op(c->line, p3(st.pos), p3(c->tgt), c->kind == GC_RAPID, (float)c->feed, st.tool, &st.t);
        memcpy(st.pos, c->tgt, sizeof st.pos);
        break;
    case GC_ARC:
        arc_op(c);
        memcpy(st.pos, c->tgt, sizeof st.pos);
        break;
    case GC_TOOL_CHANGE:
        o = add_op(OP_TOOL, c->line);
        o->tool = st.tool = c->ival;
        note_tool(c->ival);
        o->t0 = st.t; st.t += TOOL_TIME; o->t1 = st.t;
        break;
    case GC_SPINDLE:
        o = add_op(OP_SPINDLE, c->line);
        o->s = c->ival ? (c->val > 0 ? (int)c->val : 10000) : 0;
        o->t0 = st.t;
        if (c->ival) st.t += SPINDLE_TIME;
        o->t1 = st.t;
        break;
    case GC_COOLANT:
        o = add_op(OP_COOL, c->line);
        o->on = c->ival != 0;
        o->t0 = o->t1 = st.t;
        break;
    case GC_DWELL:
        st.t += (float)c->val;
        break;
    case GC_WCS_SELECT:
        st.wcs = c->ival;
        break;
    default:
        break;
    }
}

/* осите от настройката „Оси“ на фрезата */
static unsigned axes_mask(unsigned *rot)
{
    const char *a = cfg_str("mach.miAx");
    unsigned m = 7;
    if (!strcmp(a, "4a")) m |= 1u << GC_A;
    else if (!strcmp(a, "5ac")) m |= 1u << GC_A | 1u << GC_C;
    else if (!strcmp(a, "5bc")) m |= 1u << GC_B | 1u << GC_C;
    else if (!strcmp(a, "5ab")) m |= 1u << GC_A | 1u << GC_B;
    else if (!strcmp(a, "6")) m |= 7u << GC_A;
    *rot = m & (7u << GC_A);
    return m;
}

static void parse(void)
{
    memset(&st, 0, sizeof st);
    st.wcs = M.wcs >= 1 && M.wcs <= 6 ? M.wcs : 1;
    for (int a = 0; a < 3; a++) st.pos[a] = M.wcs_off[st.wcs][a];   /* началото: над нулата на Z 25 */
    st.pos[GC_Z] += 25;
    G->x0 = G->y0 = G->z0 = 1e9f;
    G->x1 = G->y1 = G->z1 = -1e9f;
    G->err_line = -1;
    G->err[0] = 0;

    /* прегледът е в работни координати: нулите и дължините са 0, диаметрите от таблицата (за G41/G42) */
    gc_tables_t *tab = calloc(1, sizeof *tab);
    if (!tab) abort();
    gc_tables_init(tab);
    for (int t = 0; t <= TOOL_MAX && t < GC_TOOL_N; t++) tab->tool[t].dia = TT[t].dia;
    for (int w = 1; w <= 6; w++)
        for (int a = 0; a < 3; a++) tab->wcs[w][a] = M.wcs_off[w][a];
    gc_config_t cfg = {DIAL_HAAS() ? GC_DIALECT_HAAS : GC_DIALECT_MACH3, 0, 0, false, on_cmd, NULL};
    cfg.axes = axes_mask(&cfg.rotary);
    gc_interp_t *in = malloc(sizeof *in);
    if (!in) abort();
    gc_interp_init(in, &cfg, tab, st.pos);
    in->wcs = st.wcs;   /* без G54 … G59 в програмата: активната нула */
    char *text = program_join(G, NULL);
    gc_err_t e;
    if (gc_run_text(in, text, &e)) {
        G->err_line = e.line;
        snprintf(G->err, sizeof G->err, "%s", e.msg);
    }
    free(text);
    free(in);
    free(tab);
    add_op(OP_END, G->nlines ? G->nlines - 1 : 0)->t0 = st.t;
    G->t_total = st.t;
    if (G->x0 > G->x1) { G->x0 = -10; G->x1 = 10; G->y0 = -10; G->y1 = 10; G->z0 = G->z1 = 0; }
}

int program_parse_into(program_t *dst, const char *name, const char *text)
{
    program_t *sv = G;
    G = dst;
    program_free_g();
    snprintf(G->name, sizeof G->name, "%s", name);
    size_t n = strlen(text);
    G->text = malloc(n + 1);
    if (!G->text) abort();
    memcpy(G->text, text, n + 1);
    char *s = G->text;
    while (*s) {
        char *e = strchr(s, '\n');
        size_t len = e ? (size_t)(e - s) : strlen(s);
        char *next = e ? e + 1 : s + len;
        while (len && (s[len - 1] == '\r' || s[len - 1] == ' ')) len--;
        s[len] = 0;
        GROW(G->line, G->nlines, G->cap_line);
        G->line[G->nlines++] = s;
        if (!e) break;
        s = next;
    }
    while (G->nlines && !G->line[G->nlines - 1][0]) G->nlines--;
    parse();
    G = sv;
    return 0;
}

int program_load_text(const char *name, const char *text) { return program_parse_into(&P, name, text); }

void program_reparse(void)
{
    if (!P.nlines) return;
    char *t = program_join(&P, NULL), name[sizeof P.name];
    snprintf(name, sizeof name, "%s", P.name);
    program_load_text(name, t);
    free(t);
}

char *program_join(const program_t *p, size_t *len)
{
    size_t n = 0;
    for (int i = 0; i < p->nlines; i++) n += strlen(p->line[i]) + 1;
    char *t = malloc(n + 1), *o = t;
    if (!t) abort();
    for (int i = 0; i < p->nlines; i++) {
        size_t l = strlen(p->line[i]);
        memcpy(o, p->line[i], l);
        o += l;
        *o++ = '\n';
    }
    *o = 0;
    if (len) *len = n;
    return t;
}

/* Промяна на ред в заредената програма: редовете се събират в нов текст и програмата се разбира наново. */
int program_edit(int line, int mode, const char *txt)
{
    if (line < 0 || line > P.nlines || (mode != PE_INSERT && line == P.nlines)) return -1;
    size_t add = txt ? strlen(txt) : 0, n = 0;
    for (int i = 0; i < P.nlines; i++) n += strlen(P.line[i]) + 1;
    char *t = malloc(n + add + 2), *o = t;
    if (!t) abort();
    for (int i = 0; i <= P.nlines; i++) {
        if (i == line && mode != PE_DELETE) { memcpy(o, txt, add); o += add; *o++ = '\n'; }
        if (i == P.nlines) break;
        if (i == line && mode != PE_INSERT) continue;
        size_t l = strlen(P.line[i]);
        memcpy(o, P.line[i], l);
        o += l;
        *o++ = '\n';
    }
    *o = 0;
    char name[sizeof P.name];
    snprintf(name, sizeof name, "%s", P.name);
    program_load_text(name, t);
    free(t);
    return 0;
}

char *program_read_file(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 64L * 1024 * 1024) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = 0;
    if (size) *size = (long)rd;
    return buf;
}

int program_load_file(const char *path)
{
    char *buf = program_read_file(path, NULL);
    if (!buf) return -1;
    const char *base = strrchr(path, '/');
    program_load_text(base ? base + 1 : path, buf);
    free(buf);
    return 0;
}

/* Същата демо програма като в ui/maket.html */
static const char *demo_dzhob(void)
{
    static char buf[8192];
    size_t n = 0;
#define L(...) n += (size_t)snprintf(buf + n, sizeof buf - n, __VA_ARGS__)
    L("(ДЕМО: ДЖОБ, ОТВОР И КОНТУР)\n(ДЕТАЙЛ 80 X 50 MM, AL 6082)\nG21 G90 G17 G40 G49\nG54\nT5 M6\nS12000 M3\nM8\n");
    L("G0 Z5.000\nG0 X5.000 Y5.000\nG1 Z-2.000 F300\n(ДЖОБ, СТЪПКА 5 MM)\n");
    int dir = 1;
    for (int y = 5; y <= 45;) {
        L("G1 X%s F1200\n", dir > 0 ? "75.000" : "5.000");
        y += 5;
        if (y <= 45) L("G1 Y%d.000\n", y);
        dir = -dir;
    }
    L("G0 Z5.000\n(ОТВОР Ø40)\nG0 X20.000 Y25.000\nG1 Z-4.000 F300\nG2 X20.000 Y25.000 I20.000 J0 F900\nG0 Z5.000\n");
    L("(КОНТУР R15)\nG0 X10.000 Y-5.000\nG1 Z-5.000 F300\nG1 X70.000 F1000\nG3 X85.000 Y10.000 I0 J15.000\nG1 Y40.000\n");
    L("G3 X70.000 Y55.000 I-15.000 J0\nG1 X10.000\nG3 X-5.000 Y40.000 I0 J-15.000\nG1 Y10.000\nG3 X10.000 Y-5.000 I15.000 J0\n");
    L("G0 Z25.000\nM9\nM5\nG0 X0 Y0\nM30\n");
#undef L
    return buf;
}

void program_load_demo(void) { program_load_text("demo_dzhob.nc", demo_dzhob()); }

/* Примерите от макета (genFlange, genFacing, genSpiral) */
static const char *demo_flanets(void)
{
    static char b[4096];
    size_t n = 0;
#define L(...) n += (size_t)snprintf(b + n, sizeof b - n, __VA_ARGS__)
    L("(ФЛАНЕЦ Ø80, 6 ОТВОРА Ø8.5 НА ДЕЛИТЕЛНА Ø60)\n(ПРОБИВАНЕ С G81, КОНТУР С ДЪГА)\nG21 G90 G17 G40 G49\nG54\nT4 M6\nS2200 M3\nM8\nG0 X30.000 Y0 Z25.000\n");
    for (int i = 0; i < 6; i++) {
        double a = i * M_PI / 3, x = 30 * cos(a), y = 30 * sin(a);
        if (fabs(y) < 5e-4) y = 0;
        if (i) L("X%.3f Y%.3f\n", x, y); else L("G98 G81 X%.3f Y%.3f Z-12.000 R2.000 F150\n", x, y);
    }
    L("G80\nG0 Z25.000\nT2 M6\nS9000 M3\nG0 X-45.000 Y0\nG0 Z2.000\nG1 Z-6.000 F300\nG2 X-45.000 Y0 I45.000 J0 F800\nG0 Z25.000\nM9\nM5\nG0 X0 Y0\nM30\n");
    return b;
}

static const char *demo_planirane(void)
{
    static char b[2048];
    size_t n = 0;
    L("(ПЛАНИРАНЕ 150 X 100, ЧЕЛНА ФРЕЗА Ø50)\nG21 G90 G17\nG54\nT8 M6\nS4500 M3\nM8\nG0 Z5.000\nG0 X-30.000 Y-10.000\nG1 Z-0.500 F400\n");
    int d = 1;
    for (int y = -10; y <= 111; y += 35) {
        L("G1 X%s F1500\n", d > 0 ? "180.000" : "-30.000");
        if (y + 35 <= 111) L("G1 Y%.3f\n", (double)(y + 35));
        d = -d;
    }
    L("G0 Z25.000\nM9\nM5\nM30\n");
    return b;
}

static const char *demo_spirala(void)
{
    static char b[2048];
    size_t n = 0;
    L("(ГРАВИРАНЕ: СПИРАЛА С R ДЪГИ, РАМКА В G91)\nG21 G90 G17\nG54\nT9 M6\nS18000 M3\nG0 Z3.000\nG0 X42.000 Y40.000\nG1 Z-0.200 F120\n");
    double x = 42;
    for (int k = 0; k < 14; k++) {
        double dx = 4 + 1.5 * k, nx = k % 2 ? x + dx : x - dx;
        L("G3 X%.3f Y40.000 R%.3f%s\n", nx, dx / 2, k ? "" : " F600");
        x = nx;
    }
    L("G0 Z3.000\nG0 X5.000 Y5.000\nG1 Z-0.200 F120\nG91\nG1 X70.000 F600\nG1 Y70.000\nG1 X-70.000\nG1 Y-70.000\nG90\nG0 Z25.000\nM5\nM30\n");
#undef L
    return b;
}

const char *program_demo_text(int i, const char **name)
{
    static const char *const N[4] = {"demo_dzhob.nc", "flanets_d80.nc", "planirane_150x100.nc", "gravirane_spirala.nc"};
    if (i < 0 || i >= 4) return NULL;
    if (name) *name = N[i];
    return i == 0 ? demo_dzhob() : i == 1 ? demo_flanets() : i == 2 ? demo_planirane() : demo_spirala();
}
