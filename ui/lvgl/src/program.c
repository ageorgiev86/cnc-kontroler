#include "program.h"

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

/* Чете думите на един ред; връща брой. Коментари (...) и ; се пропускат. */
typedef struct { char l; double v; } word_t;

static int words(const char *s, word_t *w, int max)
{
    int n = 0;
    while (*s && n < max) {
        if (*s == '(') { while (*s && *s != ')') s++; if (*s) s++; continue; }
        if (*s == ';') break;
        if (isalpha((unsigned char)*s)) {
            char l = (char)toupper((unsigned char)*s++);
            while (*s == ' ') s++;
            char *e;
            double v = strtod(s, &e);
            if (e != s) { w[n].l = l; w[n].v = v; n++; s = e; continue; }
            continue;
        }
        s++;
    }
    return n;
}

/* права линия като отделен ход (циклите G81–G83) */
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
    if (!rapid) { bound(a); bound(b); } else bound(b);
    o->np = 2;
    o->len = len;
    o->t0 = *t;
    *t += len / ((rapid ? RAPID_FEED : (feed > 1 ? feed : 1)) / 60.0f);
    o->t1 = *t;
}

static void parse(void)
{
    p3_t pos = {0, 0, 25};
    int mode = 0, tool = 0;
    float feed = 1000;
    bool inc = false;
    double cz = 0, cr = 2;      /* цикли G81–G83: дълбочина и равнина R */
    bool g98 = true;
    G->x0 = G->y0 = G->z0 = 1e9f;
    G->x1 = G->y1 = G->z1 = -1e9f;
    float t = 0;

    for (int li = 0; li < G->nlines; li++) {
        word_t w[32];
        int n = words(G->line[li], w, 32);
        if (!n) continue;
        bool has_xyz = false, end = false;
        p3_t tgt = pos;
        double I = 0, J = 0, R = 0;
        bool has_r = false;
        int T = -1, S = -1;
        /* първо G думите: режимът важи за целия ред */
        for (int k = 0; k < n; k++) {
            if (w[k].l != 'G') continue;
            int iv = (int)lround(w[k].v);
            if (iv >= 0 && iv <= 3) mode = iv;
            else if (iv == 90) inc = false;
            else if (iv == 91) inc = true;
            else if (iv >= 81 && iv <= 83) mode = 81;
            else if (iv == 80) mode = 0;
            else if (iv == 98) g98 = true;
            else if (iv == 99) g98 = false;
        }
        for (int k = 0; k < n; k++) {
            int iv = (int)lround(w[k].v);
            if (mode == 81 && (w[k].l == 'Z' || w[k].l == 'R')) {
                if (w[k].l == 'Z') cz = w[k].v; else cr = w[k].v;
                continue;
            }
            switch (w[k].l) {
            case 'G': break;
            case 'X': tgt.x = (float)(inc ? pos.x + w[k].v : w[k].v); has_xyz = true; break;
            case 'Y': tgt.y = (float)(inc ? pos.y + w[k].v : w[k].v); has_xyz = true; break;
            case 'Z': tgt.z = (float)(inc ? pos.z + w[k].v : w[k].v); has_xyz = true; break;
            case 'I': I = w[k].v; break;
            case 'J': J = w[k].v; break;
            case 'R': R = w[k].v; has_r = true; break;
            case 'F': feed = (float)w[k].v; break;
            case 'T': T = iv; break;
            case 'S': S = iv; break;
            case 'M':
                if (iv == 6 && T >= 0) {
                    op_t *o = add_op(OP_TOOL, li);
                    o->tool = tool = T;
                    note_tool(T);
                    o->t0 = t; t += TOOL_TIME; o->t1 = t;
                } else if (iv == 3 || iv == 4) {
                    op_t *o = add_op(OP_SPINDLE, li);
                    o->s = S > 0 ? S : 10000;
                    o->t0 = t; t += SPINDLE_TIME; o->t1 = t;
                } else if (iv == 5) {
                    op_t *o = add_op(OP_SPINDLE, li);
                    o->t0 = o->t1 = t;
                } else if (iv == 8 || iv == 9) {
                    op_t *o = add_op(OP_COOL, li);
                    o->on = iv == 8;
                    o->t0 = o->t1 = t;
                } else if (iv == 30 || iv == 2) {
                    end = true;
                }
                break;
            default: break;
            }
        }
        if (has_xyz && mode == 81) {   /* пробиване: бързо до XY и R, подаване до Z, бързо обратно */
            p3_t a = pos, b = {tgt.x, tgt.y, pos.z}, c = {tgt.x, tgt.y, (float)cr}, d = {tgt.x, tgt.y, (float)cz};
            line_op(li, a, b, true, feed, tool, &t);
            line_op(li, b, c, true, feed, tool, &t);
            line_op(li, c, d, false, feed, tool, &t);
            p3_t e = g98 ? b : c;
            line_op(li, d, e, true, feed, tool, &t);
            pos = e;
        } else if (has_xyz) {
            op_t *o = add_op(OP_MOVE, li);
            o->rapid = mode == 0;
            o->feed = feed;
            o->tool = tool;
            o->p0 = add_pt(pos, 0);
            float len = 0;
            if (mode == 2 || mode == 3) {
                bool cw = mode == 2;
                double cx, cy;
                if (has_r) {   /* дъга с радиус */
                    double dx = tgt.x - pos.x, dy = tgt.y - pos.y, d = hypot(dx, dy);
                    double h = sqrt(fmax(0, R * R - d * d / 4)), sg = (cw ? -1 : 1) * (R > 0 ? 1 : -1);
                    cx = (pos.x + tgt.x) / 2 - sg * h * dy / (d > 0 ? d : 1);
                    cy = (pos.y + tgt.y) / 2 + sg * h * dx / (d > 0 ? d : 1);
                } else {
                    cx = pos.x + I;
                    cy = pos.y + J;
                }
                double r = hypot(pos.x - cx, pos.y - cy);
                double a0 = atan2(pos.y - cy, pos.x - cx), a1 = atan2(tgt.y - cy, tgt.x - cx), sw = a1 - a0;
                if (cw) { if (sw >= -1e-9) sw -= 2 * M_PI; }
                else { if (sw <= 1e-9) sw += 2 * M_PI; }
                int steps = (int)ceil(fabs(sw) * 180 / M_PI / ARC_STEP_DEG);
                if (steps < 2) steps = 2;
                p3_t prev = pos;
                for (int k = 1; k <= steps; k++) {
                    double f = (double)k / steps, g = a0 + sw * f;
                    p3_t q = {(float)(cx + r * cos(g)), (float)(cy + r * sin(g)), (float)(pos.z + (tgt.z - pos.z) * f)};
                    if (k == steps) { q.x = tgt.x; q.y = tgt.y; }
                    len += hypotf(hypotf(q.x - prev.x, q.y - prev.y), q.z - prev.z);
                    add_pt(q, len);
                    bound(q);
                    prev = q;
                }
            } else {
                len = hypotf(hypotf(tgt.x - pos.x, tgt.y - pos.y), tgt.z - pos.z);
                add_pt(tgt, len);
                if (!o->rapid) { bound(pos); bound(tgt); }
                else bound(tgt);
            }
            o->np = G->npt - o->p0;
            o->len = len;
            o->t0 = t;
            t += len / ((o->rapid ? RAPID_FEED : (feed > 1 ? feed : 1)) / 60.0f);
            o->t1 = t;
            pos = tgt;
        }
        if (end) break;
    }
    add_op(OP_END, G->nlines ? G->nlines - 1 : 0)->t0 = t;
    G->t_total = t;
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
