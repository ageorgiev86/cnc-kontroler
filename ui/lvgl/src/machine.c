/* Машината за интерфейса: истинското управление е в core/ (mc.c: интерпретатор, планер и връзката
 * със STM32 по SPI, docs/interpretator-planer.md, раздел 7). Тук командите от екраните отиват към mc,
 * а M се пълни от моментната му снимка на всеки кадър. В симулатора (и с „Връзка: Симулация“)
 * отсреща е виртуален STM32 (core/src/vmcu.c) със същия протокол. */
#include "machine.h"
#include "program.h"
#include "config.h"
#include "io.h"

#include "cnc/gcode.h"
#include "cnc/link.h"
#include "cnc/mc.h"
#include "cnc/vmcu.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

machine_t M;
const char *const AX_NAME[AX_N] = {"X", "Y", "Z", "A", "B", "C"};

static mc_t *MC;
static vmcu_t *VM;
static mc_snap_t SN;
static bool link_sim;
static char link_err[200];
static uint32_t link_retry;
static char cur_lnk[16], cur_dev[64];
static unsigned cur_hz;
static int last_msg_seq, last_done_seq;
static bool manual_ref[AX_N];
static bool reset_home;          /* след RESET: реферирване на осите с датчик */
static bool local_alarm;         /* аларма от интерфейса (machine_alarm) до RESET */
static void tools_init(void);
static void mq_clear(void);
static void link_open(uint32_t now);
static void mq_tick(uint32_t now);

static double axc(int a, const char *k)
{
    char key[32];
    snprintf(key, sizeof key, "%s.%s", AX_NAME[a], k);
    return cfg_num(key);
}

/* началото на симулацията: машинната позиция при включване (за виртуалните датчици) */
static const double SIM_START[AX_N] = {-250, -180, -70, 0, 0, 0};

void machine_init(void)
{
    memset(&M, 0, sizeof M);
    cfg_init();
    io_defaults();
    tools_init();
    static const double off[7][3] = {{0}, {-250, -180, -95}, {-140, -180, -95}, {-250, -110, -95}, {-140, -110, -95}, {0}, {0}};
    for (int a = 0; a < AX_N; a++) {
        for (int w = 1; w <= 6; w++) M.wcs_off[w][a] = a <= AX_Z ? off[w][a] : 0;
        M.active[a] = a <= AX_Z;
        M.rotary[a] = a >= AX_A;
        M.home_sw[a] = true;
        M.enc_on[a] = false;
        M.ferr_lim[a] = 50;
    }
    M.state = MS_ALARM;
    snprintf(M.alarm, sizeof M.alarm, "Свързване със STM32…");
    M.type = MT_MILL;
    M.wcs = 1;
    M.off = M.wcs_off[1];
    M.parts = 1 << 1;
    M.tool = 5;
    snprintf(M.tool_desc, sizeof M.tool_desc, "Фреза 3Z ALU");
    M.tool_dia = 10;
    M.fov = M.sov = M.rov = 100;
    M.line = -1;
    M.jog_inc = 0.1;
    M.jog_f = 1000;
    M.sel_tool = 3;
    M.mag_pos = 5;
    M.mag_ph = 5;
    M.clamped = true;
    machine_apply_cfg();
    link_open(0);
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
    snprintf(M.msg, sizeof M.msg, "%.199s", txt);
    M.msg_kind = MSG_INFO;
    M.msg_until = now_ms + dur_ms;
}


/* ---------- връзката ---------- */
static int out_bit(const char *f)
{
    for (int i = 0; i < 30 && i < IO_N; i++) {
        char k[24];
        snprintf(k, sizeof k, "io.out%d.f", i + 1);
        if (!strcmp(cfg_str(k), f)) return i;
    }
    return -1;
}

/* настройките → mc */
static void build_cfg(mc_cfg_t *c)
{
    memset(c, 0, sizeof *c);
    for (int a = 0; a < AX_N; a++) {
        mc_axis_t *x = &c->ax[a];
        x->active = axc(a, "active") != 0;
        x->rotary = a >= AX_A;
        x->spm = axc(a, "spm") > 0 ? axc(a, "spm") : 100;
        x->vmax = axc(a, "vmax") / 60.0;
        x->amax = axc(a, "acc");
        x->jmax = x->amax * 40;          /* рампа на ускорението около 25 ms */
        x->smin = axc(a, "min");
        x->smax = axc(a, "max");
        x->home_sw = axc(a, "hsw") != 0;
        x->home_dir = axc(a, "hdir") > 0 ? 1 : -1;
        x->home_seq = (int)axc(a, "hseq");
        x->home_search = axc(a, "hvs") / 60.0;
        x->home_latch = axc(a, "hvl") / 60.0;
        x->home_pull = axc(a, "hpull");
        x->home_sw_pos = axc(a, "hswp");
        x->home_after = axc(a, "hpos");
        x->dir_inv = axc(a, "mdir") != 0;
    }
    c->tol = cfg_num("mach.gTol") > 0 ? cfg_num("mach.gTol") : 0.02;
    c->arc_tol = 0.002;
    c->sp_delay = cfg_num("sp.spDelay") > 0 ? cfg_num("sp.spDelay") : 2.0;
    c->out_flood = out_bit("cool");
    c->out_mist = out_bit("mist");
    c->out_air = out_bit("air");
    c->opt_stop = true;
}

static void link_close(void)
{
    if (MC) mc_free(MC);
    MC = NULL;
    if (VM) vmcu_free(VM);
    VM = NULL;
}

static void link_open(uint32_t now)
{
    link_close();
    link_retry = now + 2000;
    mc_cfg_t c;
    build_cfg(&c);
    snprintf(cur_lnk, sizeof cur_lnk, "%s", cfg_str("mcu.lnk"));
    snprintf(cur_dev, sizeof cur_dev, "%s", cfg_str("mcu.lnkDev"));
    cur_hz = (unsigned)cfg_num("mcu.lnkHz");
    link_sim = CNC_UI_SIM || strcmp(cur_lnk, "spi") != 0;   /* симулаторът е винаги с виртуален STM32 */
    lk_port_t *port;
    if (link_sim) {
        VM = vmcu_new();
        for (int a = 0; a < AX_N; a++) {   /* датчиците: машината тръгва от SIM_START */
            double sp = c.ax[a].spm;
            vmcu_set_home_switch(VM, a, (int32_t)(vmcu_pos_q(VM, a) / 65536 + llround((c.ax[a].home_sw_pos - SIM_START[a]) * sp)));
        }
        port = vmcu_port(VM);
    } else {
        port = lk_spi_open(cur_dev[0] ? cur_dev : "/dev/spidev0.0", cur_hz ? cur_hz : 4000000, link_err, sizeof link_err);
        if (!port) return;
    }
    link_err[0] = 0;
    MC = mc_new(port, &c);
    if (!MC || mc_start(MC)) { snprintf(link_err, sizeof link_err, "Нишката на управлението не тръгна"); link_close(); return; }
    mc_override(MC, M.fov / 100.0, M.rov / 100.0);
    last_msg_seq = last_done_seq = 0;
}

void machine_link_info(machine_link_t *l)
{
    memset(l, 0, sizeof *l);
    l->sim = link_sim;
    if (!MC) {
        snprintf(l->state, sizeof l->state, "%.95s", link_err[0] ? link_err : "няма");
        return;
    }
    snprintf(l->port, sizeof l->port, "%s", SN.port);
    l->link = SN.link;
    l->rate = SN.pkt_rate;
    l->rx_errors = SN.rx_errors;
    l->bad_rx = SN.bad_rx;
    l->queue = SN.queue;
    static const char *const ST[] = {"няма връзка", "грешка", "авариен стоп", "готова", "реферирване", "програма", "пауза", "движение", "чака Старт", "спира"};
    snprintf(l->state, sizeof l->state, "%s", SN.state <= MC_STOPPING ? ST[SN.state] : "?");
}

bool machine_sim(void) { return link_sim; }

/* ---------- снимката на mc → M ---------- */
static homed_t homed_of(int a) { return !SN.homed[a] ? HOMED_NO : manual_ref[a] ? HOMED_MANUAL : HOMED_SENSOR; }

/* ходът от P за реда, който се изпълнява (за прогреса в траекторията) */
static int op_of_line(int line)
{
    int lo = 0, hi = P.nop - 1;
    if (line < 0 || hi < 0) return 0;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (P.op[mid].line < line) lo = mid + 1; else hi = mid;
    }
    return lo;
}

static void from_snap(uint32_t now, float dt)
{
    mc_snapshot(MC, &SN);
    M.mcu_link = SN.link;
    M.mcu_rate_hz = (int)lround(SN.pkt_rate);
    for (int a = 0; a < AX_N; a++) {
        M.mach[a] = SN.mach[a];
        M.homed[a] = homed_of(a);
        M.ferr[a] = 0;
        M.togo[a] = 0;
    }
    M.homing = SN.state == MC_HOMING;
    bool busy_mq = mq_busy();
    switch (SN.state) {
    case MC_NOLINK: case MC_FAULT:
        M.state = MS_ALARM;
        snprintf(M.alarm, sizeof M.alarm, "%s", SN.alarm);
        break;
    case MC_ESTOP: M.state = MS_ESTOP; break;
    case MC_IDLE: M.state = busy_mq ? (M.mtc ? MS_HOLD : MS_RUN) : MS_IDLE; break;
    case MC_HOLD: case MC_WAIT: M.state = MS_HOLD; break;
    default: M.state = MS_RUN; break;
    }
    if (local_alarm && M.state != MS_ESTOP) M.state = MS_ALARM;
    M.enabled = SN.state != MC_NOLINK && SN.state != MC_FAULT && SN.state != MC_ESTOP && !local_alarm;
    M.ready = M.enabled && !machine_need_home();
    M.feed_act = (float)(SN.v * 60.0);
    M.sp_on = SN.sp_dir != 0;
    if (SN.sp_rpm > 0) M.sp_cmd = (int)SN.sp_rpm;
    {   /* показаните обороти: рампа до зададените (без енкодер на шпиндела) */
        float tgt = M.sp_on ? (float)M.sp_cmd * M.sov / 100.0f : 0, st = 6000.0f * dt, d = tgt - M.sp_act;
        M.sp_act += fabsf(d) <= st ? d : (d > 0 ? st : -st);
    }
    M.coolant = SN.cool != 0;
    if (M.run_kind == RK_PROG || M.run_kind == RK_MDI) {
        if (SN.line >= 0) {
            M.line = SN.line;
            if (M.run_kind == RK_PROG) {
                M.op = op_of_line(SN.line);
                M.op_s = 0;
                M.done_t = M.op < P.nop ? P.op[M.op].t0 : M.done_t;
            }
        }
        if (SN.tool > 0 && SN.tool != M.tool) machine_select_tool(SN.tool);
        /* ръчна смяна от програмата: екранът „Инструменти“ показва „Готово, продължи“ (M.mtc) */
        if (SN.state == MC_WAIT && SN.wait_kind == MC_WAIT_TOOL) {
            M.mtc = SN.wait_tool;
            snprintf(M.act_label, sizeof M.act_label, "Ръчна смяна: поставете T%d и натиснете ЦИКЪЛ СТАРТ", SN.wait_tool);
        } else if (!mq_busy()) { M.mtc = 0; M.act_label[0] = 0; }
        if (M.state == MS_RUN) M.run_time += dt;
    }
    if (SN.msg_seq != last_msg_seq) {
        last_msg_seq = SN.msg_seq;
        if (SN.msg[0]) machine_toast(SN.msg, now, 5000);
    }
    /* край на програмата или MDI */
    if (SN.done_seq != last_done_seq) {
        last_done_seq = SN.done_seq;
        if (M.run_kind == RK_PROG && SN.prog_done) {
            M.prog_done = true;
            M.done_t = M.t_total;
            M.op = P.nop ? P.nop - 1 : 0;
            char t[80];
            snprintf(t, sizeof t, "Програмата приключи за %d:%02d", (int)M.run_time / 60, (int)M.run_time % 60);
            machine_toast(t, now, 6000);
        } else if (M.run_kind == RK_MDI) machine_toast("MDI: изпълнено", now, 2500);
        if (M.run_kind == RK_PROG || M.run_kind == RK_MDI || M.run_kind == RK_HOME || M.run_kind == RK_JOG) {
            if (!mq_busy()) M.run_kind = RK_NONE;
        }
    }
    if (SN.state == MC_IDLE && !mq_busy() && M.run_kind != RK_NONE && M.run_kind != RK_PROBE && M.run_kind != RK_TOOL) M.run_kind = RK_NONE;
    if ((SN.state == MC_FAULT || SN.state == MC_ESTOP || SN.state == MC_NOLINK) && mq_busy()) { mq_clear(); M.run_kind = RK_NONE; }
    /* RESET: щом машината е готова, реферирването на осите с датчик */
    if (reset_home && SN.state == MC_IDLE) {
        reset_home = false;
        unsigned mask = 0;
        char axes[32] = "";
        for (int a = 0; a < AX_N; a++)
            if (M.active[a] && M.home_sw[a] && !SN.homed[a]) {
                mask |= 1u << a;
                if (axes[0]) strcat(axes, ", ");
                strcat(axes, AX_NAME[a]);
            }
        if (mask) {
            mc_home(MC, mask);
            M.run_kind = RK_HOME;
            char t[96];
            snprintf(t, sizeof t, "RESET: задвижванията са включени, реферирване на %s", axes);
            machine_toast(t, now, 3500);
        } else machine_toast("Машината е активна", now, 3500);
    }
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


/* ---------- RESET, авария ---------- */
void machine_reset(uint32_t now_ms)
{
    if (!MC) { link_open(now_ms); machine_toast(link_err[0] ? link_err : "Свързване със STM32…", now_ms, 4000); return; }
    if (M.state == MS_RUN || M.state == MS_HOLD) { machine_toast("Спрете машината", now_ms, 3500); return; }
    if (!SN.link) { machine_toast("Няма връзка със STM32", now_ms, 3500); return; }
    if (M.ready && M.enabled) { machine_toast("Машината е активна. Аварийният стоп я изключва.", now_ms, 3500); return; }
    local_alarm = false;
    mc_reset(MC);
    reset_home = true;
}

void machine_alarm(const char *why)
{
    if (MC) mc_stop(MC);
    mq_clear();
    M.run_kind = RK_NONE;
    M.jog_dir = 0;
    local_alarm = true;
    M.state = MS_ALARM;
    M.enabled = false;
    M.ready = false;
    snprintf(M.alarm, sizeof M.alarm, "%s. Машината е спряна.", why);
}

void machine_estop(void)
{
    if (MC) mc_estop(MC);
    mq_clear();
    M.run_kind = RK_NONE;
    M.jog_dir = 0;
    M.state = MS_ESTOP;
    M.enabled = false;
    M.ready = false;
    reset_home = false;
}

int machine_parts(int *list)
{
    int n = 0;
    for (int w = 1; w <= 6; w++)
        if (w == 1 || (M.parts & (1 << w))) list[n++] = w;
    return n;
}

void machine_skip_part(uint32_t now)
{
    machine_toast("„Пропусни детайла“ идва със следващата версия на управлението", now, 3500);
}

/* ---------- програма ---------- */
static unsigned axes_mask(unsigned *rot)
{
    unsigned m = 0;
    for (int a = 0; a < AX_N; a++)
        if (M.active[a]) m |= 1u << a;
    *rot = m & (7u << AX_A);
    return m | 7u;
}

/* нулите от „Офсети“, дължините спрямо инструмента в шпиндела, диаметрите за G41/G42 */
static void tables(gc_tables_t *t)
{
    gc_tables_init(t);
    for (int w = 1; w <= 6; w++)
        for (int a = 0; a < AX_N; a++) t->wcs[w][a] = M.wcs_off[w][a];
    double ref = M.tool > 0 && M.tool <= TOOL_MAX ? TT[M.tool].len + TT[M.tool].wear : 0;
    for (int k = 0; k <= TOOL_MAX && k < GC_TOOL_N; k++) {
        t->tool[k].dia = TT[k].dia;
        t->tool[k].len = TT[k].name[0] ? TT[k].len + TT[k].wear - ref : 0;
    }
}

static int run_text(const char *text, const gc_job_t *job, runkind_t kind)
{
    gc_config_t g;
    memset(&g, 0, sizeof g);
    g.dialect = !strcmp(cfg_str("mach.gDial"), "haas") ? GC_DIALECT_HAAS : GC_DIALECT_MACH3;
    g.axes = axes_mask(&g.rotary);
    gc_tables_t *t = malloc(sizeof *t);
    if (!t) return -1;
    tables(t);
    mc_set_tool(MC, M.tool);
    int r = mc_run(MC, text, &g, t, job);
    free(t);
    if (r == 0) M.run_kind = kind;
    return r;
}

void machine_start(uint32_t now)
{
    if (!MC || !SN.link) { machine_toast("Няма връзка със STM32", now, 3500); return; }
    if (SN.state == MC_HOLD || SN.state == MC_WAIT) { mc_continue(MC); return; }
    if (M.mtc && mq_busy()) { M.mtc = 0; return; }   /* ръчна смяна от „Инструменти“: продължава опашката */
    if (M.state == MS_RUN) { machine_toast("Машината се движи. Изчакайте или натиснете СТОП.", now, 3500); return; }
    if (M.state == MS_ESTOP) { machine_toast("Аварийният стоп е натиснат. Натиснете RESET.", now, 3500); return; }
    if (M.state == MS_ALARM) { machine_toast("Има активна аларма. Натиснете RESET.", now, 3500); return; }
    if (machine_need_home()) { machine_toast("Осите не са реферирани. Натиснете HOME.", now, 3500); return; }
    if (!P.nop || !P.nlines) { machine_toast("Няма заредена програма. Натиснете „Отвори“.", now, 3500); return; }
    if (P.err_line >= 0) {
        char t[260];
        snprintf(t, sizeof t, "Програмата има грешка: ред %d: %.190s", P.err_line + 1, P.err);
        machine_toast(t, now, 6000);
        return;
    }
    char *text = program_join(&P, NULL);
    int parts[6], np = machine_parts(parts);
    gc_job_t job;
    memset(&job, 0, sizeof job);
    job.parts = parts;
    job.nparts = np > 1 ? np : 0;
    job.start_line = M.start_op > 0 && M.start_line > 0 ? M.start_line - 1 : -1;
    job.start_part = 0;
    job.safe_z = axc(AX_Z, "hpos");
    job.approach = 5;
    M.prog_done = false;
    M.done_t = 0;
    M.run_time = 0;
    M.t_total = P.t_total * (np > 1 ? np : 1);
    M.part_n = np > 1 ? np : 1;
    M.part_i = 0;
    int r = run_text(text, job.nparts || job.start_line >= 0 ? &job : NULL, RK_PROG);
    free(text);
    M.start_op = 0;
    M.start_line = 0;
    if (r) machine_toast("Програмата не може да тръгне (памет)", now, 4000);
    else if (np > 1) {
        char t[96];
        snprintf(t, sizeof t, "%d детайла: всеки инструмент минава по всички нули", np);
        machine_toast(t, now, 4000);
    }
}

void machine_hold(void)
{
    if (!MC) return;
    if (M.run_kind == RK_PROG || M.run_kind == RK_MDI) mc_hold(MC, true);
    else if (M.state == MS_RUN) { mc_stop(MC); mq_clear(); }
}

void machine_stop(uint32_t now)
{
    if (M.state != MS_RUN && M.state != MS_HOLD) return;
    if (MC) mc_stop(MC);
    mq_clear();
    M.run_kind = RK_NONE;
    M.jog_dir = 0;
    machine_toast("Движението е спряно", now, 3500);
}

void machine_rewind(uint32_t now)
{
    if (M.state != MS_IDLE) { machine_toast("Rewind е разрешен само при спряна машина", now, 3500); return; }
    M.op = 0; M.op_s = 0; M.line = -1; M.prog_done = false; M.done_t = 0; M.run_time = 0;
    machine_toast("Програмата е върната в началото", now, 3500);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void ovr(void) { if (MC) mc_override(MC, M.fov / 100.0, M.rov / 100.0); }
void machine_fov(int d, uint32_t now) { M.fov = d ? clampi(M.fov + d, 10, 200) : 100; ovr(); }
void machine_sov(int d, uint32_t now) { M.sov = d ? clampi(M.sov + d, 50, 150) : 100; }
void machine_rov(int pct) { M.rov = pct; ovr(); }

float machine_remaining_s(void)
{
    float total = M.t_total > 0 ? M.t_total : P.t_total, rest = total - M.done_t;
    return rest > 0 ? rest * 100.0f / (float)(M.fov > 10 ? M.fov : 10) : 0;
}

void machine_tick(uint32_t now_ms, uint32_t dt_ms)
{
    if (dt_ms > 200) dt_ms = 200;
    float dt = dt_ms / 1000.0f;
    /* сменена връзка в настройките: отваря се наново при спряна машина */
    bool ch = strcmp(cur_lnk, cfg_str("mcu.lnk")) || strcmp(cur_dev, cfg_str("mcu.lnkDev")) || cur_hz != (unsigned)cfg_num("mcu.lnkHz");
    if (ch && M.state != MS_RUN && M.state != MS_HOLD) link_open(now_ms);
    if (!MC) {
        if ((int32_t)(now_ms - link_retry) >= 0) link_open(now_ms);
        M.mcu_link = false;
        M.state = MS_ALARM;
        M.enabled = M.ready = false;
        snprintf(M.alarm, sizeof M.alarm, "Няма връзка със STM32: %.150s", link_err[0] ? link_err : "?");
    } else {
        from_snap(now_ms, dt);
    }
    mq_tick(now_ms);
    machine_mag_tick(dt);
    io_tick(dt, now_ms);
}

/* ---------- опашка от движения и действия (смяна, Go to 0, пробник …): всяко движение е ход на mc ---------- */
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
static uint32_t mq_t0;
static int mq_seq;

static bool (*mq_refill)(void);
void mq_set_refill(bool (*fn)(void)) { mq_refill = fn; }
static void mq_clear(void) { mq_refill = NULL; mq_n = mq_i = 0; mq_started = false; M.act_label[0] = 0; M.mtc = 0; M.mag_moving = false; M.orient = false; }
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

static void act_msg(const char *t, uint32_t now) { machine_toast(t, now, 2500); M.msg_kind = MSG_ACT; }

static void mq_tick(uint32_t now)
{
    while (MC && mq_i < mq_n && !M.mtc) {
        mq_t *o = &mq[mq_i];
        if (!mq_started) {
            if (SN.state != MC_IDLE) return;   /* предишното движение още не е свършило */
            mq_started = true;
            mq_t0 = now;
            if (o->label[0]) { act_msg(o->label, now); snprintf(M.act_label, sizeof M.act_label, "%s", o->label); }
            if (o->move) {
                mq_seq = SN.done_seq;
                double t[MC_AXES];
                for (int a = 0; a < AX_N; a++) t[a] = o->tgt[a];
                mc_move(MC, t, o->feed > 0 ? o->feed * (o->ov ? M.fov / 100.0 : 1) : 0);
                return;
            }
        }
        if (o->move) {
            if (SN.done_seq == mq_seq) {
                if (SN.state == MC_IDLE && now - mq_t0 > 1500) { mq_clear(); M.run_kind = RK_NONE; machine_toast("Движението не тръгна", now, 3500); }
                return;
            }
        } else {
            if (now - mq_t0 < (uint32_t)(o->dur * 1000)) return;
            if (o->fn) o->fn(o->arg, now);
            if (!mq_n) return;   /* действието може да е спряло опашката */
        }
        mq_i++;
        mq_started = false;
        if (mq_i >= mq_n && mq_refill) {   /* следващата порция */
            mq_n = mq_i = 0;
            bool (*fn)(void) = mq_refill;
            if (!fn()) mq_refill = NULL;
        }
    }
    if (mq_n && mq_i >= mq_n) {
        mq_clear();
        M.act_label[0] = 0;
        if (M.run_kind != RK_PROG) M.run_kind = RK_NONE;
    }
}

/* ---------- ръчно управление ---------- */
static void keep(double t[AX_N]) { for (int a = 0; a < AX_N; a++) t[a] = NAN; }

bool machine_can_move(uint32_t now)
{
    const char *why = NULL;
    if (!MC || !SN.link) why = "Няма връзка със STM32. Проверете кабела и Настройки → Връзка с MCU.";
    else if (M.state == MS_ESTOP) why = "Аварийният стоп е натиснат. Освободете го и натиснете RESET.";
    else if (M.state == MS_ALARM) why = "Има активна аларма. Отстранете причината и натиснете RESET.";
    else if (!M.enabled) why = "Машината е изключена. Натиснете RESET горе вляво.";
    else if (M.state != MS_IDLE || M.homing) why = "Машината се движи. Изчакайте или натиснете СТОП.";
    if (why) machine_toast(why, now, 3500);
    return !why;
}

void machine_jog(int ax, int dir, uint32_t now)
{
    if (!M.active[ax]) { machine_toast("Оста е изключена в Настройки → Оси", now, 3500); return; }
    if (!machine_can_move(now)) return;
    if (M.jog_inc <= 0) { M.jog_ax = ax; M.jog_dir = dir; mc_jog(MC, ax, dir, 0, M.jog_f); M.run_kind = RK_JOG; return; }
    mc_jog(MC, ax, dir, M.jog_inc, M.jog_f);
    M.run_kind = RK_JOG;
}

void machine_jog_release(void)
{
    if (!M.jog_dir) return;
    M.jog_dir = 0;
    if (MC) mc_jog_stop(MC);
}

void machine_home(int ax, uint32_t now)
{
    if (!machine_can_move(now)) return;
    if (ax >= 0 && !M.active[ax]) { machine_toast("Оста е изключена в Настройки → Оси", now, 3500); return; }
    if (M.ref_manual) {   /* „Ръчно, тук“: сегашната позиция е машинната 0 */
        char t[96] = "Ръчна референтна точка: ";
        bool first = true;
        for (int a = 0; a < AX_N; a++) {
            if (!M.active[a] || (ax >= 0 && a != ax)) continue;
            mc_set_pos(MC, a, 0);
            manual_ref[a] = true;
            if (!first) strcat(t, ", ");
            strcat(t, AX_NAME[a]);
            first = false;
        }
        strcat(t, " = 0 (маш.)");
        machine_toast(t, now, 4000);
        return;
    }
    unsigned mask = 0;
    for (int a = 0; a < AX_N; a++)
        if (M.active[a] && M.home_sw[a] && (ax < 0 || a == ax)) { mask |= 1u << a; manual_ref[a] = false; }
    if (!mask) { machine_toast("Оста няма датчик за Home. Използвайте „Ръчно, тук“.", now, 4000); return; }
    mc_home(MC, mask);
    M.run_kind = RK_HOME;
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
    t[AX_Z] = axc(AX_Z, "max");   /* Z нагоре до горния мек лимит */
    mq_move(t, 0, false, "Go to 0: Z нагоре");
    keep(t);
    for (int a = 0; a < AX_N; a++)
        if (a != AX_Z && M.active[a]) t[a] = M.off[a];
    mq_move(t, 0, false, "Go to 0: към програмната нула");
    keep(t);
    t[AX_Z] = fmin(5.0 + M.off[AX_Z], axc(AX_Z, "max"));
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
        mc_set_pos(MC, ax, v);
        manual_ref[ax] = true;
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
    int b = out_bit("cool");
    if (b < 0) b = out_bit("air");
    if (b < 0) { machine_toast("Няма изход с функция „Охлаждане“ (Настройки → Входове/изходи)", now, 4000); return; }
    M.coolant = !M.coolant;
    mc_outputs(MC, M.coolant ? 1u << b : 0, 1u << b);
}

void machine_select_tool(int t)
{
    double dia;
    M.tool = t;
    snprintf(M.tool_desc, sizeof M.tool_desc, "%s", machine_tool_name(t, &dia));
    M.tool_dia = dia;
    if (MC && t != SN.tool) mc_set_tool(MC, t);
}

/* ---------- MDI: през същия интерпретатор и планер ---------- */
int machine_mdi(const char *text, uint32_t now)
{
    if (!text || !*text) { machine_toast("Въведете команда", now, 3000); return -1; }
    if (!machine_can_move(now)) return -1;
    if (need_homed(now)) return -1;
    if (run_text(text, NULL, RK_MDI)) { machine_toast("MDI: няма памет", now, 3000); return -1; }
    return 0;
}
