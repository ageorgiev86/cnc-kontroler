/* Управлението на машината (виж cnc/mc.h).
 *
 * Нишки:
 *   - цикълът (mc_cycle, 1 ms): командите от интерфейса, планерът → сегменти, обменът със STM32,
 *     събитията на програмата в момента на изпълнението им, реферирането, моментната снимка;
 *   - интерпретаторът (при програма и MDI): gc_run_job пише командите в опашка (CQ); ако е пълна,
 *     чака. Така и голяма програма не стои цялата в паметта.
 *
 * Сегментите: всеки такт на планера (1 ms) е един сегмент с абсолютна позиция. В STM32 се държат
 * около FILL ms напред. Изпратените се пазят (RING): по тях се праща наново изгубеното и се знае
 * редът и скоростта на изпълнявания. Събитията без спиране (охлаждане, нула, детайл) се изпълняват,
 * когато STM32 стигне сегмента им; тези със спиране (шпиндел, M0, смяна) — след като опашката свърши. */
#define _GNU_SOURCE
#include "cnc/mc.h"

#include "cnc/planner.h"

#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEG_US   1000
#define FILL     60          /* сегмента (ms) напред в STM32 */
#define RING     2048        /* изпратени сегменти */
#define CQ       4096        /* команди от интерпретатора */
#define NEV      256         /* чакащи събития без спиране */
#define NOLINK_N 100         /* цикъла без валиден отговор → няма връзка */

enum { C_RESET, C_ESTOP, C_HOME, C_JOG, C_JOG_STOP, C_MOVE, C_RUN, C_HOLD, C_STOP, C_CONT, C_OVR, C_SINGLE,
       C_OUT, C_SPINDLE, C_SETPOS, C_CFG, C_TOOL };

typedef struct {
    int k;
    int i, j;
    double d[MC_AXES], f, g;
    char *text;
    gc_config_t gcfg;
    gc_tables_t *tab;
    gc_job_t job;
    int parts[16];
    mc_cfg_t cfg;
} cmd_t;

typedef struct { uint32_t id; uint16_t flags; int64_t q[MC_AXES]; int line; float v; uint64_t sent; } sent_t;

enum { S_NONE, S_RUN, S_MOVE, S_JOG, S_HOMEAFTER };

struct mc {
    lk_port_t *port;
    mc_cfg_t cfg;
    pthread_mutex_t mu;
    pthread_t th;
    bool th_on;
    volatile bool quit;
    cmd_t q[64];
    int qh, qn;
    mc_snap_t pub;            /* публикуваната снимка (под mu) */

    /* само в цикъла */
    mc_snap_t s;
    uint64_t cyc;
    int miss;
    uint16_t seq, ack;
    bool estop_req;
    int reset_n;
    bool need_reset;          /* след свързване и след всяка грешка: само с RESET */
    bool ever_link;
    struct { uint8_t type, size; uint8_t d[24]; } ctrl[32];
    int nctrl;
    bool cfg_sent;
    double base[MC_AXES];
    bool base_set;
    uint32_t last_time_us;
    /* сегменти */
    uint32_t seg_id, send_next;
    sent_t ring[RING];
    int64_t last_q[MC_AXES];
    bool q_valid;
    uint32_t mcu_exec, mcu_last_rx;
    int mcu_qfree, mcu_state;
    /* сесия на движението */
    int sess;
    pl_t *pl;
    bool pl_done;
    uint32_t end_id;
    struct { uint32_t id; gc_cmd_t c; } ev[NEV];
    int nev;
    bool wait_ev;
    uint32_t wait_id;
    gc_cmd_t wait_c;
    uint64_t wait_until;
    bool jog_cont;
    double fov, rov;
    bool single;
    bool hold;
    /* интерпретатор */
    pthread_t ith;
    bool ith_on;
    pthread_mutex_t qmu;
    pthread_cond_t qcv;
    gc_cmd_t cq[CQ];
    int ch, cn;
    volatile bool idone;
    char ierr[200];
    int ierr_line;
    gc_cmd_t pend;
    bool have_pend;
    gc_interp_t *in;
    char *text;
    gc_tables_t *tab;
    gc_job_t job;
    int parts[16];
    /* реферирането */
    unsigned home_left;
    int home_axis;
    uint32_t home_req;
    unsigned home_mask;
    /* изходи и шпиндел */
    uint32_t out_val;
    int sp_dir;
    double sp_rpm;
    uint64_t io_t;
    bool io_dirty;
    /* брояч на пакетите */
    int pkts;
    uint64_t pkt_t0;
};

static uint64_t now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u;
}

/* ---------- команди от интерфейса ---------- */
static void push_cmd(mc_t *m, const cmd_t *c)
{
    pthread_mutex_lock(&m->mu);
    if (m->qn < 64) m->q[(m->qh + m->qn++) % 64] = *c;
    else free(c->text), free(c->tab);
    pthread_mutex_unlock(&m->mu);
}

static void simple(mc_t *m, int k, int i, double f, double g)
{
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = k;
    c.i = i;
    c.f = f;
    c.g = g;
    push_cmd(m, &c);
}

void mc_reset(mc_t *m) { simple(m, C_RESET, 0, 0, 0); }
void mc_estop(mc_t *m) { simple(m, C_ESTOP, 0, 0, 0); }
int mc_home(mc_t *m, unsigned mask) { simple(m, C_HOME, (int)mask, 0, 0); return 0; }
void mc_jog_stop(mc_t *m) { simple(m, C_JOG_STOP, 0, 0, 0); }
void mc_hold(mc_t *m, bool on) { simple(m, C_HOLD, on, 0, 0); }
void mc_stop(mc_t *m) { simple(m, C_STOP, 0, 0, 0); }
void mc_continue(mc_t *m) { simple(m, C_CONT, 0, 0, 0); }
void mc_override(mc_t *m, double feed, double rapid) { simple(m, C_OVR, 0, feed, rapid); }
void mc_single(mc_t *m, bool on) { simple(m, C_SINGLE, on, 0, 0); }
void mc_spindle(mc_t *m, int dir, double rpm) { simple(m, C_SPINDLE, dir, rpm, 0); }
void mc_set_pos(mc_t *m, int ax, double mach) { simple(m, C_SETPOS, ax, mach, 0); }
void mc_set_tool(mc_t *m, int tool) { simple(m, C_TOOL, tool, 0, 0); }

void mc_outputs(mc_t *m, uint32_t value, uint32_t mask)
{
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = C_OUT;
    c.i = (int)value;
    c.j = (int)mask;
    push_cmd(m, &c);
}

int mc_jog(mc_t *m, int ax, int dir, double dist, double feed)
{
    if (ax < 0 || ax >= MC_AXES || !dir) return -1;
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = C_JOG;
    c.i = ax;
    c.j = dir;
    c.f = feed;
    c.g = dist;
    push_cmd(m, &c);
    return 0;
}

int mc_move(mc_t *m, const double tgt[MC_AXES], double feed)
{
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = C_MOVE;
    memcpy(c.d, tgt, sizeof c.d);
    c.f = feed;
    push_cmd(m, &c);
    return 0;
}

int mc_run(mc_t *m, const char *text, const gc_config_t *gcfg, const gc_tables_t *tab, const gc_job_t *job)
{
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = C_RUN;
    c.text = strdup(text);
    c.tab = malloc(sizeof *c.tab);
    if (!c.text || !c.tab) { free(c.text); free(c.tab); return -1; }
    if (tab) *c.tab = *tab; else gc_tables_init(c.tab);
    c.gcfg = *gcfg;
    if (job) {
        c.job = *job;
        int n = job->nparts > 16 ? 16 : job->nparts;
        if (n > 0 && job->parts) memcpy(c.parts, job->parts, sizeof(int) * (size_t)n);
        c.job.nparts = n;
        c.i = 1;
    }
    push_cmd(m, &c);
    return 0;
}

void mc_set_cfg(mc_t *m, const mc_cfg_t *cfg)
{
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = C_CFG;
    c.cfg = *cfg;
    push_cmd(m, &c);
}

void mc_snapshot(mc_t *m, mc_snap_t *s)
{
    pthread_mutex_lock(&m->mu);
    *s = m->pub;
    pthread_mutex_unlock(&m->mu);
}

/* ---------- съобщения ---------- */
static void msg(mc_t *m, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void msg(mc_t *m, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m->s.msg, sizeof m->s.msg, fmt, ap);
    va_end(ap);
    m->s.msg_seq++;
}

static void alarm_(mc_t *m, mc_state_t st, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/* ---------- контролни записи ---------- */
static void ctrl(mc_t *m, uint8_t type, const uint8_t *d, uint8_t size)
{
    if (m->nctrl >= 32) return;
    m->ctrl[m->nctrl].type = type;
    m->ctrl[m->nctrl].size = size;
    memcpy(m->ctrl[m->nctrl].d, d, size);
    m->nctrl++;
}

static void send_axis_cfg(mc_t *m)
{
    for (int a = 0; a < MC_AXES; a++) {
        const mc_axis_t *x = &m->cfg.ax[a];
        uint8_t d[8];
        lk_axis_cfg(d, a, LK_CFG_MAX_ACC, (int32_t)fmin(2e9, x->amax * x->spm));
        ctrl(m, LK_T_AXIS_CONFIG, d, 8);
        lk_axis_cfg(d, a, LK_CFG_INVERT, (x->step_inv ? 1 : 0) | (x->dir_inv ? 2 : 0));
        ctrl(m, LK_T_AXIS_CONFIG, d, 8);
    }
}

static void send_ctrl(mc_t *m, int cmd)
{
    uint8_t d[4];
    lk_ctrl(d, cmd);
    ctrl(m, LK_T_MOTION_CTRL, d, 4);
}

/* ---------- позиция ---------- */
static double q2mm(const mc_t *m, int a, int64_t q) { return m->base[a] + (double)q / 65536.0 / m->cfg.ax[a].spm; }
static int64_t mm2q(const mc_t *m, int a, double mm) { return llround((mm - m->base[a]) * m->cfg.ax[a].spm * 65536.0); }

/* ---------- интерпретаторът (отделна нишка) ---------- */
static void on_gc(void *ctx, const gc_cmd_t *c)
{
    mc_t *m = ctx;
    pthread_mutex_lock(&m->qmu);
    while (m->cn == CQ && !m->in->cancel) pthread_cond_wait(&m->qcv, &m->qmu);
    if (!m->in->cancel) m->cq[(m->ch + m->cn++) % CQ] = *c;
    pthread_mutex_unlock(&m->qmu);
}

static void *interp_main(void *arg)
{
    mc_t *m = arg;
    gc_err_t e;
    int r = gc_run_job(m->in, m->text, m->job.nparts || m->job.start_line >= 0 ? &m->job : NULL, &e);
    pthread_mutex_lock(&m->qmu);
    if (r && !m->in->cancel) { snprintf(m->ierr, sizeof m->ierr, "%s", e.msg); m->ierr_line = e.line; }
    m->idone = true;
    pthread_mutex_unlock(&m->qmu);
    return NULL;
}

static void interp_stop(mc_t *m)
{
    if (!m->ith_on) return;
    pthread_mutex_lock(&m->qmu);
    m->in->cancel = true;
    pthread_cond_broadcast(&m->qcv);
    pthread_mutex_unlock(&m->qmu);
    pthread_join(m->ith, NULL);
    m->ith_on = false;
    free(m->in); m->in = NULL;
    free(m->text); m->text = NULL;
    free(m->tab); m->tab = NULL;
    m->cn = 0;
    m->have_pend = false;
}

/* ---------- сесия на движението ---------- */
static void sess_end(mc_t *m)
{
    interp_stop(m);
    if (m->pl) pl_destroy(m->pl);
    m->pl = NULL;
    m->sess = S_NONE;
    m->nev = 0;
    m->wait_ev = false;
    m->hold = false;
    m->jog_cont = false;
    m->s.wait_kind = MC_WAIT_NONE;
}

/* всичко спира; позицията после се взима от STM32 */
static void drop_all(mc_t *m)
{
    sess_end(m);
    m->send_next = m->seg_id + 1;   /* неизпратените не се пращат */
    m->q_valid = false;
    m->home_left = 0;
}

static void alarm_(mc_t *m, mc_state_t st, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m->s.alarm, sizeof m->s.alarm, fmt, ap);
    va_end(ap);
    if (m->sess || m->home_left) send_ctrl(m, LK_CTRL_ABORT);
    drop_all(m);
    m->s.state = st;
    m->need_reset = true;
}

/* нов планер от сегашната позиция */
static bool sess_begin(mc_t *m, int kind)
{
    if (m->sess != S_NONE || !m->q_valid) return false;
    pl_config_t pc;
    memset(&pc, 0, sizeof pc);
    double start[GC_NAX];
    for (int a = 0; a < MC_AXES; a++) {
        const mc_axis_t *x = &m->cfg.ax[a];
        pc.vmax[a] = x->vmax > 0 ? x->vmax : 100;
        pc.amax[a] = x->amax > 0 ? x->amax : 500;
        pc.jmax[a] = x->jmax > 0 ? x->jmax : pc.amax[a] * 50;
        start[a] = q2mm(m, a, m->last_q[a]);
    }
    pc.tol = m->cfg.tol > 0 ? m->cfg.tol : 0.02;
    pc.arc_tol = m->cfg.arc_tol > 0 ? m->cfg.arc_tol : 0.002;
    pc.dt = SEG_US / 1e6;
    m->pl = pl_create(&pc, start);
    if (!m->pl) return false;
    pl_override(m->pl, m->fov, m->rov);
    pl_single(m->pl, m->single);
    m->sess = kind;
    m->pl_done = false;
    m->nev = 0;
    m->wait_ev = false;
    m->hold = false;
    m->s.prog_done = false;
    return true;
}

/* преди нова сесия: позицията на STM32 става началото */
static void sync_pos(mc_t *m, const lk_rx_t *r)
{
    for (int a = 0; a < MC_AXES; a++) {
        int64_t q = (int64_t)r->cmd[a] * 65536;
        if (!m->q_valid || llabs((m->last_q[a] >> 16) - r->cmd[a]) > 1) m->last_q[a] = q;
    }
    m->q_valid = true;
}

static void push_line(mc_t *m, const double *tgt, bool rapid, double feed)
{
    gc_cmd_t c;
    memset(&c, 0, sizeof c);
    c.kind = rapid ? GC_RAPID : GC_FEED;
    memcpy(c.tgt, tgt, sizeof c.tgt);
    c.feed = feed;
    c.line = -1;
    pl_push(m->pl, &c);
}

static double clamp_soft(const mc_t *m, int a, double v)
{
    if (!m->s.homed[a] || m->cfg.ax[a].rotary) return v;
    const mc_axis_t *x = &m->cfg.ax[a];
    return v < x->smin ? x->smin : v > x->smax ? x->smax : v;
}

/* ---------- събитията на програмата ---------- */
static void set_io(mc_t *m) { m->io_dirty = true; }

static void cool_out(mc_t *m, int cool)
{
    uint32_t v = m->out_val;
    int bits[3] = {m->cfg.out_flood, m->cfg.out_mist, m->cfg.out_air};
    int fl[3] = {GC_COOL_FLOOD | GC_COOL_TSC, GC_COOL_MIST, GC_COOL_AIR};
    for (int k = 0; k < 3; k++)
        if (bits[k] >= 0 && bits[k] < 30) { if (cool & fl[k]) v |= 1u << bits[k]; else v &= ~(1u << bits[k]); }
    m->out_val = v;
    m->s.cool = cool;
    set_io(m);
}

static void spindle_out(mc_t *m, int dir, double rpm)
{
    m->sp_dir = dir;
    m->sp_rpm = rpm;
    m->s.sp_dir = dir;
    m->s.sp_rpm = rpm;
    uint8_t d[12];
    lk_spindle(d, dir > 0 ? 1 : dir < 0 ? 2 : 0);
    ctrl(m, LK_T_SPINDLE, d, 12);
}

/* събитие без спиране, когато STM32 стигне сегмента му */
static void do_event(mc_t *m, const gc_cmd_t *c)
{
    switch (c->kind) {
    case GC_COOLANT: cool_out(m, c->ival); break;
    case GC_WCS_SELECT: m->s.wcs = c->ival; break;
    case GC_PART: m->s.part = c->ival; break;
    case GC_TOOL_CHANGE: m->s.tool = c->ival; break;
    default: break;
    }
}

/* събитие със спиране: опашката в STM32 е празна; true = продължава веднага */
static bool do_stop_event(mc_t *m, const gc_cmd_t *c, uint64_t t)
{
    switch (c->kind) {
    case GC_SPINDLE:
        spindle_out(m, c->ival, c->val);
        m->wait_until = t + (c->ival ? (uint64_t)(m->cfg.sp_delay * 1e6) : 0);
        return false;
    case GC_TOOL_CHANGE:   /* ръчна смяна: шпинделът спира, чака Старт */
        if (c->ival == m->s.tool) return true;
        spindle_out(m, 0, 0);
        m->s.wait_kind = MC_WAIT_TOOL;
        m->s.wait_tool = c->ival;
        m->s.state = MC_WAIT;
        msg(m, "Сложете T%d и натиснете Старт", c->ival);
        return false;
    case GC_PAUSE:
        if (c->ival == 1 && !m->cfg.opt_stop) return true;
        m->s.wait_kind = MC_WAIT_PAUSE;
        m->s.state = MC_WAIT;
        msg(m, "%s: натиснете Старт", c->ival ? "M1" : "M0");
        return false;
    case GC_HOME:
        msg(m, "G28.1 в програмата се пропуска: реферирайте от бутона HOME");
        return true;
    default:
        return true;
    }
}

/* ---------- сегменти ---------- */
static void emit_seg(mc_t *m, const pl_sample_t *s)
{
    sent_t *e = &m->ring[(++m->seg_id) % RING];
    e->id = m->seg_id;
    e->flags = s->v <= 1e-9 ? LK_SEG_END : 0;
    for (int a = 0; a < MC_AXES; a++) e->q[a] = mm2q(m, a, s->pos[a]);
    e->line = s->line;
    e->v = (float)s->v;
    e->sent = 0;
    memcpy(m->last_q, e->q, sizeof m->last_q);
}

/* меки лимити: точката е извън тях (след реферирането) */
static int soft_violation(const mc_t *m, const double *pos)
{
    for (int a = 0; a < MC_AXES; a++) {
        const mc_axis_t *x = &m->cfg.ax[a];
        if (!x->active || !m->s.homed[a] || x->rotary) continue;
        if (pos[a] < x->smin - 1e-3 || pos[a] > x->smax + 1e-3) return a;
    }
    return -1;
}

static const char *const AXN = "XYZABC";

/* интерпретаторът → планерът */
static void feed_planner(mc_t *m)
{
    if (!m->ith_on && !m->have_pend) return;
    for (;;) {
        if (!m->have_pend) {
            pthread_mutex_lock(&m->qmu);
            if (m->cn) {
                m->pend = m->cq[m->ch];
                m->ch = (m->ch + 1) % CQ;
                m->cn--;
                m->have_pend = true;
                pthread_cond_signal(&m->qcv);
            }
            bool done = m->idone && !m->cn;
            pthread_mutex_unlock(&m->qmu);
            if (!m->have_pend) {
                if (done) pl_input_end(m->pl);
                return;
            }
        }
        if (pl_push(m->pl, &m->pend)) return;   /* планерът е пълен */
        m->have_pend = false;
    }
}

static void generate(mc_t *m, uint64_t t)
{
    if (!m->pl) return;
    if (m->sess == S_RUN) feed_planner(m);
    while (!m->wait_ev && !m->pl_done && (int)(m->seg_id - m->mcu_exec) < FILL) {
        pl_sample_t s;
        int rc = pl_tick(m->pl, &s);
        if (rc == PL_SAMPLE) {
            int a = soft_violation(m, s.pos);
            if (a >= 0) {
                alarm_(m, MC_FAULT, "Мек лимит по %c (%.3f): движението е спряно", AXN[a], s.pos[a]);
                return;
            }
            emit_seg(m, &s);
            for (int k = 0; k < s.nev; k++) {
                if (m->nev < NEV) { m->ev[m->nev].id = m->seg_id; m->ev[m->nev].c = s.evs[k]; m->nev++; }
            }
            if (m->hold && s.v <= 1e-9 && m->sess == S_JOG) { m->pl_done = true; m->end_id = m->seg_id; }
            continue;
        }
        if (rc == PL_WAIT) {
            m->wait_ev = true;
            m->wait_id = m->seg_id;
            m->wait_c = s.ev;
            m->wait_until = 0;
            break;
        }
        if (rc == PL_DONE || (rc == PL_STARVED && m->sess != S_RUN)) {
            if (rc == PL_STARVED) pl_input_end(m->pl);
            m->pl_done = true;
            m->end_id = m->seg_id;
            break;
        }
        break;   /* PL_STARVED: интерпретаторът не е докарал още */
    }
    (void)t;
}

/* ---------- пакет ---------- */
static void build_packet(mc_t *m, lk_tx_t *tx)
{
    lk_begin(tx);
    int k = 0;
    for (; k < m->nctrl; k++)
        if (lk_add(tx, m->ctrl[k].type, m->ctrl[k].d, m->ctrl[k].size)) break;
    memmove(m->ctrl, m->ctrl + k, sizeof m->ctrl[0] * (size_t)(m->nctrl - k));
    m->nctrl -= k;
    /* сегменти: колкото има място в пакета и в STM32 */
    int room = m->mcu_qfree - (int)(m->send_next - 1 - m->mcu_last_rx);
    while (m->send_next <= m->seg_id && room > 4 && lk_room(tx) >= 2 + LK_SEG_SIZE) {
        sent_t *e = &m->ring[m->send_next % RING];
        lk_seg_t sg;
        lk_seg(&sg, e->id, SEG_US, e->flags, e->q);
        lk_add(tx, LK_T_SEGMENT, sg.d, LK_SEG_SIZE);
        e->sent = m->cyc;
        m->send_next++;
        room--;
    }
    uint8_t flags = 0;
    if (m->estop_req) flags |= LK_F_ESTOP_REQ;
    if (m->reset_n > 0) { flags |= LK_F_RESET_REQ; m->reset_n--; }
    lk_finish(tx, flags, ++m->seq, m->ack);
}

/* ---------- реферирането ---------- */
static void home_next(mc_t *m)
{
    int best = -1;
    for (int a = 0; a < MC_AXES; a++)
        if (m->home_left >> a & 1 && (best < 0 || m->cfg.ax[a].home_seq < m->cfg.ax[best].home_seq)) best = a;
    if (best < 0) return;
    m->home_left &= ~(1u << best);
    const mc_axis_t *x = &m->cfg.ax[best];
    if (!x->home_sw) {   /* без датчик: тази позиция е машинната нула (ръчно реферирана) */
        m->home_axis = -1;
        return;
    }
    m->home_axis = best;
    m->home_req++;
    double travel = x->rotary ? 720 : fabs(x->smax - x->smin) * 1.5 + 10;
    uint8_t d[24];
    lk_home_start(d, m->home_req, best, x->home_dir, best, (uint32_t)fmax(1, x->home_search * x->spm),
                  (uint32_t)fmax(1, x->home_latch * x->spm), (uint32_t)(travel * x->spm), (uint32_t)fmax(1, x->home_pull * x->spm));
    ctrl(m, LK_T_HOME_START, d, 24);
    msg(m, "Home %c: търсене на датчика", AXN[best]);
}

static void home_tick(mc_t *m, const lk_rx_t *r)
{
    if (m->home_axis >= 0) {
        if (!r->home_valid || r->home_req != m->home_req) return;
        int a = m->home_axis;
        if (r->home_result != 0) {
            alarm_(m, MC_FAULT, "Home %c: %s. Проверете датчика и натиснете RESET.", AXN[a],
                   r->home_result == 1 ? "датчикът не сработи в рамките на хода" : "реферирането е прекъснато");
            return;
        }
        m->base[a] = m->cfg.ax[a].home_sw_pos - (double)r->home_pos / m->cfg.ax[a].spm;
        m->s.homed[a] = true;
        m->home_axis = -1;
        m->q_valid = false;
    }
    if (m->mcu_state != LK_ST_READY) return;
    if (m->home_left) { home_next(m); return; }
    /* всички: към позициите след Home */
    sync_pos(m, r);
    if (!sess_begin(m, S_HOMEAFTER)) { m->s.state = MC_IDLE; return; }
    double tgt[GC_NAX];
    for (int a = 0; a < MC_AXES; a++) {
        tgt[a] = q2mm(m, a, m->last_q[a]);
        if (m->home_mask >> a & 1 && m->s.homed[a]) tgt[a] = m->cfg.ax[a].home_after;
    }
    push_line(m, tgt, true, 0);
    pl_input_end(m->pl);
    m->s.state = MC_MOVE;
}

/* ---------- командите ---------- */
static bool can_move(mc_t *m)
{
    if (m->s.state == MC_IDLE) return true;
    if (m->s.state == MC_NOLINK) msg(m, "Няма връзка със STM32");
    else if (m->s.state == MC_FAULT || m->s.state == MC_ESTOP) msg(m, "Натиснете RESET");
    else msg(m, "Машината е заета");
    return false;
}

static void handle(mc_t *m, cmd_t *c, const lk_rx_t *r)
{
    switch (c->k) {
    case C_RESET:
        m->need_reset = false;
        m->estop_req = false;
        m->reset_n = 5;
        m->cfg_sent = false;
        break;
    case C_ESTOP:
        m->estop_req = true;
        alarm_(m, MC_ESTOP, "Авариен стоп (от интерфейса)");
        break;
    case C_CFG:
        m->cfg = c->cfg;
        m->cfg_sent = false;
        break;
    case C_OVR:
        m->fov = c->f;
        m->rov = c->g;
        if (m->pl) pl_override(m->pl, m->fov, m->rov);
        break;
    case C_SINGLE:
        m->single = c->i;
        if (m->pl) pl_single(m->pl, m->single);
        break;
    case C_OUT:
        m->out_val = (m->out_val & ~(uint32_t)c->j) | ((uint32_t)c->i & (uint32_t)c->j);
        set_io(m);
        break;
    case C_SPINDLE:
        spindle_out(m, c->i, c->f);
        break;
    case C_TOOL:
        m->s.tool = c->i;
        break;
    case C_SETPOS:
        if (c->i >= 0 && c->i < MC_AXES && m->s.state == MC_IDLE && r->ok) {
            m->base[c->i] = c->f - (double)r->cmd[c->i] / m->cfg.ax[c->i].spm;
            m->s.homed[c->i] = true;
            m->q_valid = false;
        }
        break;
    case C_HOME:
        if (!can_move(m)) break;
        m->home_mask = (unsigned)c->i;
        m->home_left = 0;
        for (int a = 0; a < MC_AXES; a++)
            if (c->i >> a & 1 && m->cfg.ax[a].active) { m->home_left |= 1u << a; m->s.homed[a] = false; }
        m->home_axis = -1;
        m->s.state = MC_HOMING;
        break;
    case C_JOG: {
        if (!can_move(m)) break;
        sync_pos(m, r);
        if (!sess_begin(m, S_JOG)) break;
        double tgt[GC_NAX];
        for (int a = 0; a < MC_AXES; a++) tgt[a] = q2mm(m, a, m->last_q[a]);
        int a = c->i;
        double far = c->g > 0 ? c->g : (m->s.homed[a] ? 1e6 : 2000);
        tgt[a] = clamp_soft(m, a, tgt[a] + c->j * far);
        m->jog_cont = c->g <= 0;
        push_line(m, tgt, false, c->f);
        pl_input_end(m->pl);
        m->s.state = MC_MOVE;
        break;
    }
    case C_JOG_STOP:
        if (m->sess == S_JOG && m->jog_cont) { m->hold = true; pl_hold(m->pl, true); }
        break;
    case C_MOVE: {
        if (!can_move(m)) break;
        sync_pos(m, r);
        if (!sess_begin(m, S_MOVE)) break;
        double tgt[GC_NAX];
        for (int a = 0; a < MC_AXES; a++) tgt[a] = isnan(c->d[a]) ? q2mm(m, a, m->last_q[a]) : clamp_soft(m, a, c->d[a]);
        push_line(m, tgt, c->f <= 0, c->f);
        pl_input_end(m->pl);
        m->s.state = MC_MOVE;
        break;
    }
    case C_RUN: {
        if (!can_move(m)) break;
        sync_pos(m, r);
        if (!sess_begin(m, S_RUN)) break;
        m->in = malloc(sizeof *m->in);
        if (!m->in) { sess_end(m); break; }
        m->text = c->text;
        m->tab = c->tab;
        c->text = NULL;
        c->tab = NULL;
        m->job = c->job;
        memcpy(m->parts, c->parts, sizeof m->parts);
        m->job.parts = m->parts;
        if (!c->i) { memset(&m->job, 0, sizeof m->job); m->job.start_line = -1; }
        gc_config_t g = c->gcfg;
        g.emit = on_gc;
        g.ctx = m;
        double start[GC_NAX];
        for (int a = 0; a < MC_AXES; a++) start[a] = q2mm(m, a, m->last_q[a]);
        gc_interp_init(m->in, &g, m->tab, start);
        m->in->wcs = m->s.wcs >= 1 ? m->s.wcs : 1;
        m->ch = m->cn = 0;
        m->idone = false;
        m->ierr[0] = 0;
        m->ierr_line = -1;
        if (pthread_create(&m->ith, NULL, interp_main, m) != 0) { sess_end(m); break; }
        m->ith_on = true;
        m->s.state = MC_RUN;
        break;
    }
    case C_HOLD:
        if (m->sess == S_RUN && m->pl) {
            m->hold = c->i;
            pl_hold(m->pl, m->hold);
            m->s.state = m->hold ? MC_HOLD : MC_RUN;
        }
        break;
    case C_STOP:
        if (m->sess || m->s.state == MC_HOMING || m->s.state == MC_WAIT) {
            send_ctrl(m, LK_CTRL_ABORT);
            drop_all(m);
            m->s.state = MC_STOPPING;
            spindle_out(m, 0, 0);
            cool_out(m, 0);
        }
        break;
    case C_CONT:
        if (m->s.state == MC_WAIT) {
            m->s.wait_kind = MC_WAIT_NONE;
            if (m->wait_c.kind == GC_TOOL_CHANGE) m->s.tool = m->wait_c.ival;
            m->wait_ev = false;
            pl_continue(m->pl);
            m->s.state = MC_RUN;
        } else if (m->s.state == MC_HOLD) {
            m->hold = false;
            pl_hold(m->pl, false);
            m->s.state = MC_RUN;
        }
        break;
    }
    free(c->text);
    free(c->tab);
}

/* ---------- един цикъл ---------- */
void mc_cycle(mc_t *m)
{
    uint64_t t = now_us();
    m->cyc++;
    /* командите */
    cmd_t cmds[64];
    int nc = 0;
    pthread_mutex_lock(&m->mu);
    while (m->qn) { cmds[nc++] = m->q[m->qh]; m->qh = (m->qh + 1) % 64; m->qn--; }
    pthread_mutex_unlock(&m->mu);

    /* обменът (командите от предишния цикъл и сегментите) */
    if (!m->cfg_sent && m->s.link) { send_axis_cfg(m); m->cfg_sent = true; }
    if (m->io_dirty || t - m->io_t > 500000) {   /* изходите: при промяна и на 0,5 s (пълното състояние) */
        uint8_t d[12];
        lk_io(d, m->out_val, 0x3FFFFFFFu);
        ctrl(m, LK_T_IO_SET, d, 12);
        if (!m->io_dirty) { lk_spindle(d, m->sp_dir > 0 ? 1 : m->sp_dir < 0 ? 2 : 0); ctrl(m, LK_T_SPINDLE, d, 12); }
        m->io_dirty = false;
        m->io_t = t;
    }
    /* повторно изпращане: STM32 не е приел сегмент, пратен преди 3 цикъла */
    if (m->mcu_last_rx + 1 < m->send_next && m->mcu_last_rx + 1 <= m->seg_id) {
        sent_t *e = &m->ring[(m->mcu_last_rx + 1) % RING];
        if (e->id == m->mcu_last_rx + 1 && e->sent && m->cyc - e->sent > 3) m->send_next = e->id;
    }
    lk_tx_t tx;
    build_packet(m, &tx);
    uint8_t rb[LK_PKT];
    lk_rx_t r;
    bool ok = m->port->xfer(m->port, tx.b, rb) == 0 && lk_parse(rb, &r) == 0;
    if (!ok) {
        memset(&r, 0, sizeof r);
        m->s.bad_rx++;
        if (++m->miss >= NOLINK_N && m->s.state != MC_NOLINK) {
            alarm_(m, MC_NOLINK, "Няма връзка със STM32 (%s)", m->port->name);
            m->s.link = false;
        }
    } else {
        m->miss = 0;
        m->ack = r.seq;
        m->pkts++;
        /* STM32 е рестартиран (номерата на сегментите почват отначало): позицията не е известна */
        if (m->s.link && r.last_rx_id + 1000u < m->mcu_last_rx) {
            for (int a = 0; a < MC_AXES; a++) m->s.homed[a] = false;
            m->base_set = false;
            m->seg_id = r.last_rx_id;
            m->send_next = m->seg_id + 1;
            alarm_(m, MC_FAULT, "STM32 е рестартиран: натиснете RESET и реферирайте");
        }
        m->last_time_us = r.time_us;
        if (!m->s.link) {
            m->s.link = true;
            m->cfg_sent = false;
            m->need_reset = true;
            if (m->s.state == MC_NOLINK) m->s.state = MC_FAULT;
            snprintf(m->s.alarm, sizeof m->s.alarm, "%s със STM32 (%s): натиснете RESET", m->ever_link ? "Връзката е възстановена" : "Свързано", m->port->name);
            m->ever_link = true;
            m->seg_id = r.last_rx_id > m->seg_id ? r.last_rx_id : m->seg_id;
            m->send_next = m->seg_id + 1;
            m->mcu_exec = r.exec_id;
        }
        if (!m->base_set) {
            for (int a = 0; a < MC_AXES; a++) m->base[a] = -(double)r.cmd[a] / m->cfg.ax[a].spm;   /* машинната 0 е тук */
            m->base_set = true;
        }
        m->mcu_exec = r.exec_id;
        m->mcu_last_rx = r.last_rx_id;
        m->mcu_qfree = r.queue_free;
        m->mcu_state = r.state;
        m->s.mcu_state = r.state;
        m->s.local_in = r.local_in;
        m->s.outputs = r.outputs;
        m->s.inputs = r.inputs;
        m->s.pendant = r.pendant;
        m->s.mpg = r.mpg;
        m->s.rx_errors = r.rx_errors;
        memcpy(m->s.ain_mv, r.ain_mv, sizeof r.ain_mv);
        for (int a = 0; a < MC_AXES; a++) m->s.mach[a] = m->base[a] + (double)r.cmd[a] / m->cfg.ax[a].spm;

        /* грешка в STM32 */
        if (r.state == LK_ST_FAULT || r.state == LK_ST_ESTOP) {
            m->s.fault = r.fault;
            if (m->s.state != MC_FAULT && m->s.state != MC_ESTOP && m->s.state != MC_NOLINK)
                alarm_(m, r.state == LK_ST_ESTOP ? MC_ESTOP : MC_FAULT, "STM32 спря: %s. Натиснете RESET.", lk_fault_text(r.fault));
            else if (r.state == LK_ST_ESTOP && m->s.state == MC_FAULT) m->s.state = MC_ESTOP;
        } else {
            m->s.fault = 0;
            if ((m->s.state == MC_FAULT || m->s.state == MC_ESTOP) && !m->need_reset && !m->estop_req && m->reset_n == 0 && r.state == LK_ST_READY) {
                m->s.state = MC_STOPPING;
                m->s.alarm[0] = 0;
            }
        }
    }

    for (int k = 0; k < nc; k++) handle(m, &cmds[k], &r);

    if (ok) {
        /* събитията без спиране на изпълнения сегмент */
        int w = 0;
        for (int k = 0; k < m->nev; k++) {
            if ((int32_t)(m->mcu_exec - m->ev[k].id) >= 0) do_event(m, &m->ev[k].c);
            else m->ev[w++] = m->ev[k];
        }
        m->nev = w;
        /* събитие със спиране: след като STM32 изпълни всичко до него */
        if (m->wait_ev && m->s.state != MC_WAIT && m->mcu_exec == m->wait_id && m->mcu_qfree == 256 && m->send_next > m->wait_id) {
            if (!m->wait_until) {
                if (do_stop_event(m, &m->wait_c, t)) { m->wait_ev = false; pl_continue(m->pl); }
            } else if (t >= m->wait_until) {
                m->wait_until = 0;
                m->wait_ev = false;
                pl_continue(m->pl);
            }
        }
        /* грешка на интерпретатора: спиране, щом планерът стигне до нея */
        if (m->sess == S_RUN && m->ierr[0] && m->pl_done) {
            msg(m, "Ред %d: %.170s", m->ierr_line + 1, m->ierr);
            snprintf(m->s.alarm, sizeof m->s.alarm, "Ред %d: %.170s", m->ierr_line + 1, m->ierr);
        }
        /* краят на сесията */
        if (m->sess && m->pl_done && m->mcu_exec == m->end_id && m->send_next > m->end_id && m->mcu_qfree == 256) {
            int kind = m->sess;
            bool err = kind == S_RUN && m->ierr[0];
            sess_end(m);
            m->s.done_seq++;
            if (kind == S_RUN) {
                m->s.prog_done = !err;
                if (err) { m->s.state = MC_FAULT; }
                else { msg(m, "Програмата завърши"); m->s.state = MC_IDLE; }
            } else m->s.state = MC_IDLE;
        }
        if (m->s.state == MC_HOMING) home_tick(m, &r);
        if (m->s.state == MC_STOPPING && r.state == LK_ST_READY && r.queue_free == 256) {
            m->seg_id = m->seg_id > r.last_rx_id ? m->seg_id : r.last_rx_id;
            m->send_next = m->seg_id + 1;
            m->mcu_exec = m->seg_id;
            m->q_valid = false;
            sync_pos(m, &r);
            m->s.state = MC_IDLE;
        }
        if (m->s.state == MC_IDLE && !m->sess) sync_pos(m, &r);
        generate(m, t);
    }

    /* редът и скоростта на изпълнявания сегмент */
    const sent_t *e = &m->ring[m->mcu_exec % RING];
    if (m->sess && e->id == m->mcu_exec && m->mcu_exec) { m->s.line = e->line; m->s.v = e->v; }
    else { m->s.v = 0; if (!m->sess) m->s.line = -1; }
    m->s.queue = (int)(m->seg_id - m->mcu_exec);
    if (t - m->pkt_t0 >= 1000000) { m->s.pkt_rate = m->pkts * 1e6 / (double)(t - m->pkt_t0); m->pkts = 0; m->pkt_t0 = t; }

    pthread_mutex_lock(&m->mu);
    m->pub = m->s;
    pthread_mutex_unlock(&m->mu);
}

/* ---------- нишката ---------- */
static void *loop(void *arg)
{
    mc_t *m = arg;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    while (!m->quit) {
        mc_cycle(m);
        next.tv_nsec += SEG_US * 1000;
        if (next.tv_nsec >= 1000000000) { next.tv_nsec -= 1000000000; next.tv_sec++; }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > next.tv_sec + 1) next = now;   /* голямо закъснение: без наваксване */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    return NULL;
}

mc_t *mc_new(lk_port_t *port, const mc_cfg_t *cfg)
{
    mc_t *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->port = port;
    m->cfg = *cfg;
    pthread_mutex_init(&m->mu, NULL);
    pthread_mutex_init(&m->qmu, NULL);
    pthread_cond_init(&m->qcv, NULL);
    m->fov = m->rov = 1;
    m->s.state = MC_NOLINK;
    m->s.line = -1;
    m->s.wcs = 1;
    m->home_axis = -1;
    m->home_req = (uint32_t)now_us();
    m->mcu_qfree = 256;
    m->send_next = 1;
    snprintf(m->s.port, sizeof m->s.port, "%s", port->name);
    snprintf(m->s.alarm, sizeof m->s.alarm, "Свързване със STM32 (%s)…", port->name);
    m->pub = m->s;
    return m;
}

int mc_start(mc_t *m)
{
    pthread_attr_t at;
    pthread_attr_init(&at);
    struct sched_param sp = {.sched_priority = 50};
    /* реално време, ако е разрешено (иначе обикновена нишка) */
    if (pthread_attr_setinheritsched(&at, PTHREAD_EXPLICIT_SCHED) == 0 && pthread_attr_setschedpolicy(&at, SCHED_FIFO) == 0 &&
        pthread_attr_setschedparam(&at, &sp) == 0 && pthread_create(&m->th, &at, loop, m) == 0) {
        m->th_on = true;
    } else if (pthread_create(&m->th, NULL, loop, m) == 0) {
        m->th_on = true;
    }
    pthread_attr_destroy(&at);
    return m->th_on ? 0 : -1;
}

void mc_free(mc_t *m)
{
    if (!m) return;
    m->quit = true;
    if (m->th_on) pthread_join(m->th, NULL);
    sess_end(m);
    if (m->port && m->port->close) m->port->close(m->port);
    pthread_mutex_destroy(&m->mu);
    pthread_mutex_destroy(&m->qmu);
    pthread_cond_destroy(&m->qcv);
    free(m);
}
