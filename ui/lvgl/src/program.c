#include "program.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

program_t P;

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

static int cap_pt, cap_op, cap_line;

static void program_free(void)
{
    for (int i = 0; i < P.nlines; i++) free(P.line[i]);
    free(P.line);
    free(P.pt);
    free(P.cum);
    free(P.op);
    memset(&P, 0, sizeof P);
    cap_pt = cap_op = cap_line = 0;
}

static int add_pt(p3_t p, float cum)
{
    if (P.npt >= cap_pt) {
        cap_pt = cap_pt ? cap_pt * 2 : 256;
        p3_t *a = realloc(P.pt, (size_t)cap_pt * sizeof *a);
        float *c = realloc(P.cum, (size_t)cap_pt * sizeof *c);
        if (!a || !c) abort();
        P.pt = a;
        P.cum = c;
    }
    P.pt[P.npt] = p;
    P.cum[P.npt] = cum;
    return P.npt++;
}

static op_t *add_op(op_kind_t k, int line)
{
    GROW(P.op, P.nop, cap_op);
    op_t *o = &P.op[P.nop++];
    memset(o, 0, sizeof *o);
    o->kind = k;
    o->line = line;
    return o;
}

static void note_tool(int t)
{
    for (int i = 0; i < P.ntools; i++)
        if (P.tools[i] == t) return;
    if (P.ntools < PROG_MAX_TOOLS) P.tools[P.ntools++] = t;
}

static void bound(p3_t p)
{
    if (p.x < P.x0) P.x0 = p.x;
    if (p.x > P.x1) P.x1 = p.x;
    if (p.y < P.y0) P.y0 = p.y;
    if (p.y > P.y1) P.y1 = p.y;
    if (p.z < P.z0) P.z0 = p.z;
    if (p.z > P.z1) P.z1 = p.z;
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

static void parse(void)
{
    p3_t pos = {0, 0, 25};
    int mode = 0, tool = 0;
    float feed = 1000;
    bool inc = false;
    P.x0 = P.y0 = P.z0 = 1e9f;
    P.x1 = P.y1 = P.z1 = -1e9f;
    float t = 0;

    for (int li = 0; li < P.nlines; li++) {
        word_t w[32];
        int n = words(P.line[li], w, 32);
        if (!n) continue;
        bool has_xyz = false, end = false;
        p3_t tgt = pos;
        double I = 0, J = 0, R = 0;
        bool has_r = false;
        int T = -1, S = -1;
        for (int k = 0; k < n; k++) {
            int iv = (int)lround(w[k].v);
            switch (w[k].l) {
            case 'G':
                if (iv >= 0 && iv <= 3) mode = iv;
                else if (iv == 90) inc = false;
                else if (iv == 91) inc = true;
                break;
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
        if (has_xyz) {
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
            o->np = P.npt - o->p0;
            o->len = len;
            o->t0 = t;
            t += len / ((o->rapid ? RAPID_FEED : (feed > 1 ? feed : 1)) / 60.0f);
            o->t1 = t;
            pos = tgt;
        }
        if (end) break;
    }
    add_op(OP_END, P.nlines ? P.nlines - 1 : 0)->t0 = t;
    P.t_total = t;
    if (P.x0 > P.x1) { P.x0 = -10; P.x1 = 10; P.y0 = -10; P.y1 = 10; P.z0 = P.z1 = 0; }
}

int program_load_text(const char *name, const char *text)
{
    program_free();
    snprintf(P.name, sizeof P.name, "%s", name);
    const char *s = text;
    while (*s) {
        const char *e = strchr(s, '\n');
        size_t len = e ? (size_t)(e - s) : strlen(s);
        while (len && (s[len - 1] == '\r' || s[len - 1] == ' ')) len--;
        GROW(P.line, P.nlines, cap_line);
        char *l = malloc(len + 1);
        if (!l) abort();
        memcpy(l, s, len);
        l[len] = 0;
        P.line[P.nlines++] = l;
        if (!e) break;
        s = e + 1;
    }
    while (P.nlines && !P.line[P.nlines - 1][0]) free(P.line[--P.nlines]);
    parse();
    return 0;
}

int program_load_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 64L * 1024 * 1024) { fclose(f); return -1; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = 0;
    const char *base = strrchr(path, '/');
    program_load_text(base ? base + 1 : path, buf);
    free(buf);
    return 0;
}

/* Същата демо програма като в ui/maket.html */
void program_load_demo(void)
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
    program_load_text("demo_dzhob.nc", buf);
}
