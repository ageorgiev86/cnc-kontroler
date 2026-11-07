#include "machine.h"
#include "program.h"
#include "config.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

machine_t M;
const char *const AX_NAME[AX_N] = {"X", "Y", "Z", "A", "B", "C"};

static double home_pos(int a) { char k[16]; snprintf(k, sizeof k, "%s.hpos", AX_NAME[a]); return cfg_num(k); }
#define HOME_SPEED 120.0      /* мм/с — ускорено, само за демото */

static int home_group = -1;   /* текуща група от реферирването */
static void tools_init(void);

void machine_init(void)
{
    memset(&M, 0, sizeof M);
    cfg_init();
    tools_init();
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
    M.jog_inc = 0.1;
    M.jog_f = 1000;
    M.sel_tool = 3;
    M.mag_pos = 5;
    M.mag_ph = 5;
    M.clamped = true;
    machine_apply_cfg();
}

/* Таблицата на инструментите (както в макета); по-късно се пази с настройките */
tool_t TT[TOOL_MAX + 1];

static void tools_init(void)
{
    static const struct { const char *n; double d, l, w; } T0[] = {
        {"Фреза 2Z", 6, 72.315, 0}, {"Фреза 3Z", 10, 84.902, 0.004}, {"Сферична R3", 6, 77.120, 0},
        {"Свредло", 8.5, 118.440, 0}, {"Фреза 3Z ALU", 10, 92.405, 0.002}, {"Метчик M10", 10, 96.770, 0},
        {"Центрово 90°", 6, 68.050, 0}, {"Челна фреза", 50, 61.230, 0.010}, {"Гравьор 30°", 3, 70.880, 0},
        {"3D пробник", 2, 104.600, 0}, {"Фаска 45°", 12, 80.310, 0}, {"Фреза 4Z", 16, 99.015, 0.006},
    };
    memset(TT, 0, sizeof TT);
    for (int i = 0; i < 12; i++) {
        snprintf(TT[i + 1].name, sizeof TT[i + 1].name, "%s", T0[i].n);
        TT[i + 1].dia = T0[i].d;
        TT[i + 1].len = T0[i].l;
        TT[i + 1].wear = T0[i].w;
    }
}

const char *machine_tool_name(int t, double *dia)
{
    if (t < 1 || t > TOOL_MAX || !TT[t].name[0]) { if (dia) *dia = 0; return "Празно гнездо"; }
    if (dia) *dia = TT[t].dia;
    return TT[t].name;
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

static void mq_clear(void);

void machine_estop(void)
{
    mq_clear();
    M.run_kind = RK_NONE;
    M.jog_dir = 0;
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
        double d = home_pos(a) - M.mach[a], step = HOME_SPEED * dt;
        if (fabs(d) <= step) {
            M.mach[a] = home_pos(a);
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
    if (M.state == MS_RUN && M.run_kind != RK_PROG) { machine_toast("Машината се движи. Изчакайте или натиснете СТОП.", now, 3500); return; }
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
    M.run_kind = RK_PROG;
}

void machine_hold(void)
{
    if (M.state == MS_RUN) { M.state = MS_HOLD; M.feed_act = 0; }
}

void machine_stop(uint32_t now)
{
    if (M.state != MS_RUN && M.state != MS_HOLD) return;
    mq_clear();
    M.run_kind = RK_NONE;
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
    if (M.state != MS_RUN || M.run_kind != RK_PROG) return;
    M.run_time += dt;
    float budget = dt * SIMX;          /* „номинални“ секунди за този кадър */
    while (budget > 0 && M.state == MS_RUN) {
        op_t *o = &P.op[M.op];
        if (M.line != o->line && o->kind != OP_END) M.line = o->line;
        if (o->kind == OP_END) {
            M.state = MS_IDLE; M.run_kind = RK_NONE; M.prog_done = true; M.done_t = P.t_total; M.feed_act = 0;
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
                else {
                    snprintf(t, sizeof t, "Смяна на инструмент T%d", o->tool);
                    act_msg(t, now);
                    snprintf(M.act_label, sizeof M.act_label, "%s", t);
                    M.sp_on = false;
                    M.mag_pos = o->tool;
                    int per = (mag_n() + mag_wheels() - 1) / mag_wheels();
                    M.wpos[(o->tool - 1) / per < 6 ? (o->tool - 1) / per : 5] = (o->tool - 1) % per + 1;
                }
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
            if (o->kind == OP_TOOL && o->tool != M.tool) { machine_select_tool(o->tool); M.act_label[0] = 0; }
            M.op_s = 0; M.op++; M.done_t = o->t1;
            continue;
        }
        M.op_s += budget;
        M.done_t = o->t0 + M.op_s;
        budget = 0;
    }
}

static void mq_tick(uint32_t now, float dt);
static void jog_tick(float dt);

void machine_tick(uint32_t now_ms, uint32_t dt_ms)
{
    if (dt_ms > 200) dt_ms = 200;   /* след пауза в опресняването не се прескача */
    if (M.homing) tick_homing(now_ms, dt_ms / 1000.0);
    tick_program(now_ms, dt_ms / 1000.0f);
    mq_tick(now_ms, dt_ms / 1000.0f);
    jog_tick(dt_ms / 1000.0f);
    machine_mag_tick(dt_ms / 1000.0f);
    if (M.state != MS_RUN && !M.homing)
        for (int a = 0; a < AX_N; a++) M.ferr[a] *= 0.8;
}

/* ---------- опашка от движения и действия ---------- */
#define MQ_MAX 64
typedef struct {
    bool move, ov;
    double tgt[AX_N];
    float feed, dur;
    mq_fn_t fn;
    int arg;
    char label[80];
} mq_t;

static mq_t mq[MQ_MAX];
static int mq_n, mq_i;
static bool mq_started;
static double mq_from[AX_N];
static float mq_len, mq_s;

static void mq_clear(void) { mq_n = mq_i = 0; mq_started = false; M.act_label[0] = 0; M.mtc = 0; M.mag_moving = false; M.orient = false; }
bool mq_busy(void) { return mq_i < mq_n; }

void mq_begin(runkind_t kind)
{
    mq_clear();
    M.run_kind = kind;
}

static mq_t *mq_add(const char *label)
{
    if (mq_n >= MQ_MAX) return NULL;
    mq_t *o = &mq[mq_n++];
    memset(o, 0, sizeof *o);
    if (label) snprintf(o->label, sizeof o->label, "%s", label);
    return o;
}

void mq_move(const double tgt[AX_N], float feed, bool ov, const char *label)
{
    mq_t *o = mq_add(label);
    if (!o) return;
    o->move = true;
    o->ov = ov;
    o->feed = feed;
    memcpy(o->tgt, tgt, sizeof o->tgt);
}

void mq_act(float dur, mq_fn_t fn, int arg, const char *label)
{
    mq_t *o = mq_add(label);
    if (!o) return;
    o->dur = dur;
    o->fn = fn;
    o->arg = arg;
}

void mq_go(void)
{
    if (!mq_n) { M.run_kind = RK_NONE; return; }
    M.state = MS_RUN;
}

static void mq_tick(uint32_t now, float dt)
{
    if (M.state != MS_RUN || M.run_kind == RK_PROG || M.run_kind == RK_NONE) return;
    float budget = dt * SIMX;
    while (budget > 0 && mq_i < mq_n && M.state == MS_RUN) {
        mq_t *o = &mq[mq_i];
        if (!mq_started) {
            mq_started = true;
            mq_s = 0;
            if (o->label[0]) { act_msg(o->label, now); snprintf(M.act_label, sizeof M.act_label, "%s", o->label); }
            if (o->move) {
                double l2 = 0;
                for (int a = 0; a < AX_N; a++) {
                    mq_from[a] = M.mach[a];
                    if (isnan(o->tgt[a])) o->tgt[a] = M.mach[a];
                    double d = o->tgt[a] - M.mach[a];
                    l2 += d * d;
                }
                mq_len = (float)sqrt(l2);
            }
        }
        if (o->move) {
            float feed = o->feed <= 0 ? RAPID * M.rov / 100.0f : o->ov ? o->feed * M.fov / 100.0f : o->feed;
            float v = feed / 60.0f;
            if (v <= 0) return;   /* оверрайд 0 % */
            M.feed_act = feed;
            float need = (mq_len - mq_s) / v;
            bool end = need <= budget;
            mq_s = end ? mq_len : mq_s + v * budget;
            budget = end ? budget - need : 0;
            float f = mq_len > 0 ? mq_s / mq_len : 1;
            for (int a = 0; a < AX_N; a++) {
                M.mach[a] = mq_from[a] + (o->tgt[a] - mq_from[a]) * f;
                M.togo[a] = o->tgt[a] - M.mach[a];
                if (a <= AX_Z) M.ferr[a] = M.enc_on[a] && !end ? 0.04f * feed / 60.0f * (0.6f + 0.4f * sinf(now / 70.0f + a)) : 0;
            }
            if (!end) return;
        } else {
            M.feed_act = 0;
            if (mq_s + budget < o->dur) { mq_s += budget; return; }
            budget -= o->dur - mq_s;
            if (o->fn) o->fn(o->arg, now);
            if (M.state != MS_RUN) return;   /* действието може да спре машината */
        }
        mq_i++;
        mq_started = false;
    }
    if (mq_i >= mq_n && M.state == MS_RUN) {
        mq_clear();
        M.act_label[0] = 0;
        M.state = MS_IDLE;
        M.run_kind = RK_NONE;
        M.feed_act = 0;
        for (int a = 0; a < AX_N; a++) M.togo[a] = 0;
    }
}

/* ---------- ръчно управление ---------- */
static void keep(double t[AX_N]) { for (int a = 0; a < AX_N; a++) t[a] = NAN; }

bool machine_can_move(uint32_t now)
{
    const char *why = NULL;
    if (M.state == MS_ESTOP) why = "Аварийният стоп е натиснат. Освободете го и натиснете RESET.";
    else if (M.state == MS_ALARM) why = "Има активна аларма. Отстранете причината и натиснете RESET.";
    else if (!M.enabled) why = "Машината е изключена. Натиснете RESET горе вляво.";
    else if (M.state != MS_IDLE || M.homing) why = "Машината се движи. Изчакайте или натиснете СТОП.";
    if (why) machine_toast(why, now, 3500);
    return !why;
}

static bool need_homed(uint32_t now)
{
    char t[96] = "";
    for (int a = 0; a < AX_N; a++)
        if (M.active[a] && M.homed[a] == HOMED_NO) {
            if (t[0]) strcat(t, ", ");
            strcat(t, AX_NAME[a]);
        }
    if (!t[0]) return false;
    char msg[160];
    snprintf(msg, sizeof msg, "Осите %s не са реферирани. Натиснете HOME.", t);
    machine_toast(msg, now, 3500);
    return true;
}

void machine_jog(int ax, int dir, uint32_t now)
{
    if (!M.active[ax]) { machine_toast("Оста е изключена в Настройки → Оси", now, 3500); return; }
    if (!machine_can_move(now)) return;
    if (M.jog_inc <= 0) { M.jog_ax = ax; M.jog_dir = dir; return; }
    double t[AX_N];
    keep(t);
    t[ax] = M.mach[ax] + dir * M.jog_inc;
    mq_begin(RK_JOG);
    mq_move(t, (float)M.jog_f, false, NULL);
    mq_go();
}

void machine_jog_release(void)
{
    if (!M.jog_dir) return;
    M.jog_dir = 0;
    M.feed_act = 0;
}

static void jog_tick(float dt)
{
    if (!M.jog_dir) return;
    if (M.state != MS_IDLE || !M.enabled) { M.jog_dir = 0; return; }
    M.mach[M.jog_ax] += M.jog_dir * M.jog_f / 60.0 * dt;
    M.feed_act = (float)M.jog_f;
}

static void home_done(int mask, uint32_t now)
{
    for (int a = 0; a < AX_N; a++)
        if (mask & (1 << a)) { M.homed[a] = HOMED_SENSOR; M.ferr[a] = 0; }
}

static void home_msg(int mask, uint32_t now)
{
    char t[120] = "Реферирането завърши: ";
    bool first = true;
    for (int a = 0; a < AX_N; a++)
        if (mask & (1 << a)) { if (!first) strcat(t, ", "); strcat(t, AX_NAME[a]); first = false; }
    machine_toast(t, now, 6000);
}

void machine_home(int ax, uint32_t now)
{
    if (!machine_can_move(now)) return;
    if (ax >= 0 && !M.active[ax]) { machine_toast("Оста е изключена в Настройки → Оси", now, 3500); return; }
    if (M.ref_manual) {
        char t[96] = "Ръчна референтна точка: ";
        bool first = true;
        for (int a = 0; a < AX_N; a++) {
            if (!M.active[a] || (ax >= 0 && a != ax)) continue;
            M.mach[a] = 0;
            M.homed[a] = HOMED_MANUAL;
            if (!first) strcat(t, ", ");
            strcat(t, AX_NAME[a]);
            first = false;
        }
        strcat(t, " = 0 (маш.)");
        machine_toast(t, now, 4000);
        return;
    }
    int all = 0;
    mq_begin(RK_HOME);
    /* по групи, както при RESET: първо Z, после X и Y, после въртящите */
    for (int g = next_group(-1); g >= 0; g = next_group(g)) {
        int mask = 0;
        double t[AX_N];
        keep(t);
        for (int a = 0; a < AX_N; a++)
            if (homes(a) && M.home_seq[a] == g && (ax < 0 || a == ax)) { mask |= 1 << a; t[a] = home_pos(a); M.homed[a] = HOMED_NO; }
        if (!mask) continue;
        char lbl[64] = "Реферирване ";
        for (int a = 0; a < AX_N; a++)
            if (mask & (1 << a)) strcat(lbl, AX_NAME[a]);
        mq_move(t, 3000, false, lbl);
        mq_act(0.3f, home_done, mask, NULL);
        all |= mask;
    }
    if (!all) { M.run_kind = RK_NONE; machine_toast("Оста няма датчик за Home. Използвайте „Ръчно, тук“.", now, 4000); return; }
    mq_act(0, home_msg, all, NULL);
    mq_go();
}

static void zero_msg(int arg, uint32_t now)
{
    char t[96];
    snprintf(t, sizeof t, "На програмната нула G%d. Z е на 5.000 над детайла.", 53 + M.wcs);
    machine_toast(t, now, 5000);
}

void machine_goto_zero(uint32_t now)
{
    if (!machine_can_move(now) || need_homed(now)) return;
    double t[AX_N];
    mq_begin(RK_JOG);
    keep(t);
    t[AX_Z] = 0;   /* Z нагоре до машинна нула */
    mq_move(t, 0, false, "Go to 0: Z нагоре до машинна нула");
    keep(t);
    for (int a = 0; a < AX_N; a++)
        if (a != AX_Z && M.active[a]) t[a] = M.off[a];
    mq_move(t, 0, false, "Go to 0: към програмната нула");
    keep(t);
    t[AX_Z] = fmin(5.0 + M.off[AX_Z], 0);
    mq_move(t, 0, false, "Go to 0: Z до безопасна височина");
    mq_act(0, zero_msg, 0, NULL);
    mq_go();
}

void machine_zero(int ax, uint32_t now)
{
    if (!machine_can_move(now)) return;
    char t[64];
    if (ax < 0) {
        for (int a = 0; a <= AX_Z; a++)
            if (M.active[a]) M.off[a] = M.mach[a];
        snprintf(t, sizeof t, "G%d X, Y, Z = 0", 53 + M.wcs);
    } else {
        if (!M.active[ax]) { machine_toast("Оста е изключена", now, 3000); return; }
        M.off[ax] = M.mach[ax];
        snprintf(t, sizeof t, "G%d %s = 0", 53 + M.wcs, AX_NAME[ax]);
    }
    machine_toast(t, now, 3500);
}

int machine_set_pos(int ax, bool mach, double v, uint32_t now)
{
    if (!M.active[ax]) { machine_toast("Оста е изключена в Настройки → Оси", now, 3500); return -1; }
    if (!machine_can_move(now)) return -1;
    char t[96];
    if (mach) {
        M.mach[ax] = v;
        M.homed[ax] = HOMED_MANUAL;
        snprintf(t, sizeof t, "Референтна точка %s: машинна позиция = %.3f", AX_NAME[ax], v);
    } else {
        M.off[ax] = M.mach[ax] - v;
        snprintf(t, sizeof t, "G%d %s: работна позиция = %.3f", 53 + M.wcs, AX_NAME[ax], v);
    }
    machine_toast(t, now, 3500);
    return 0;
}

void machine_coolant(uint32_t now)
{
    if (!M.enabled) { machine_can_move(now); return; }
    M.coolant = !M.coolant;
}

void machine_select_tool(int t)
{
    double dia;
    M.tool = t;
    snprintf(M.tool_desc, sizeof M.tool_desc, "%s", machine_tool_name(t, &dia));
    M.tool_dia = dia;
}

/* ---------- MDI ---------- */
static void mdi_spindle(int s, uint32_t now) { M.sp_cmd = s > 0 ? s : M.sp_cmd; M.sp_on = s > 0; }
static void mdi_cool(int on, uint32_t now) { M.coolant = on; }
static void mdi_tool(int t, uint32_t now) { M.sp_on = false; machine_select_tool(t); }
static void mdi_done(int n, uint32_t now) { machine_toast("MDI: изпълнено", now, 2500); }

int machine_mdi(const char *text, uint32_t now)
{
    if (!text || !*text) { machine_toast("Въведете команда", now, 3000); return -1; }
    if (!machine_can_move(now)) return -1;
    static int mode = 0;          /* модални: G0/G1, G90/G91, F */
    static bool inc = false;
    static float feed = 500;
    double pos[AX_N];
    for (int a = 0; a < AX_N; a++) pos[a] = machine_work(a);
    bool moves = false;
    mq_begin(RK_MDI);
    const char *s = text;
    int ln = 0;
    while (*s) {
        ln++;
        const char *e = strchr(s, '\n');
        size_t n = e ? (size_t)(e - s) : strlen(s);
        char line[256];
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, s, n);
        line[n] = 0;
        s = e ? e + 1 : s + n;

        double tgt[AX_N];
        memcpy(tgt, pos, sizeof tgt);
        bool has = false;
        int T = -1, S = -1;
        for (char *p = line; *p;) {
            if (*p == '(') { while (*p && *p != ')') p++; if (*p) p++; continue; }
            if (*p == ';') break;
            if (!isalpha((unsigned char)*p)) { p++; continue; }
            char l = (char)toupper((unsigned char)*p++);
            char *end;
            double v = strtod(p, &end);
            if (end == p) {
                char t[80];
                snprintf(t, sizeof t, "Ред %d: липсва стойност след %c", ln, l);
                machine_toast(t, now, 4000);
                mq_clear(); M.run_kind = RK_NONE;
                return -1;
            }
            p = end;
            int iv = (int)lround(v);
            int ax = l == 'X' ? AX_X : l == 'Y' ? AX_Y : l == 'Z' ? AX_Z : l == 'A' ? AX_A : l == 'B' ? AX_B : l == 'C' ? AX_C : -1;
            if (ax >= 0) {
                if (!M.active[ax]) {
                    char t[80];
                    snprintf(t, sizeof t, "Ос %c е изключена", l);
                    machine_toast(t, now, 4000);
                    mq_clear(); M.run_kind = RK_NONE;
                    return -1;
                }
                tgt[ax] = inc ? pos[ax] + v : v;
                has = true;
                continue;
            }
            switch (l) {
            case 'G':
                if (iv == 0 || iv == 1) mode = iv;
                else if (iv == 90) inc = false;
                else if (iv == 91) inc = true;
                else if (iv == 2 || iv == 3) { machine_toast("MDI: дъгите G2/G3 идват с интерпретатора", now, 4000); mq_clear(); M.run_kind = RK_NONE; return -1; }
                break;
            case 'F': feed = (float)v; break;
            case 'S': S = iv; break;
            case 'T': T = iv; break;
            case 'M':
                if (iv == 3 || iv == 4) mq_act(0.5f, mdi_spindle, S > 0 ? S : (M.sp_cmd ? M.sp_cmd : 10000), "MDI: шпинделът се пуска");
                else if (iv == 5) mq_act(0, mdi_spindle, 0, NULL);
                else if (iv == 8 || iv == 9) mq_act(0, mdi_cool, iv == 8, NULL);
                else if (iv == 6 && T >= 0) { mq_act(3.0f, mdi_tool, T, "MDI: смяна на инструмент"); moves = true; }
                break;
            default: break;
            }
        }
        if (S > 0 && M.sp_on) M.sp_cmd = S;
        if (has) {
            double t[AX_N];
            for (int a = 0; a < AX_N; a++) t[a] = tgt[a] + M.off[a];
            mq_move(t, mode == 0 ? 0 : feed, true, NULL);
            memcpy(pos, tgt, sizeof pos);
            moves = true;
        }
    }
    if (!mq_n) { M.run_kind = RK_NONE; machine_toast("MDI: няма какво да се изпълни", now, 3000); return -1; }
    if (moves && need_homed(now)) { mq_clear(); M.run_kind = RK_NONE; return -1; }
    mq_act(0, mdi_done, 0, NULL);
    mq_go();
    return 0;
}
