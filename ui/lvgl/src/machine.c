#include "machine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

machine_t M;
const char *const AX_NAME[AX_N] = {"X", "Y", "Z", "A", "B", "C"};

#define HOME_POS (-2.0)       /* позиция след Home (маш. координата) */
#define HOME_SPEED 120.0      /* мм/с — ускорено, само за демото */

static int home_group = -1;   /* текуща група от реферирването */

void machine_init(void)
{
    memset(&M, 0, sizeof M);
    static const double mach[AX_N] = {-250, -180, -70, 0, 0, 0};
    static const double off[AX_N] = {-250, -180, -95, 0, 0, 0};
    static const int seq[AX_N] = {1, 1, 0, 2, 2, 2};
    for (int a = 0; a < AX_N; a++) {
        M.mach[a] = mach[a];
        M.off[a] = off[a];
        M.active[a] = a <= AX_Z;
        M.rotary[a] = a >= AX_A;
        M.home_sw[a] = true;
        M.home_seq[a] = seq[a];
        M.enc_on[a] = a != AX_C;
        M.ferr_lim[a] = 50;
    }
    M.state = MS_IDLE;
    M.type = MT_MILL;
    M.wcs = 1;
    M.tool = 5;
    snprintf(M.tool_desc, sizeof M.tool_desc, "Фреза 3Z ALU");
    M.tool_dia = 10;
    M.mcu_link = true;
    M.mcu_rate_hz = 1000;
}

double machine_work(int ax) { return M.mach[ax] - M.off[ax]; }

bool machine_need_home(void)
{
    for (int a = 0; a < AX_N; a++)
        if (M.active[a] && M.homed[a] == HOMED_NO) return true;
    return false;
}

void machine_toast(const char *txt, uint32_t now_ms, uint32_t dur_ms)
{
    snprintf(M.msg, sizeof M.msg, "%s", txt);
    M.msg_kind = MSG_INFO;
    M.msg_until = now_ms + dur_ms;
}

static bool homes(int a) { return M.active[a] && M.home_sw[a]; }

static int next_group(int after)
{
    int best = -1;
    for (int a = 0; a < AX_N; a++)
        if (homes(a) && M.home_seq[a] > after && (best < 0 || M.home_seq[a] < best)) best = M.home_seq[a];
    return best;
}

void machine_reset(uint32_t now_ms)
{
    if (M.homing) { machine_toast("Реферирането е в ход", now_ms, 3500); return; }
    if (M.ready && M.enabled) { machine_toast("Машината е активна. Аварийният стоп я изключва.", now_ms, 3500); return; }
    if (M.state == MS_RUN || M.state == MS_HOLD) { machine_toast("Спрете машината", now_ms, 3500); return; }

    M.state = MS_IDLE;
    M.enabled = true;
    home_group = next_group(-1);
    if (home_group < 0) {
        M.ready = true;
        machine_toast("Задвижванията са включени. Няма оси с датчик: реферирайте ръчно.", now_ms, 3500);
        return;
    }
    char axes[32] = "";
    for (int g = home_group; g >= 0; g = next_group(g))
        for (int a = 0; a < AX_N; a++)
            if (homes(a) && M.home_seq[a] == g) {
                if (axes[0]) strcat(axes, ", ");
                strcat(axes, AX_NAME[a]);
                M.homed[a] = HOMED_NO;
            }
    M.homing = true;
    char t[96];
    snprintf(t, sizeof t, "RESET: задвижванията са включени, реферирване на %s", axes);
    machine_toast(t, now_ms, 3500);
}

void machine_estop(void)
{
    M.state = MS_ESTOP;
    M.enabled = false;
    M.ready = false;
    M.homing = false;
    home_group = -1;
    for (int a = 0; a < AX_N; a++) {
        M.homed[a] = HOMED_NO;
        M.ferr[a] = 0;
    }
}

/* Демо: реферирване по групи (Z, после X и Y заедно) */
static void tick_homing(uint32_t now_ms, double dt)
{
    bool moving = false;
    for (int a = 0; a < AX_N; a++) {
        if (!homes(a) || M.home_seq[a] != home_group || M.homed[a] != HOMED_NO) continue;
        double d = HOME_POS - M.mach[a], step = HOME_SPEED * dt;
        if (fabs(d) <= step) {
            M.mach[a] = HOME_POS;
            M.homed[a] = HOMED_SENSOR;
            M.ferr[a] = 0;
        } else {
            M.mach[a] += d > 0 ? step : -step;
            M.ferr[a] = M.enc_on[a] ? 6.0 + 3.0 * sin(now_ms / 90.0 + a) : 0;
            moving = true;
        }
    }
    if (moving) return;
    home_group = next_group(home_group);
    if (home_group < 0) {
        M.homing = false;
        M.ready = true;
        machine_toast("Машината е активна и реферирана", now_ms, 6000);
    }
}

void machine_tick(uint32_t now_ms, uint32_t dt_ms)
{
    if (M.homing) tick_homing(now_ms, dt_ms / 1000.0);
}
