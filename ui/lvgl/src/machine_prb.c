/* Нулеви точки G54–G59, детайли и циклите с 3D пробника (runCycle в ui/maket.html).
 * Демото „знае“ къде са стените на детайла и спира движението там, както би го спрял входът на пробника. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "machine.h"

bool prb_warn;

/* ---------- нулеви точки ---------- */
bool machine_wcs_isset(int n)
{
    for (int a = 0; a < AX_N; a++)
        if (M.wcs_off[n][a] != 0) return true;
    return false;
}

void machine_wcs_select(int n, uint32_t now)
{
    if (M.state != MS_IDLE) { machine_toast("Спрете машината, за да смените активната нула", now, 3500); return; }
    M.wcs = n;
    M.off = M.wcs_off[n];
    char t[120];
    snprintf(t, sizeof t, "Активна нула: G%d. Пробникът и „Ръчно“ записват в нея.", 53 + n);
    machine_toast(t, now, 3500);
}

void machine_wcs_toggle(int n, uint32_t now)
{
    if (n < 2 || n > 6) return;
    if (M.state != MS_IDLE) { machine_toast("Спрете машината", now, 3000); return; }
    char t[160];
    if (M.parts & (1 << n)) {
        M.parts &= ~(1 << n);
        if (M.wcs == n) { M.wcs = 1; M.off = M.wcs_off[1]; }
        snprintf(t, sizeof t, "G%d вече не е детайл", 53 + n);
    } else {
        M.parts |= 1 << n;
        M.wcs = n;
        M.off = M.wcs_off[n];
        snprintf(t, sizeof t, machine_wcs_isset(n) ? "G%d е детайл и е активна"
                                                   : "G%d е детайл и е активна: засечете нулата с цикъл на пробника или въведете координатите", 53 + n);
    }
    machine_toast(t, now, 4500);
}

/* ---------- цикли ---------- */
static void keep(double t[AX_N]) { for (int a = 0; a < AX_N; a++) t[a] = NAN; }

static double R[8];          /* латчнати позиции (работни) */
static double prb_pos[AX_N]; /* докъде стига опашката досега (машинни) */
static char cyc_name[64];

static void latch_x(int i, uint32_t now) { R[i] = machine_work(AX_X); M.prb_cnt++; M.prb_flash = now; }
static void latch_y(int i, uint32_t now) { R[i] = machine_work(AX_Y); M.prb_cnt++; M.prb_flash = now; }
static void latch_z(int i, uint32_t now) { R[i] = M.mach[AX_Z]; M.prb_cnt++; M.prb_flash = now; }

/* Допир по ос: търсене до стената, отдръпване, бавно докосване, отдръпване. trig е разстоянието до стената. */
static void touch(int ax, int dir, double trig, const char *label, int slot)
{
    double fast = cfg_num("prb.prFast"), slow = cfg_num("prb.prSlow"), back = cfg_num("prb.prBack");
    /* стената е спрямо мястото, докъдето стига опашката (движенията в нея са абсолютни) */
    double t[AX_N], wall = prb_pos[ax] + dir * trig;
    char l[96];
    keep(t); t[ax] = wall;
    snprintf(l, sizeof l, "%s: търсене", label);
    mq_move(t, (float)fast, false, l);
    keep(t); t[ax] = wall - dir * back;
    snprintf(l, sizeof l, "%s: отдръпване", label);
    mq_move(t, 1500, false, l);
    keep(t); t[ax] = wall + dir * ((double)rand() / RAND_MAX - 0.5) * 0.004;
    snprintf(l, sizeof l, "%s: измерване", label);
    mq_move(t, (float)slow, false, l);
    mq_act(0, ax == AX_X ? latch_x : ax == AX_Y ? latch_y : latch_z, slot, NULL);
    keep(t); t[ax] = wall - dir * back;
    mq_move(t, 1500, false, NULL);
    prb_pos[ax] = wall - dir * back;
}

static void move_to(int ax, double mach, float feed, const char *label)
{
    double t[AX_N];
    keep(t);
    t[ax] = mach;
    mq_move(t, feed, false, label);
    prb_pos[ax] = mach;
}

static char f4b[8][24];
static const char *f4(int i, double v) { snprintf(f4b[i], sizeof f4b[i], "%.4f", v); return f4b[i]; }

static double ball(void) { return cfg_num("prb.prDia") / 2; }

static void done_z(int a, uint32_t now)
{
    double z = R[0] + (!strcmp(cfg_str("prb.prMount"), "fixed") ? cfg_num("prb.prOffZ") : 0);
    M.off[AX_Z] = z;
    char k[32];
    machine_result("Повърхност Z", "Работната нула по Z е на повърхността.");
    machine_result_row("Z маш.", f4(0, z));
    snprintf(k, sizeof k, "Нов G%d Z", 53 + M.wcs);
    machine_result_row(k, f4(1, z));
}

static void done_hole(int width, uint32_t now)
{
    double rb = ball(), ox = cfg_num("prb.prOffX"), oy = cfg_num("prb.prOffY");
    double xp = R[0] + ox, xm = R[1] + ox, cx = (xp + xm) / 2, dx = xp - xm + 2 * rb;
    char n[96];
    if (width == 0) {
        double yp = R[2] + oy, ym = R[3] + oy, cy = (yp + ym) / 2, dy = yp - ym + 2 * rb;
        M.off[AX_X] += cx;
        M.off[AX_Y] += cy;
        snprintf(n, sizeof n, "G%d X и Y са преместени в центъра.", 53 + M.wcs);
        machine_result("Център на отвор", n);
        machine_result_row("Център X", f4(0, cx));
        machine_result_row("Център Y", f4(1, cy));
        machine_result_row("Ø по X", f4(2, dx));
        machine_result_row("Ø по Y", f4(3, dy));
        machine_result_row("Овалност", f4(4, fabs(dx - dy)));
    } else if (width == 1) {
        M.off[AX_X] += cx;
        snprintf(n, sizeof n, "G%d X е преместен в центъра на жлеба.", 53 + M.wcs);
        machine_result("Ширина на жлеб X", n);
        machine_result_row("Център X", f4(0, cx));
        machine_result_row("Ширина", f4(1, dx));
    } else {
        double yp = R[2] + oy, ym = R[3] + oy, cy = (yp + ym) / 2, dy = yp - ym + 2 * rb;
        M.off[AX_Y] += cy;
        snprintf(n, sizeof n, "G%d Y е преместен в центъра на жлеба.", 53 + M.wcs);
        machine_result("Ширина на жлеб Y", n);
        machine_result_row("Център Y", f4(0, cy));
        machine_result_row("Ширина", f4(1, dy));
    }
}

static void done_boss(int a, uint32_t now)
{
    double rb = ball(), ox = cfg_num("prb.prOffX"), oy = cfg_num("prb.prOffY");
    double xm = R[0] + ox, xp = R[1] + ox, ym = R[2] + oy, yp = R[3] + oy;
    double cx = (xp + xm) / 2, cy = (yp + ym) / 2, dx = xp - xm - 2 * rb, dy = yp - ym - 2 * rb;
    M.off[AX_X] += cx;
    M.off[AX_Y] += cy;
    char n[96];
    snprintf(n, sizeof n, "G%d X и Y са преместени в центъра на боса.", 53 + M.wcs);
    machine_result("Център на бос", n);
    machine_result_row("Център X", f4(0, cx));
    machine_result_row("Център Y", f4(1, cy));
    machine_result_row("Ø по X", f4(2, dx));
    machine_result_row("Ø по Y", f4(3, dy));
}

static int corner_sx, corner_sy, corner_inner;
static void done_corner(int a, uint32_t now)
{
    double rb = ball(), x = R[0] + cfg_num("prb.prOffX") + corner_sx * rb, y = R[1] + cfg_num("prb.prOffY") + corner_sy * rb;
    M.off[AX_X] += x;
    M.off[AX_Y] += y;
    char n[96], d[32];
    snprintf(n, sizeof n, corner_inner ? "G%d X и Y са в ъгъла на джоба." : "G%d X и Y са в ъгъла на детайла.", 53 + M.wcs);
    machine_result(cyc_name, n);
    machine_result_row("Ъгъл X", f4(0, x));
    machine_result_row("Ъгъл Y", f4(1, y));
    snprintf(d, sizeof d, "X%s / Y%s", corner_sx > 0 ? "+" : "−", corner_sy > 0 ? "+" : "−");
    machine_result_row("Посоки", d);
}

static void done_rot(int a, uint32_t now)
{
    double dx = 30, dy = R[1] - R[0], ang = atan2(dy, dx) * 180 / M_PI;
    char b[32];
    machine_result("Ротация на детайла", "Ъгълът се прилага като завъртане на координатите (G68) от интерпретатора.");
    machine_result_row("Точка 1 Y", f4(0, R[0]));
    machine_result_row("Точка 2 Y", f4(1, R[1]));
    snprintf(b, sizeof b, "%.4f°", ang);
    machine_result_row("Ъгъл", b);
}

static void dep(int on, uint32_t now) { (void)on; }

/* калибриране в пръстен: ефективният диаметър на сферата и офсетът спрямо оста на шпиндела */
static void done_cal(int a, uint32_t now)
{
    double ring = cfg_num("prb.prRing"), xp = R[0], xm = R[1], yp = R[2], ym = R[3];
    double dx = ring - (xp - xm), dy = ring - (yp - ym), d = (dx + dy) / 2;
    double cx = (xp + xm) / 2 + M.off[AX_X], cy = (yp + ym) / 2 + M.off[AX_Y];
    double ox = cfg_num("prb.prRingX") - cx, oy = cfg_num("prb.prRingY") - cy;
    drf_set_num("prb.prDia", d, 3); cfg_commit_key("prb.prDia");
    drf_set_num("prb.prOffX", ox, 3); cfg_commit_key("prb.prOffX");
    drf_set_num("prb.prOffY", oy, 3); cfg_commit_key("prb.prOffY");
    machine_result("Калибриране в пръстен", "Записано в Настройки → 3D пробник.");
    machine_result_row("Ефективен Ø", f4(0, d));
    machine_result_row("Офсет X", f4(1, ox));
    machine_result_row("Офсет Y", f4(2, oy));
    machine_result_row("Ø по X / Y", f4(3, dx - dy));
}

void machine_probe_cycle(const char *k, int corner, uint32_t now)
{
    if (!machine_can_move(now)) return;
    bool fx = !strcmp(cfg_str("prb.prMount"), "fixed");
    if (!fx && M.tool != 10) { prb_warn = true; return; }
    if (fx && M.sp_on) { machine_toast("Спрете шпиндела преди измерване с пробника", now, 3500); return; }
    if (!M.homed[AX_X] || !M.homed[AX_Y] || !M.homed[AX_Z]) { machine_toast("Осите X, Y, Z не са реферирани. Натиснете HOME.", now, 3500); return; }
    prb_warn = false;
    for (int a = 0; a < AX_N; a++) prb_pos[a] = M.mach[a];
    double rb = ball(), x0 = M.mach[AX_X], y0 = M.mach[AX_Y];
    double rnd = (double)rand() / RAND_MAX;
    mq_begin(RK_PROBE);
    int depo = (int)cfg_num("prb.prDep");
    if (fx && depo > 0) mq_act((float)cfg_num("prb.prDepT"), dep, 1, "Спускане на пробника");

    if (!strcmp(k, "cal")) {
        /* истината в демото: ефективен Ø 1.962 и малко биене на стилуса */
        double rt = 1.962 / 2, Rr = cfg_num("prb.prRing") / 2, cx = cfg_num("prb.prRingX") - 0.014, cy = cfg_num("prb.prRingY") + 0.009;
        move_to(AX_X, cx, 0, "Към центъра на пръстена");
        move_to(AX_Y, cy, 0, NULL);
        touch(AX_X, 1, Rr - rt, "X+", 0);
        move_to(AX_X, cx, 2000, NULL);
        touch(AX_X, -1, Rr - rt, "X−", 1);
        move_to(AX_X, cx, 2000, NULL);
        touch(AX_Y, 1, Rr - rt, "Y+", 2);
        move_to(AX_Y, cy, 2000, NULL);
        touch(AX_Y, -1, Rr - rt, "Y−", 3);
        move_to(AX_Y, cy, 2000, NULL);
        mq_act(0, done_cal, 0, NULL);
    } else if (!strcmp(k, "z")) {
        touch(AX_Z, -1, 6 + rnd * 8, "Повърхност Z", 0);
        mq_act(0, done_z, 0, NULL);
        move_to(AX_Z, prb_pos[AX_Z] + 5, 0, NULL);
    } else if (!strcmp(k, "hole") || !strcmp(k, "width") || !strcmp(k, "widthy")) {
        int kind = !strcmp(k, "hole") ? 0 : !strcmp(k, "width") ? 1 : 2;
        double Rh = (kind == 2 ? 8 : 12.5) + rnd * 0.03, ox = (rnd - 0.5) * 2, oy = ((double)rand() / RAND_MAX - 0.5) * 2;
        if (kind != 2) {
            touch(AX_X, 1, ox + Rh - rb, "X+", 0);
            move_to(AX_X, x0, 2000, NULL);
            touch(AX_X, -1, Rh - rb - ox, "X−", 1);
            move_to(AX_X, x0 + ox, 2000, "Сферата към центъра по X");
        }
        if (kind != 1) {
            touch(AX_Y, 1, oy + Rh - rb, "Y+", 2);
            move_to(AX_Y, y0, 2000, NULL);
            touch(AX_Y, -1, Rh - rb - oy, "Y−", 3);
            move_to(AX_Y, y0 + oy, 2000, "Сферата към центъра по Y");
        }
        mq_act(0, done_hole, kind, NULL);
    } else if (!strcmp(k, "boss")) {
        /* бос Ø30: отвън навътре по четирите страни, с вдигане на Z над боса */
        double Rb = 15 + rnd * 0.03, z0 = M.mach[AX_Z], cl = Rb + rb + 5;
        const struct { int ax, dir; double c; int slot; const char *n; } S4[4] = {
            {AX_X, 1, -cl, 0, "X+"}, {AX_X, -1, cl, 1, "X−"}, {AX_Y, 1, -cl, 2, "Y+"}, {AX_Y, -1, cl, 3, "Y−"}};
        for (int i = 0; i < 4; i++) {
            move_to(AX_Z, z0 + 8, 0, "Z над боса");
            move_to(S4[i].ax, (S4[i].ax == AX_X ? x0 : y0) + S4[i].c, 0, NULL);
            move_to(AX_Z, z0 - 4, 1500, NULL);
            touch(S4[i].ax, S4[i].dir, 5, S4[i].n, S4[i].slot);
        }
        move_to(AX_Z, z0 + 8, 0, NULL);
        move_to(AX_X, x0, 0, NULL);
        move_to(AX_Y, y0, 0, NULL);
        mq_act(0, done_boss, 0, NULL);
    } else if (!strcmp(k, "rot")) {
        touch(AX_Y, 1, 5 + rnd, "Точка 1", 0);
        move_to(AX_X, x0 + 30, 2000, "Към точка 2");
        touch(AX_Y, 1, 5 + rnd + 0.06, "Точка 2", 1);
        move_to(AX_X, x0, 2000, NULL);
        mq_act(0, done_rot, 0, NULL);
    } else if (!strcmp(k, "corner") || !strcmp(k, "icorner")) {
        static const int CX[4] = {1, -1, 1, -1}, CY[4] = {1, 1, -1, -1};
        static const char *const CN[4] = {"преден ляв", "преден десен", "заден ляв", "заден десен"};
        corner_inner = !strcmp(k, "icorner");
        corner_sx = corner_inner ? -CX[corner] : CX[corner];
        corner_sy = corner_inner ? -CY[corner] : CY[corner];
        snprintf(cyc_name, sizeof cyc_name, "%s ъгъл · %s", corner_inner ? "Вътрешен" : "Външен", CN[corner]);
        char l[32];
        snprintf(l, sizeof l, "Стена X%s", corner_sx > 0 ? "+" : "−");
        touch(AX_X, corner_sx, 4 + rnd * 3, l, 0);
        move_to(AX_X, x0, 2000, NULL);
        snprintf(l, sizeof l, "Стена Y%s", corner_sy > 0 ? "+" : "−");
        touch(AX_Y, corner_sy, 4 + (double)rand() / RAND_MAX * 3, l, 1);
        move_to(AX_Y, y0, 2000, NULL);
        mq_act(0, done_corner, 0, NULL);
    }
    if (fx && depo > 0) mq_act((float)cfg_num("prb.prDepT"), dep, 0, "Прибиране на пробника");
    mq_go();
}
