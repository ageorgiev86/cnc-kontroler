/* Инструменти в модела на машината: магазин, автоматична и ръчна смяна, измерване с toolsetter
 * и прилагане на настройките. Последователностите са като в ui/maket.html (atcOp, measureOps). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "machine.h"

static const char *const AXK[6] = {"X", "Y", "Z", "A", "B", "C"};

static double axc(int a, const char *k)
{
    char key[32];
    snprintf(key, sizeof key, "%s.%s", AXK[a], k);
    return cfg_num(key);
}

int mag_n(void)
{
    if (!strcmp(cfg_str("mach.type"), "lathe")) return (int)cfg_num("mach.ltTur");
    int n = (int)cfg_num("mag.mn");
    return n < 1 ? 1 : n > TOOL_MAX ? TOOL_MAX : n;
}

int mag_wheels(void)
{
    if (!cfg_num("mag.mOn") || strcmp(cfg_str("mag.mtype"), "carousel")) return 1;
    int w = (int)cfg_num("mag.mWheels");
    return w < 1 ? 1 : w > 6 ? 6 : w;
}

bool machine_auto_tc(void) { return cfg_num("mag.mOn") && cfg_num("mag.atcOn"); }

/* ---------- резултат ---------- */
void machine_result(const char *title, const char *note)
{
    prb_result_t *r = &M.res;
    r->seq++;
    snprintf(r->title, sizeof r->title, "%s", title);
    snprintf(r->note, sizeof r->note, "%s", note ? note : "");
    r->nrows = 0;
    r->warn = false;
}

void machine_result_row(const char *k, const char *v)
{
    prb_result_t *r = &M.res;
    if (r->nrows >= 6) return;
    snprintf(r->rows[r->nrows][0], sizeof r->rows[0][0], "%s", k);
    snprintf(r->rows[r->nrows][1], sizeof r->rows[0][1], "%s", v);
    r->nrows++;
    r->seq++;
}

/* ---------- смяна на инструмент ---------- */
static void keep(double t[AX_N]) { for (int a = 0; a < AX_N; a++) t[a] = NAN; }

static int per_wheel(void) { return (mag_n() + mag_wheels() - 1) / mag_wheels(); }
static int wheel_of(int i) { int w = (i - 1) / per_wheel(); return w < mag_wheels() ? w : mag_wheels() - 1; }
static int place_of(int i) { return (i - 1) % per_wheel() + 1; }

static void a_spindle_off(int arg, uint32_t now) { M.sp_on = false; M.coolant = false; }
static void a_orient(int arg, uint32_t now) { M.orient = true; }
static void a_mag_to(int t, uint32_t now) { M.mag_pos = t; M.wpos[wheel_of(t)] = place_of(t); M.mag_moving = true; }
static void a_release(int t, uint32_t now) { M.mag_moving = false; M.clamped = false; machine_select_tool(0); }
static void a_clamp(int t, uint32_t now) { M.mag_moving = false; M.clamped = true; machine_select_tool(t); }
static void a_g43(int t, uint32_t now) { M.orient = false; }
static void a_mtc_wait(int t, uint32_t now) { M.mtc = t; M.state = MS_HOLD; M.feed_act = 0; }
static void a_mtc_done(int t, uint32_t now) { M.mtc = 0; M.clamped = true; M.mag_pos = t; machine_select_tool(t); }

static void measure_ops(int t);

void machine_tool_change(int t, uint32_t now)
{
    if (!machine_can_move(now)) return;
    char lbl[200], msg[200];
    if (t == M.tool) { snprintf(msg, sizeof msg, "T%d вече е в шпиндела", t); machine_toast(msg, now, 3000); return; }
    if (t < 1 || t > mag_n()) { snprintf(msg, sizeof msg, "Няма гнездо %d. Магазинът има %d гнезда.", t, mag_n()); machine_toast(msg, now, 3500); return; }
    bool atc = machine_auto_tc();
    if (!TT[t].name[0] && atc) { snprintf(msg, sizeof msg, "Гнездо %d е празно", t); machine_toast(msg, now, 3000); return; }
    int old = M.tool;
    double tg[AX_N];
#define L(fmt, ...) (snprintf(lbl, sizeof lbl, "Смяна на инструмент T%d: " fmt, t, ##__VA_ARGS__), lbl)
    mq_begin(RK_TOOL);
    if (!atc) {   /* ръчна смяна */
        double x = cfg_num("mag.mcX"), y = cfg_num("mag.mcY"), z = cfg_num("mag.mcZ");
        mq_act(0.4f, a_spindle_off, 0, L("спиране на шпиндела"));
        keep(tg); tg[AX_Z] = z;
        mq_move(tg, 0, false, L("Z нагоре до %.3f", z));
        keep(tg); tg[AX_X] = x; tg[AX_Y] = y;
        mq_move(tg, 0, false, L("към позицията за ръчна смяна"));
        mq_act(0, a_mtc_wait, t, NULL);
        mq_act(0, a_mtc_done, t, NULL);
        mq_act(0.3f, a_g43, t, L("корекция на дължината G43 H%d", t));
        if (cfg_num("mag.mcMeas") && t != 10) {
            if (M.homed[AX_X] && M.homed[AX_Y] && M.homed[AX_Z]) measure_ops(t);
            else { snprintf(msg, sizeof msg, "Осите не са реферирани: T%d няма да се измери с toolsetter-а", t); machine_toast(msg, now, 4000); }
        }
        mq_go();
        return;
    }
    mq_act(0.4f, a_spindle_off, 0, L("спиране на шпиндела"));
    mq_act(0.6f, a_orient, 0, L("ориентация на шпиндела на %.1f°", cfg_num("mag.mor")));
    if (!strcmp(cfg_str("mag.mtype"), "linear")) {
        double x1 = cfg_num("mag.m1x"), y1 = cfg_num("mag.m1y"), pitch = cfg_num("mag.mpitch"), mz = cfg_num("mag.mz");
        bool byx = !strcmp(cfg_str("mag.maxis"), "X");
        keep(tg); tg[AX_Z] = 0;
        mq_move(tg, 0, false, L("Z нагоре"));
        if (old) {
            keep(tg); tg[AX_X] = x1 + (byx ? (old - 1) * pitch : 0); tg[AX_Y] = y1 + (byx ? 0 : (old - 1) * pitch);
            mq_move(tg, 0, false, L("шпинделът към гнездо %d", old));
            mq_act(0, a_mag_to, old, NULL);
            keep(tg); tg[AX_Z] = mz;
            mq_move(tg, 2000, false, L("Z надолу до %.3f", mz));
            mq_act(0.6f, a_release, old, L("освобождаване на T%d", old));
            keep(tg); tg[AX_Z] = 0;
            mq_move(tg, 0, false, L("Z нагоре без инструмент"));
        }
        keep(tg); tg[AX_X] = x1 + (byx ? (t - 1) * pitch : 0); tg[AX_Y] = y1 + (byx ? 0 : (t - 1) * pitch);
        mq_move(tg, 0, false, L("шпинделът към гнездо %d", t));
        mq_act(0, a_mag_to, t, NULL);
        keep(tg); tg[AX_Z] = mz;
        mq_move(tg, 2000, false, L("Z надолу до %.3f", mz));
        mq_act(0.6f, a_clamp, t, L("затягане на T%d", t));
        keep(tg); tg[AX_Z] = 0;
        mq_move(tg, 0, false, L("Z нагоре"));
    } else {
        /* позиция за смяна: X и/или Y се включват с отметка; без отметки шпинделът остава, където е */
        bool px = cfg_num("mag.mXOn") != 0, py = cfg_num("mag.mYOn") != 0;
        if (px || py) {
            keep(tg); tg[AX_Z] = 0;
            mq_move(tg, 0, false, L("Z нагоре"));
            keep(tg);
            if (px) tg[AX_X] = cfg_num("mag.mX");
            if (py) tg[AX_Y] = cfg_num("mag.mY");
            mq_move(tg, 0, false, L("към позицията за смяна"));
        }
        const char *nm = !strcmp(cfg_str("mag.mtype"), "chain") ? "веригата" : "магазинът";
        int per = per_wheel(), wn = mag_wheels();
        float msp = (float)cfg_num("mag.msp");
        int at[6];
        memcpy(at, M.wpos, sizeof at);
#define DIST(a, b) (abs((a) - (b)) % per < per - abs((a) - (b)) % per ? abs((a) - (b)) % per : per - abs((a) - (b)) % per)
        char wl[40];
        if (old) {
            int w = wheel_of(old);
            if (wn > 1) snprintf(wl, sizeof wl, "колело %d", w + 1); else snprintf(wl, sizeof wl, "%s", nm);
            mq_act(fmaxf(0.3f, DIST(at[w], place_of(old)) * msp), a_mag_to, old, L("%s към гнездо %d", wl, old));
            at[w] = place_of(old);
            mq_act(0.6f, a_release, old, L("освобождаване на T%d", old));
        }
        int w = wheel_of(t);
        if (wn > 1) snprintf(wl, sizeof wl, "колело %d", w + 1); else snprintf(wl, sizeof wl, "%s", nm);
        if (wn > 1 && (!old || wheel_of(old) != w)) mq_act(0.5f, NULL, 0, L("избор на колело %d", w + 1));
        mq_act(fmaxf(0.3f, DIST(at[w], place_of(t)) * msp), a_mag_to, t, L("%s към гнездо %d", wl, t));
        mq_act(0.6f, a_clamp, t, L("затягане на T%d", t));
#undef DIST
    }
    mq_act(0.3f, a_g43, t, L("корекция на дължината G43 H%d", t));
#undef L
    mq_go();
}

/* ---------- измерване на дължината с toolsetter-а ---------- */
static double meas_len, meas_old;
static int meas_t;

static void a_ts_touch(int t, uint32_t now)
{
    M.ts_cnt++;
    M.ts_flash = now;
    double L = meas_len;
    char b[48], title[48];
    snprintf(title, sizeof title, "Дължина на T%d", t);
    machine_result(title, "Таблицата на инструментите е обновена.");
    snprintf(b, sizeof b, "%.4f", L);
    machine_result_row("Нова дължина", b);
    snprintf(b, sizeof b, "%.4f", meas_old);
    machine_result_row("Предишна", b);
    snprintf(b, sizeof b, "%.4f", L - meas_old);
    machine_result_row("Разлика", b);
    TT[t].len = L;
}

static void measure_ops(int t)
{
    double tsz = cfg_num("ts.tsz"), top = cfg_num("ts.tstop");
    meas_t = t;
    meas_old = TT[t].len;
    meas_len = TT[t].len + ((double)rand() / RAND_MAX - 0.5) * 0.03;
    double tg[AX_N];
    char lbl[96];
    keep(tg); tg[AX_Z] = tsz;
    snprintf(lbl, sizeof lbl, "Toolsetter: Z нагоре до %.3f", tsz);
    mq_move(tg, 0, false, lbl);
    keep(tg); tg[AX_X] = cfg_num("ts.tsx"); tg[AX_Y] = cfg_num("ts.tsy");
    snprintf(lbl, sizeof lbl, "Toolsetter: към X %.3f Y %.3f", tg[AX_X], tg[AX_Y]);
    mq_move(tg, 0, false, lbl);
    double trig = top + meas_len;
    keep(tg); tg[AX_Z] = trig + 1;
    mq_move(tg, (float)cfg_num("ts.tsvs"), false, "Toolsetter: търсене");
    keep(tg); tg[AX_Z] = trig;
    mq_move(tg, (float)cfg_num("ts.tsvl"), false, "Toolsetter: бавно докосване");
    mq_act(0.1f, a_ts_touch, t, NULL);
    keep(tg); tg[AX_Z] = tsz;
    mq_move(tg, 0, false, NULL);
}

void machine_measure_tool(uint32_t now)
{
    if (!machine_sim()) { machine_toast("С истинската машина пробникът и toolsetter-ът идват с PROBE_ARM във firmware-а. Сега само в симулация.", now, 5000); return; }
    if (!machine_can_move(now)) return;
    if (!M.tool) { machine_toast("В шпиндела няма инструмент", now, 3000); return; }
    if (M.tool == 10) { machine_toast("3D пробникът не се мери с toolsetter-а", now, 3500); return; }
    if (!M.homed[AX_X] || !M.homed[AX_Y] || !M.homed[AX_Z]) { machine_toast("Осите X, Y, Z не са реферирани. Натиснете HOME.", now, 3500); return; }
    mq_begin(RK_PROBE);
    measure_ops(M.tool);
    mq_go();
    char t[64];
    snprintf(t, sizeof t, "Измерване на T%d с toolsetter-а", M.tool);
    machine_toast(t, now, 3000);
}

/* ---------- анимация на магазина ---------- */
static float approach(float cur, float tgt, float step, int n, bool ring)
{
    float d = tgt - cur;
    if (ring && n > 0) d = fmodf(fmodf(d, (float)n) + n * 1.5f, (float)n) - n / 2.0f;
    if (fabsf(d) <= step) return cur + d;
    return cur + (d > 0 ? step : -step);
}

void machine_mag_tick(float dt)
{
    int n = mag_n();
    bool lin = !strcmp(cfg_str("mag.mtype"), "linear");
    float sp = (lin ? 4.0f : 1.0f / fmaxf(0.05f, (float)cfg_num("mag.msp"))) * dt;
    M.mag_ph = approach(M.mag_ph, (float)M.mag_pos, sp, n, !lin);
    if (!lin) M.mag_ph = fmodf(M.mag_ph - 1 + n * 4, (float)n) + 1;
    int per = per_wheel();
    for (int w = 0; w < mag_wheels(); w++) {
        M.wph[w] = approach(M.wph[w], (float)M.wpos[w], sp, per, true);
        M.wph[w] = fmodf(M.wph[w] - 1 + per * 4, (float)per) + 1;
    }
}

/* ---------- настройки → модел ---------- */
void machine_apply_cfg(void)
{
    for (int a = 0; a < AX_N; a++) {
        M.active[a] = axc(a, "active") != 0;
        M.home_sw[a] = axc(a, "hsw") != 0;
        M.home_seq[a] = (int)axc(a, "hseq");
        M.enc_on[a] = axc(a, "encOn") != 0;
        M.ferr_lim[a] = axc(a, "encFerr");
    }
    const char *t = cfg_str("mach.type");
    M.type = !strcmp(t, "laser") ? MT_LASER : !strcmp(t, "plasma") ? MT_PLASMA : !strcmp(t, "lathe") ? MT_LATHE : MT_MILL;
    if (M.mag_pos > mag_n()) M.mag_pos = 1;
    if (M.sel_tool > mag_n()) M.sel_tool = 1;
    /* друг брой гнезда на колело: колелата започват от гнездо 1, а текущото е на позицията за смяна */
    static int last_per;
    int per = per_wheel();
    if (per != last_per) {
        last_per = per;
        for (int w = 0; w < 6; w++) M.wpos[w] = 1, M.wph[w] = 1;
        M.wpos[wheel_of(M.mag_pos)] = place_of(M.mag_pos);
        M.wph[wheel_of(M.mag_pos)] = (float)place_of(M.mag_pos);
        M.mag_ph = (float)M.mag_pos;
    }
}
