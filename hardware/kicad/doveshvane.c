/* doveshvane.c: довършващ рутер за връзките, които Freerouting не е трасирал.
 *
 * Мрежа от клетки (стъпка RES мм) на двата слоя. Центърът на клетка i е на i*RES мм, така че изводите на
 * QFP (стъпка 0,5 / 0,25 мм) падат точно в центрове. За всяка връзка: разстояние (EDT) до чуждата мед в
 * прозорец около двата края, клетката е свободна, ако е по-далеч от разстояние + половин ширина; A* с
 * 8 посоки и преход между слоевете. Положеното трасе става мед за следващите връзки.
 *
 * Вход (stdin, мм):
 *   G W H RES                      размер на мрежата в клетки
 *   B l t r b clr                  контур на платката и разстояние до ръба
 *   F l t r b clr                  правоъгълник с по-малко разстояние (изводите на STM32)
 *   P layer net n x y ...          неподвижна мед (layer 0 = F.Cu, 1 = B.Cu, 2 = двата)
 *   Q layer net n x y ...          трасе/преход, което може да се разкъса
 *   H x y r                        отвор (за разстоянието отвор до отвор при преходите)
 *   J id net clr viad viac rip nw w...  връзка: разстояние, преход (диаметър, разстояние), разкъсване
 *                                  до rip мм от краищата (0 = без), ширини за опит
 *   S layer n x y ... / T ...      начало / край (медта на двата края)
 *   E                              трасирай връзката
 * Изход: OK id / L layer w x1 y1 x2 y2 / V x y d / X layer x y (минава през чуждо трасе) / FAIL id
 *
 * Разкъсване: ако връзката не става, втори опит, при който чуждите трасета (не площадките) се минават срещу
 * висока цена; клетките им се изпращат с X и Python маха тези трасета (мрежите им се трасират наново).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int GW, GH;
static double RES;
static short *lab[2];            /* неподвижна мед в клетката (площадки, заключени трасета, маса): номер на мрежа */
static short *labt[2];           /* трасета, които могат да се разкъсат */
static double bl, bt, br, bb, bclr;
static double fl, ft, fr, fb, fclr;
static double *holes; static int nholes, capholes;   /* x, y, радиус на всеки отвор (мм) */
static double ripr = 3.0;        /* разкъсване само до толкова мм от краищата на връзката (от реда J) */
#define HOLE_GAP 0.5             /* отвор до отвор (pravila.py: min_hole_to_hole) */

#define NONET 32000              /* мед без мрежа (отвори без мед): пречка за всички */

static void die(const char *m) { fprintf(stderr, "%s\n", m); exit(1); }

/* запълва многоъгълника в масив m (ширина w, отместване ox, oy в клетки) с v; четно-нечетно */
static void fill_poly(short *m, int w, int h, int ox, int oy, const double *xy, int n, short v, int keep)
{
    double ymin = 1e9, ymax = -1e9;
    for (int i = 0; i < n; i++) {
        if (xy[2 * i + 1] < ymin) ymin = xy[2 * i + 1];
        if (xy[2 * i + 1] > ymax) ymax = xy[2 * i + 1];
    }
    int y0 = (int)ceil(ymin / RES) - oy, y1 = (int)floor(ymax / RES) - oy;
    if (y0 < 0) y0 = 0;
    if (y1 > h - 1) y1 = h - 1;
    double *xs = malloc(sizeof(double) * (n + 2));
    for (int y = y0; y <= y1; y++) {
        double yc = (y + oy) * RES;
        int k = 0;
        for (int i = 0; i < n; i++) {
            double xa = xy[2 * i], ya = xy[2 * i + 1];
            double xb = xy[2 * ((i + 1) % n)], yb = xy[2 * ((i + 1) % n) + 1];
            if ((ya <= yc && yb > yc) || (yb <= yc && ya > yc))
                xs[k++] = xa + (yc - ya) * (xb - xa) / (yb - ya);
        }
        for (int i = 1; i < k; i++)               /* сортиране с вмъкване: малко пресичания */
            for (int j = i; j > 0 && xs[j - 1] > xs[j]; j--) {
                double t = xs[j]; xs[j] = xs[j - 1]; xs[j - 1] = t;
            }
        for (int i = 0; i + 1 < k; i += 2) {
            int x0 = (int)ceil(xs[i] / RES) - ox, x1 = (int)floor(xs[i + 1] / RES) - ox;
            if (x0 < 0) x0 = 0;
            if (x1 > w - 1) x1 = w - 1;
            for (int x = x0; x <= x1; x++)
                if (!keep || m[(size_t)y * w + x] == 0)
                    m[(size_t)y * w + x] = v;
        }
    }
    free(xs);
}

static double *read_pts(int n)
{
    double *xy = malloc(sizeof(double) * 2 * n);
    for (int i = 0; i < 2 * n; i++)
        if (scanf("%lf", &xy[i]) != 1) die("лош вход");
    return xy;
}

/* ---- точно евклидово разстояние (Felzenszwalb–Huttenlocher), квадрат в клетки ---- */
static void edt1(const float *f, float *d, int n, int *v, float *z)
{
    int k = 0;
    v[0] = 0; z[0] = -1e20f; z[1] = 1e20f;
    for (int q = 1; q < n; q++) {
        float s;
        for (;;) {
            s = ((f[q] + (float)q * q) - (f[v[k]] + (float)v[k] * v[k])) / (2.0f * q - 2.0f * v[k]);
            if (s <= z[k] && k > 0) k--; else break;
        }
        if (s <= z[k]) { v[0] = q; z[0] = -1e20f; z[1] = 1e20f; k = 0; continue; }
        k++; v[k] = q; z[k] = s; z[k + 1] = 1e20f;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k + 1] < q) k++;
        d[q] = (float)(q - v[k]) * (q - v[k]) + f[v[k]];
    }
}

static void edt2(float *g, int w, int h)
{
    int n = w > h ? w : h;
    float *f = calloc(n, sizeof(float)), *d = calloc(n, sizeof(float)), *z = malloc(sizeof(float) * (n + 1));
    int *v = malloc(sizeof(int) * n);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) f[y] = g[(size_t)y * w + x];
        edt1(f, d, h, v, z);
        for (int y = 0; y < h; y++) g[(size_t)y * w + x] = d[y];
    }
    for (int y = 0; y < h; y++) {
        memcpy(f, g + (size_t)y * w, sizeof(float) * w);
        edt1(f, d, w, v, z);
        memcpy(g + (size_t)y * w, d, sizeof(float) * w);
    }
    free(f); free(d); free(z); free(v);
}

/* ---- купчина за A* ---- */
typedef struct { float f; int i; } HN;
static HN *heap; static int hn, hcap;
static void hpush(float f, int i)
{
    if (hn == hcap) { hcap = hcap ? hcap * 2 : 1 << 16; heap = realloc(heap, sizeof(HN) * hcap); }
    int k = hn++;
    while (k > 0 && heap[(k - 1) / 2].f > f) { heap[k] = heap[(k - 1) / 2]; k = (k - 1) / 2; }
    heap[k].f = f; heap[k].i = i;
}
static HN hpop(void)
{
    HN top = heap[0], last = heap[--hn];
    int k = 0;
    for (;;) {
        int c = 2 * k + 1;
        if (c >= hn) break;
        if (c + 1 < hn && heap[c + 1].f < heap[c].f) c++;
        if (heap[c].f >= last.f) break;
        heap[k] = heap[c]; k = c;
    }
    heap[k] = last;
    return top;
}

/* маркира медта на ново трасе в мрежата */
static void paint_seg(int layer, double x1, double y1, double x2, double y2, double w, short net)
{
    double r = w / 2;
    int xa = (int)floor((fmin(x1, x2) - r) / RES), xb = (int)ceil((fmax(x1, x2) + r) / RES);
    int ya = (int)floor((fmin(y1, y2) - r) / RES), yb = (int)ceil((fmax(y1, y2) + r) / RES);
    double dx = x2 - x1, dy = y2 - y1, L2 = dx * dx + dy * dy;
    for (int y = ya; y <= yb; y++)
        for (int x = xa; x <= xb; x++) {
            if (x < 0 || y < 0 || x >= GW || y >= GH) continue;
            double px = x * RES, py = y * RES, t = L2 > 0 ? ((px - x1) * dx + (py - y1) * dy) / L2 : 0;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            double ex = x1 + t * dx - px, ey = y1 + t * dy - py;
            if (ex * ex + ey * ey <= r * r)
                for (int l = 0; l < 2; l++)
                    if (layer == l || layer == 2) labt[l][(size_t)y * GW + x] = net;
        }
}

typedef struct { int layer, n; double *xy; } Poly;

static int route(int id, int net, double clr, double viad, double viac, double w,
                 Poly *src, int ns, Poly *dst, int nd, double margin, int rip)
{
    /* прозорец около двата края */
    double l = 1e9, t = 1e9, r = -1e9, b = -1e9;
    for (int k = 0; k < ns + nd; k++) {
        Poly *p = k < ns ? &src[k] : &dst[k - ns];
        for (int i = 0; i < p->n; i++) {
            l = fmin(l, p->xy[2 * i]); r = fmax(r, p->xy[2 * i]);
            t = fmin(t, p->xy[2 * i + 1]); b = fmax(b, p->xy[2 * i + 1]);
        }
    }
    int ox = (int)floor((l - margin) / RES), oy = (int)floor((t - margin) / RES);
    int ex = (int)ceil((r + margin) / RES), ey = (int)ceil((b + margin) / RES);
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    if (ex > GW - 1) ex = GW - 1;
    if (ey > GH - 1) ey = GH - 1;
    int w_ = ex - ox + 1, h_ = ey - oy + 1;
    size_t N = (size_t)w_ * h_;
    const double eps = 0.04;
    double viadrill = viad >= 1.4 ? 0.7 : 0.6;

    /* свободни клетки за трасе и за преход: H спрямо неподвижната мед, S спрямо трасетата */
    unsigned char *freet = calloc(2 * N, 1), *freev = calloc(N, 1);
    unsigned char *softt = calloc(2 * N, 1), *softv = calloc(N, 1);
    float *g = malloc(sizeof(float) * N);
    for (int kind = 0; kind < 2; kind++)
        for (int L = 0; L < 2; L++) {
            short *m = kind == 0 ? lab[L] : labt[L];
            for (int y = 0; y < h_; y++)
                for (int x = 0; x < w_; x++) {
                    short v = m[(size_t)(y + oy) * GW + x + ox];
                    g[(size_t)y * w_ + x] = (v != 0 && v != net) ? 0.0f : 1e20f;
                }
            edt2(g, w_, h_);
            unsigned char *ftr = kind == 0 ? freet : softt, *fv_ = kind == 0 ? freev : softv;
            for (int y = 0; y < h_; y++)
                for (int x = 0; x < w_; x++) {
                    double px = (x + ox) * RES, py = (y + oy) * RES;
                    double c = (px >= fl && px <= fr && py >= ft && py <= fb) ? fclr : clr;
                    double d = sqrt(g[(size_t)y * w_ + x]) * RES;
                    double de = fmin(fmin(px - bl, br - px), fmin(py - bt, bb - py));
                    size_t i = (size_t)y * w_ + x;
                    ftr[L * N + i] = d >= c + w / 2 + eps && de >= bclr + w / 2;
                    int fv = d >= viac + viad / 2 + eps && de >= bclr + viad / 2;
                    fv_[i] = L == 0 ? fv : (fv_[i] && fv);
                }
        }
    free(g);
    /* преходите: и отвор до отвор */
    for (int k = 0; k < nholes; k++) {
        double hx = holes[3 * k], hy = holes[3 * k + 1], need = holes[3 * k + 2] + HOLE_GAP + viadrill / 2 + 0.02;
        int x0 = (int)floor((hx - need) / RES) - ox, x1 = (int)ceil((hx + need) / RES) - ox;
        int y0 = (int)floor((hy - need) / RES) - oy, y1 = (int)ceil((hy + need) / RES) - oy;
        for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < h_; y++)
            for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < w_; x++) {
                double dx = (x + ox) * RES - hx, dy = (y + oy) * RES - hy;
                if (dx * dx + dy * dy < need * need) freev[(size_t)y * w_ + x] = 0;
            }
    }

    /* начало и край */
    short *ms = calloc(2 * N, sizeof(short));
    for (int k = 0; k < ns + nd; k++) {
        Poly *p = k < ns ? &src[k] : &dst[k - ns];
        for (int L = 0; L < 2; L++)
            if (p->layer == L || p->layer == 2)
                fill_poly(ms + L * N, w_, h_, ox, oy, p->xy, p->n, k < ns ? 1 : 2, 1);
    }
    /* трасетата са пречка навсякъде, освен при разкъсване до ripr мм от двата края (там изводът е затворен) */
    {
        unsigned char *zone = calloc(N, 1);
        if (rip) {
            float *gz = malloc(sizeof(float) * N);
            for (size_t i = 0; i < N; i++) gz[i] = (ms[i] || ms[N + i]) ? 0.0f : 1e20f;
            edt2(gz, w_, h_);
            for (size_t i = 0; i < N; i++) zone[i] = sqrt(gz[i]) * RES <= ripr;
            free(gz);
        }
        for (size_t i = 0; i < N; i++) {
            if (!zone[i]) { freet[i] &= softt[i]; freet[N + i] &= softt[N + i]; freev[i] &= softv[i]; }
        }
        free(zone);
    }

    /* началото и краят са цялото островче мед, до което се допират (на двата слоя, през преходите) */
    {
        int *q = malloc(sizeof(int) * 2 * N);
        int qh = 0, qt = 0;
        for (size_t i = 0; i < 2 * N; i++)
            if (ms[i]) q[qt++] = (int)i;
        while (qh < qt) {
            int i = q[qh++], L = i / (int)N, rem = i % (int)N, x = rem % w_, y = rem / w_;
            int nb[5] = {-1, -1, -1, -1, -1};
            if (x > 0) nb[0] = i - 1;
            if (x < w_ - 1) nb[1] = i + 1;
            if (y > 0) nb[2] = i - w_;
            if (y < h_ - 1) nb[3] = i + w_;
            nb[4] = (1 - L) * (int)N + rem;
            for (int k = 0; k < 5; k++) {
                int j = nb[k];
                if (j < 0 || ms[j]) continue;
                int Lj = j / (int)N, rj = j % (int)N;
                size_t cj = (size_t)(rj / w_ + oy) * GW + rj % w_ + ox, ci = (size_t)(y + oy) * GW + x + ox;
                if (lab[Lj][cj] != net && labt[Lj][cj] != net) continue;
                if (k == 4 && lab[L][ci] != net && labt[L][ci] != net) continue;
                ms[j] = ms[i];
                q[qt++] = j;
            }
        }
        free(q);
    }
    double tl = 1e9, tt = 1e9, tr = -1e9, tb = -1e9;     /* за евристиката: правоъгълникът на края */
    for (int k = 0; k < nd; k++)
        for (int i = 0; i < dst[k].n; i++) {
            tl = fmin(tl, dst[k].xy[2 * i] / RES - ox); tr = fmax(tr, dst[k].xy[2 * i] / RES - ox);
            tt = fmin(tt, dst[k].xy[2 * i + 1] / RES - oy); tb = fmax(tb, dst[k].xy[2 * i + 1] / RES - oy);
        }

    float *gs = malloc(sizeof(float) * 2 * N);
    int *par = malloc(sizeof(int) * 2 * N);
    for (size_t i = 0; i < 2 * N; i++) { gs[i] = 1e30f; par[i] = -1; }
    const float VIA = (float)(3.0 / RES);   /* преходът струва колкото 3 мм трасе */
    const float RIP = (float)(2.0 / RES);   /* клетка през чуждо трасе: колкото 2 мм */
    hn = 0;
    int nsrc = 0;
    for (size_t i = 0; i < 2 * N; i++)
        if (ms[i] == 1 && freet[i]) { gs[i] = 0; par[i] = (int)i; hpush(0, (int)i); nsrc++; }
    int ntgt = 0;
    for (size_t i = 0; i < 2 * N; i++) ntgt += ms[i] == 2;
    int found = -1, expanded = 0;
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (hn > 0) {
        HN c = hpop();
        int i = c.i;
        int L = i / (int)N, rem = i % (int)N, x = rem % w_, y = rem / w_;
        if (ms[i] == 2) { found = i; break; }
        expanded++;
        float gi = gs[i];
        for (int k = 0; k < 9; k++) {
            int j, nx = x, ny = y, nl = L;
            float cost;
            if (k < 8) {
                nx = x + DX[k]; ny = y + DY[k];
                if (nx < 0 || ny < 0 || nx >= w_ || ny >= h_) continue;
                cost = k < 4 ? 1.0f : 1.41421356f;
            } else {
                if (!freev[rem]) continue;
                nl = 1 - L; cost = VIA;
                if (rip && !softv[rem]) cost += 20 * RIP;
            }
            j = nl * (int)N + ny * w_ + nx;
            if (!freet[j] && ms[j] != 2) continue;
            if (!freet[j] && ms[j] == 2 && k == 8) continue;
            if (rip && k < 8 && !softt[j]) cost += RIP;
            float ng = gi + cost;
            if (ng < gs[j]) {
                gs[j] = ng; par[j] = i;
                double hx = nx < tl ? tl - nx : nx > tr ? nx - tr : 0;
                double hy = ny < tt ? tt - ny : ny > tb ? ny - tb : 0;
                double hmin = fmin(hx, hy), hmax = fmax(hx, hy);
                float h = (float)(hmax + 0.41421356 * hmin) * 1.001f;   /* осмоъгълно, малко над → по-малко зигзаг */
                hpush(ng + h, j);
            }
        }
    }
    int ok = 0;
    if (found >= 0) {
        /* пътят обратно → отсечки на всеки слой, преходи при смяна */
        int cnt = 0;
        for (int i = found; ; i = par[i]) { cnt++; if (par[i] == i) break; }
        int *pth = malloc(sizeof(int) * cnt);
        int k = cnt;
        for (int i = found; ; i = par[i]) { pth[--k] = i; if (par[i] == i) break; }
        printf("OK %d\n", id);
        if (rip)
            for (int a = 0; a < cnt; a++) {
                int La = pth[a] / (int)N, ra = pth[a] % (int)N;
                int via = a + 1 < cnt && pth[a + 1] / (int)N != La;
                if (!softt[pth[a]] || (via && !softv[ra]))
                    printf("X %d %.4f %.4f\n", via ? 2 : La, (ra % w_ + ox) * RES, (ra / w_ + oy) * RES);
            }
        /* по слоеве; в един слой се пазят само точките, където посоката се сменя */
        int s = 0;
        while (s < cnt) {
            int L = pth[s] / (int)N, e = s;
            while (e + 1 < cnt && pth[e + 1] / (int)N == L) e++;
            int a0 = s;
            for (int a = s + 1; a <= e; a++) {
                int last = a == e;
                if (!last) {
                    int r0 = pth[a - 1] % (int)N, r1 = pth[a] % (int)N, r2 = pth[a + 1] % (int)N;
                    if (r1 % w_ - r0 % w_ == r2 % w_ - r1 % w_ && r1 / w_ - r0 / w_ == r2 / w_ - r1 / w_)
                        continue;
                }
                int ra = pth[a0] % (int)N, rb = pth[a] % (int)N;
                double x1 = (ra % w_ + ox) * RES, y1 = (ra / w_ + oy) * RES;
                double x2 = (rb % w_ + ox) * RES, y2 = (rb / w_ + oy) * RES;
                printf("L %d %.4f %.4f %.4f %.4f %.4f\n", L, w, x1, y1, x2, y2);
                paint_seg(L, x1, y1, x2, y2, w, (short)net);
                a0 = a;
            }
            if (e + 1 < cnt) {                                  /* преход */
                int re = pth[e] % (int)N;
                double x = (re % w_ + ox) * RES, y = (re / w_ + oy) * RES;
                printf("V %.4f %.4f %.4f\n", x, y, viad);
                paint_seg(2, x, y, x, y, viad, (short)net);
                if (nholes == capholes) { capholes = capholes ? capholes * 2 : 1024; holes = realloc(holes, sizeof(double) * 3 * capholes); }
                holes[3 * nholes] = x; holes[3 * nholes + 1] = y; holes[3 * nholes + 2] = viadrill / 2; nholes++;
            }
            s = e + 1;
        }
        free(pth);
        ok = 1;
    }
    if (!ok)
        fprintf(stderr, "D %d w=%.2f прозорец %dx%d начало %d край %d обходени %d\n", id, w, w_, h_, nsrc, ntgt, expanded);
    free(freet); free(freev); free(softt); free(softv); free(ms); free(gs); free(par);
    return ok;
}

int main(void)
{
    char cmd[4];
    Poly src[64], dst[64];
    int ns = 0, nd = 0, id = 0, net = 0, nw = 0, rip = 0;
    double clr = 0, viad = 0, viac = 0, ws[8];
    while (scanf("%3s", cmd) == 1) {
        if (cmd[0] == 'G') {
            if (scanf("%d %d %lf", &GW, &GH, &RES) != 3) die("G");
            for (int l = 0; l < 2; l++) {
                lab[l] = calloc((size_t)GW * GH, sizeof(short));
                labt[l] = calloc((size_t)GW * GH, sizeof(short));
            }
        } else if (cmd[0] == 'B') {
            if (scanf("%lf %lf %lf %lf %lf", &bl, &bt, &br, &bb, &bclr) != 5) die("B");
        } else if (cmd[0] == 'F') {
            if (scanf("%lf %lf %lf %lf %lf", &fl, &ft, &fr, &fb, &fclr) != 5) die("F");
        } else if (cmd[0] == 'P' || cmd[0] == 'Q') {
            int layer, nn, n;
            if (scanf("%d %d %d", &layer, &nn, &n) != 3) die("P");
            double *xy = read_pts(n);
            for (int l = 0; l < 2; l++)
                if (layer == l || layer == 2)
                    fill_poly(cmd[0] == 'P' ? lab[l] : labt[l], GW, GH, 0, 0, xy, n, (short)nn, 0);
            free(xy);
        } else if (cmd[0] == 'H') {
            if (nholes == capholes) { capholes = capholes ? capholes * 2 : 1024; holes = realloc(holes, sizeof(double) * 3 * capholes); }
            if (scanf("%lf %lf %lf", &holes[3 * nholes], &holes[3 * nholes + 1], &holes[3 * nholes + 2]) != 3) die("H");
            nholes++;
        } else if (cmd[0] == 'J') {
            double ripv;
            if (scanf("%d %d %lf %lf %lf %lf %d", &id, &net, &clr, &viad, &viac, &ripv, &nw) != 7) die("J");
            rip = ripv > 0;
            ripr = ripv;
            for (int i = 0; i < nw; i++) if (scanf("%lf", &ws[i]) != 1) die("J w");
            ns = nd = 0;
        } else if (cmd[0] == 'S' || cmd[0] == 'T') {
            Poly p;
            if (scanf("%d %d", &p.layer, &p.n) != 2) die("S");
            p.xy = read_pts(p.n);
            if (cmd[0] == 'S' && ns < 64) src[ns++] = p;
            else if (cmd[0] == 'T' && nd < 64) dst[nd++] = p;
            else free(p.xy);
        } else if (cmd[0] == 'E') {
            int ok = 0;
            for (int m = 0; m < 2 && !ok; m++)
                for (int i = 0; i < nw && !ok; i++)
                    ok = route(id, net, clr, viad, viac, ws[i], src, ns, dst, nd, m == 0 ? 12.0 : 60.0, 0);
            for (int m = 0; m < 2 && !ok && rip; m++)       /* с разкъсване, най-тясното трасе */
                ok = route(id, net, clr, viad, viac, ws[nw - 1], src, ns, dst, nd, m == 0 ? 12.0 : 60.0, 1);
            if (!ok) printf("FAIL %d\n", id);
            fflush(stdout);
            for (int i = 0; i < ns; i++) free(src[i].xy);
            for (int i = 0; i < nd; i++) free(dst[i].xy);
            ns = nd = 0;
        }
    }
    return 0;
}
