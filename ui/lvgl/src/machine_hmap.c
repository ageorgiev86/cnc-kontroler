/* Карта на височините (по модела на Autoleveler в UGS, както в ui/maket.html):
 * мрежа по стъпка между Мин и Макс (последната точка е на ръба), обхождане колона по колона по X,
 * змиевидно по Y. След допир Z се вдига с „Z отдръпване“ (% от Макс Z − Мин Z), най-много до Макс Z.
 * Корекция: Z += карта(x, y) − Z повърхност (билинейно), извън областта няма корекция.
 * Точките са много, затова се подават на опашката на порции (mq_set_refill). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "machine.h"

hmap_t HM = {0, 80, 0, 50, -1, 2, 10, 0, 25, 50, 1000, 0, 0, 1, {{0}}, 0, false, true};

static int next_pt;   /* следващата точка за опашката */

/* „истинската“ повърхност в демото: леко огъната платка, работна Z */
static double surf(double x, double y) { return .055 * sin(x / 23 + .4) + .035 * cos(y / 17) + .0009 * x - .0006 * y - .03; }

static double gx(int i) { return fmin(HM.x1, HM.x0 + i * HM.res); }
static double gy(int j) { return fmin(HM.y1, HM.y0 + j * HM.res); }

void hmap_grid(void)
{
    if (HM.res <= 0) HM.res = 1;
    HM.nx = (int)ceil((HM.x1 - HM.x0) / HM.res - 1e-9) + 1;
    HM.ny = (int)ceil((HM.y1 - HM.y0) / HM.res - 1e-9) + 1;
    if (HM.nx > HM_MAX) HM.nx = HM_MAX;
    if (HM.ny > HM_MAX) HM.ny = HM_MAX;
    if (HM.nx < 2) HM.nx = 2;
    if (HM.ny < 2) HM.ny = 2;
    HM.got = 0;
    HM.done = false;
    for (int i = 0; i < HM_MAX; i++)
        for (int j = 0; j < HM_MAX; j++) HM.z[i][j] = NAN;
}

static void latch(int k, uint32_t now)
{
    int i = k / HM_MAX, j = k % HM_MAX;
    HM.z[i][j] = (float)(machine_work(AX_Z) + ((double)rand() / RAND_MAX - 0.5) * 0.002);
    HM.got++;
    M.prb_cnt++;
    M.prb_flash = now;
}

static void finish(int a, uint32_t now)
{
    HM.done = true;
    HM.wcs = M.wcs;
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < HM.nx; i++)
        for (int j = 0; j < HM.ny; j++) { lo = fminf(lo, HM.z[i][j]); hi = fmaxf(hi, HM.z[i][j]); }
    char b[48], t[64];
    snprintf(t, sizeof t, "Карта на височините · G%d", 53 + M.wcs);
    machine_result(t, HM.apply ? "Корекцията по Z е включена за програмата." : "Корекцията по Z е изключена.");
    snprintf(b, sizeof b, "%d × %d = %d", HM.nx, HM.ny, HM.nx * HM.ny);
    machine_result_row("Точки", b);
    snprintf(b, sizeof b, "%.4f", lo);
    machine_result_row("Мин Z", b);
    snprintf(b, sizeof b, "%.4f", hi);
    machine_result_row("Макс Z", b);
    snprintf(b, sizeof b, "%.4f", hi - lo);
    machine_result_row("Разлика", b);
    machine_toast("Картата на височините е готова", now, 4000);
}

/* следващите до 10 точки */
static bool refill(void)
{
    int n = HM.nx * HM.ny;
    if (next_pt >= n) return false;
    double t[AX_N];
    for (int k = 0; k < 10 && next_pt < n; k++, next_pt++) {
        int i = next_pt / HM.ny, jj = next_pt % HM.ny, j = i % 2 ? HM.ny - 1 - jj : jj;
        double x = gx(i), y = gy(j), zs = surf(x, y);
        double up = fmin(HM.z1, zs + HM.ret / 100.0 * (HM.z1 - HM.z0));
        char l[64];
        snprintf(l, sizeof l, "Карта: точка %d от %d", next_pt + 1, n);
        for (int a = 0; a < AX_N; a++) t[a] = NAN;
        t[AX_X] = x + M.off[AX_X];
        t[AX_Y] = y + M.off[AX_Y];
        mq_move(t, (float)HM.scan, false, l);
        for (int a = 0; a < AX_N; a++) t[a] = NAN;
        t[AX_Z] = zs + M.off[AX_Z];
        mq_move(t, (float)HM.f, false, NULL);
        mq_act(0, latch, i * HM_MAX + j, NULL);
        t[AX_Z] = up + M.off[AX_Z];
        mq_move(t, (float)HM.scan, false, NULL);
    }
    if (next_pt >= n) mq_act(0, finish, 0, NULL);
    return true;
}

void hmap_scan(uint32_t now)
{
    if (!machine_sim()) { machine_toast("С истинската машина пробникът и toolsetter-ът идват с PROBE_ARM във firmware-а. Сега само в симулация.", now, 5000); return; }
    if (!machine_can_move(now)) return;
    if (!M.homed[AX_X] || !M.homed[AX_Y] || !M.homed[AX_Z]) { machine_toast("Осите X, Y, Z не са реферирани. Натиснете HOME.", now, 3500); return; }
    double z = machine_work(AX_Z);
    if (z < HM.z0 || z > HM.z1) {
        char t[220];
        snprintf(t, sizeof t, "Текущата Z (%.3f) трябва да е между Мин Z и Макс Z. Доведете сондата над платката.", z);
        machine_toast(t, now, 5000);
        return;
    }
    hmap_grid();
    next_pt = 0;
    char t[64];
    snprintf(t, sizeof t, "Карта на височините · G%d", 53 + M.wcs);
    machine_result(t, "Сканиране …");
    mq_begin(RK_PROBE);
    refill();
    mq_set_refill(refill);
    mq_go();
}

double hmap_corr(double x, double y)
{
    if (!HM.apply || !HM.done || HM.wcs != M.wcs) return 0;
    if (x < HM.x0 || x > HM.x1 || y < HM.y0 || y > HM.y1) return 0;
    double fx = (x - HM.x0) / HM.res, fy = (y - HM.y0) / HM.res;
    int i = (int)fx, j = (int)fy;
    if (i >= HM.nx - 1) i = HM.nx - 2;
    if (j >= HM.ny - 1) j = HM.ny - 2;
    double x0 = gx(i), x1 = gx(i + 1), y0 = gy(j), y1 = gy(j + 1);
    double u = x1 > x0 ? (x - x0) / (x1 - x0) : 0, v = y1 > y0 ? (y - y0) / (y1 - y0) : 0;
    double z = HM.z[i][j] * (1 - u) * (1 - v) + HM.z[i + 1][j] * u * (1 - v) + HM.z[i][j + 1] * (1 - u) * v + HM.z[i + 1][j + 1] * u * v;
    return z - HM.zs;
}
