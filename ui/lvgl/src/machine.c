#include "machine.h"
#include "program.h"

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
    M.fov = M.sov = M.rov = 100;
    M.line = -1;
}

/* Таблицата на инструментите (като в макета); по-късно идва от настройките */
static const struct { const char *n; double d; } TOOLS[] = {
    {"", 0}, {"Фреза 2Z", 6}, {"Фреза 3Z", 10}, {"Сферична R3", 6}, {"Свредло", 8.5}, {"Фреза 3Z ALU", 10},
    {"Метчик M10", 10}, {"Центрово 90°", 6}, {"Челна фреза", 50}, {"Гравьор 30°", 3}, {"3D пробник", 2},
    {"Фаска 45°", 12}, {"Фреза 4Z", 16},
};

const char *machine_tool_name(int t, double *dia)
{
    if (t < 1 || t >= (int)(sizeof TOOLS / sizeof TOOLS[0])) { if (dia) *dia = 0; return "Празно гнездо"; }
    if (dia) *dia = TOOLS[t].d;
    return TOOLS[t].n;
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
    M.sp_on = false;
    M.coolant = false;
    M.feed_act = 0;
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

/* ---------- изпълнение на програмата (демо) ---------- */
#define SIMX 2.5f          /* демото върви 2,5 пъти по-бързо, както макетът */
#define RAPID 6000.0f
#define SP_RAMP 6000.0f    /* об/мин за секунда */

static void act_msg(const char *t, uint32_t now) { machine_toast(t, now, 2500); M.msg_kind = MSG_ACT; }

static void set_work(p3_t p)
{
    M.mach[AX_X] = p.x + M.off[AX_X];
    M.mach[AX_Y] = p.y + M.off[AX_Y];
    M.mach[AX_Z] = p.z + M.off[AX_Z];
}

static p3_t work_pos(void)
{
    p3_t p = {(float)machine_work(AX_X), (float)machine_work(AX_Y), (float)machine_work(AX_Z)};
    return p;
}

void machine_start(uint32_t now)
{
    if (M.state == MS_HOLD) { M.state = MS_RUN; return; }
    if (M.state == MS_ESTOP) { machine_toast("Аварийният стоп е натиснат. Натиснете RESET.", now, 3500); return; }
    if (M.state == MS_ALARM) { machine_toast("Има активна аларма. Натиснете RESET.", now, 3500); return; }
    if (!M.enabled) { machine_toast("Машината е изключена. Натиснете RESET горе вляво.", now, 3500); return; }
    if (M.homing) { machine_toast("Реферирането е в ход", now, 3500); return; }
    if (M.state == MS_RUN) return;
    if (machine_need_home()) { machine_toast("Осите не са реферирани. Натиснете HOME.", now, 3500); return; }
    if (!P.nop) { machine_toast("Няма заредена програма. Натиснете „Отвори“.", now, 3500); return; }
    if (M.prog_done || M.op >= P.nop - 1) { M.op = 0; M.prog_done = false; M.done_t = 0; }
    if (M.op == 0) { M.run_time = 0; M.op_s = 0; M.line = -1; }
    M.state = MS_RUN;
}

void machine_hold(void)
{
    if (M.state == MS_RUN) { M.state = MS_HOLD; M.feed_act = 0; }
}

void machine_stop(uint32_t now)
{
    if (M.state != MS_RUN && M.state != MS_HOLD) return;
    M.state = MS_IDLE;
    M.sp_on = false;
    M.coolant = false;
    M.feed_act = 0;
    for (int a = 0; a < AX_N; a++) M.togo[a] = 0;
    machine_toast("Движението е спряно", now, 3500);
}

void machine_rewind(uint32_t now)
{
    if (M.state != MS_IDLE) { machine_toast("Rewind е разрешен само при спряна машина", now, 3500); return; }
    M.op = 0; M.op_s = 0; M.line = -1; M.prog_done = false; M.done_t = 0; M.run_time = 0;
    machine_toast("Програмата е върната в началото", now, 3500);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
void machine_fov(int d, uint32_t now) { M.fov = d ? clampi(M.fov + d, 10, 200) : 100; }
void machine_sov(int d, uint32_t now) { M.sov = d ? clampi(M.sov + d, 50, 150) : 100; }
void machine_rov(int pct) { M.rov = pct; }

float machine_remaining_s(void)
{
    float rest = P.t_total - M.done_t;
    if (rest < 0) rest = 0;
    return rest / SIMX * 100.0f / (float)(M.fov > 10 ? M.fov : 10);
}

/* позиция по хода на разстояние s */
static p3_t along(const op_t *o, float s, p3_t *end)
{
    const p3_t *pt = &P.pt[o->p0];
    const float *cu = &P.cum[o->p0];
    *end = pt[o->np - 1];
    if (s <= 0) return pt[0];
    if (s >= o->len) return pt[o->np - 1];
    int i = 1;
    while (i < o->np - 1 && cu[i] < s) i++;
    float seg = cu[i] - cu[i - 1], f = seg > 0 ? (s - cu[i - 1]) / seg : 1;
    p3_t a = pt[i - 1], b = pt[i], r = {a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f};
    return r;
}

static void tick_program(uint32_t now, float dt)
{
    /* шпинделът набира и сваля обороти и при пауза */
    float tgt = M.sp_on ? (float)M.sp_cmd * M.sov / 100.0f : 0;
    float st = SP_RAMP * dt, d = tgt - M.sp_act;
    M.sp_act += fabsf(d) <= st ? d : (d > 0 ? st : -st);
    if (M.state != MS_RUN) return;
    M.run_time += dt;
    float budget = dt * SIMX;          /* „номинални“ секунди за този кадър */
    while (budget > 0 && M.state == MS_RUN) {
        op_t *o = &P.op[M.op];
        if (M.line != o->line && o->kind != OP_END) M.line = o->line;
        if (o->kind == OP_END) {
            M.state = MS_IDLE; M.prog_done = true; M.done_t = P.t_total; M.feed_act = 0;
            M.sp_on = false; M.coolant = false;
            for (int a = 0; a < AX_N; a++) M.togo[a] = 0;
            char t[80];
            snprintf(t, sizeof t, "Програмата приключи за %d:%02d", (int)M.run_time / 60, (int)M.run_time % 60);
            machine_toast(t, now, 6000);
            return;
        }
        if (o->kind == OP_MOVE) {
            float feed = o->rapid ? RAPID * M.rov / 100.0f : o->feed * M.fov / 100.0f;
            float v = feed / 60.0f;                  /* мм/s */
            float need = (o->len - M.op_s) / (v > 0 ? v : 1);
            M.feed_act = feed;
            M.feed_prog = o->rapid ? 0 : o->feed;
            p3_t end;
            if (need <= budget) {
                budget -= need;
                set_work(along(o, o->len, &end));
                M.op_s = 0; M.op++;
                M.done_t = o->t1;
                if (M.single && P.op[M.op].kind != OP_END) { M.state = MS_HOLD; machine_toast("Единичен блок: натиснете ЦИКЪЛ СТАРТ", now, 4000); }
                continue;
            }
            M.op_s += v * budget;
            budget = 0;
            p3_t p = along(o, M.op_s, &end), w;
            set_work(p);
            w = work_pos();
            M.togo[AX_X] = end.x - w.x; M.togo[AX_Y] = end.y - w.y; M.togo[AX_Z] = end.z - w.z;
            M.done_t = o->t0 + (o->t1 - o->t0) * (o->len > 0 ? M.op_s / o->len : 1);
            /* следяща грешка, пропорционална на скоростта (демо) */
            for (int a = 0; a <= AX_Z; a++) M.ferr[a] = M.enc_on[a] ? 0.04f * feed / 60.0f * (0.6f + 0.4f * sinf(now / 70.0f + a)) : 0;
            return;
        }
        /* действия: смяна на инструмент, шпиндел, охлаждане */
        float dur = o->t1 - o->t0;
        if (M.op_s == 0) {
            char t[96];
            if (o->kind == OP_TOOL) {
                if (o->tool == M.tool) dur = 0;
                else { snprintf(t, sizeof t, "Смяна на инструмент T%d", o->tool); act_msg(t, now); M.sp_on = false; }
            } else if (o->kind == OP_SPINDLE) {
                M.sp_cmd = o->s; M.sp_on = o->s > 0;
                if (o->s) { snprintf(t, sizeof t, "Шпинделът се пуска: S%d", o->s); act_msg(t, now); }
            } else if (o->kind == OP_COOL) {
                M.coolant = o->on;
            }
        }
        M.feed_act = 0;
        if (M.op_s + budget >= dur) {
            budget -= dur - M.op_s;
            if (o->kind == OP_TOOL && o->tool != M.tool) {
                M.tool = o->tool;
                double dia;
                snprintf(M.tool_desc, sizeof M.tool_desc, "%s", machine_tool_name(o->tool, &dia));
                M.tool_dia = dia;
            }
            M.op_s = 0; M.op++; M.done_t = o->t1;
            continue;
        }
        M.op_s += budget;
        M.done_t = o->t0 + M.op_s;
        budget = 0;
    }
}

void machine_tick(uint32_t now_ms, uint32_t dt_ms)
{
    if (M.homing) tick_homing(now_ms, dt_ms / 1000.0);
    tick_program(now_ms, dt_ms / 1000.0f);
    if (M.state != MS_RUN && !M.homing)
        for (int a = 0; a < AX_N; a++) M.ferr[a] *= 0.8;
}
