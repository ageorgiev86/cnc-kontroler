/* Виртуален STM32 (виж cnc/vmcu.h). Поведението следва firmware/stm32/Core/Src/link.c,
 * stepgen.c и homing.c: сегментите са абсолютни позиции, изпълняват се линейно за времето си. */
#include "cnc/vmcu.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define QN 256
#define TIMEOUT_US 30000u   /* като firmware-а (LINK_SPI_TIMEOUT_MS) */

typedef struct { uint32_t id; uint32_t us; uint16_t flags; int64_t q[LK_AXES]; } seg_t;

struct vmcu {
    seg_t q[QN];
    int head, n;
    uint32_t cur_us;           /* изминато от сегмента в главата */
    int64_t pos[LK_AXES];      /* 1/65536 стъпка */
    int64_t from[LK_AXES];     /* началото на сегмента в главата */
    uint32_t exec_id, last_rx_id;
    bool last_end;             /* последният изпълнен сегмент е END_OF_MOVE (или не е имало) */
    int fault;
    bool cfg_fault;
    uint32_t local_in, outputs;
    uint32_t rx_errors;
    uint16_t seq, rx_seq;
    uint64_t now_us, last_pkt_us;
    bool active, manual;
    struct timespec t0;
    bool t0_set;
    /* реферирането */
    bool homing;
    int h_axis, h_dir, h_phase;    /* 0 търси, 1 отдръпва, 2 докосва бавно */
    uint32_t h_req, h_search, h_latch, h_travel, h_pull;
    int64_t h_start;
    bool h_have;
    uint32_t hd_req;
    int hd_axis, hd_result;
    int32_t hd_pos;
    int32_t sw[LK_AXES];
    bool sw_set[LK_AXES];
    lk_port_t port;
    bool lag;
    uint8_t prev[LK_PKT];
    bool have_prev;
};

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int64_t get64(const uint8_t *p) { return (int64_t)((uint64_t)get32(p) | (uint64_t)get32(p + 4) << 32); }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }

vmcu_t *vmcu_new(void)
{
    vmcu_t *v = calloc(1, sizeof *v);
    if (!v) return NULL;
    v->last_end = true;
    for (int a = 0; a < LK_AXES; a++) v->pos[a] = (int64_t)(1000 + 37 * a) * 65536;   /* включване: някъде */
    return v;
}

void vmcu_free(vmcu_t *v) { free(v); }
void vmcu_manual_time(vmcu_t *v, bool on) { v->manual = on; }
void vmcu_spi_lag(vmcu_t *v, bool on) { v->lag = on; v->have_prev = false; }
void vmcu_set_inputs(vmcu_t *v, uint32_t set, uint32_t clear) { v->local_in = (v->local_in | set) & ~clear; }
void vmcu_set_home_switch(vmcu_t *v, int a, int32_t steps) { v->sw[a] = steps; v->sw_set[a] = true; }
int64_t vmcu_pos_q(const vmcu_t *v, int a) { return v->pos[a]; }
uint32_t vmcu_outputs(const vmcu_t *v) { return v->outputs; }
int vmcu_fault(const vmcu_t *v) { return v->fault; }

static void stop_all(vmcu_t *v)
{
    v->n = 0;
    v->cur_us = 0;
    v->homing = false;
    v->last_end = true;
}

static void set_fault(vmcu_t *v, int f)
{
    if (v->fault) return;
    v->fault = f;
    stop_all(v);
    v->outputs = 0;   /* при всяка грешка изходите и шпинделът се изключват */
}

/* реферирането: оста се движи с постоянна скорост (стъпки/s) */
static void home_step(vmcu_t *v, uint32_t us)
{
    int a = v->h_axis;
    int32_t steps = (int32_t)(v->pos[a] >> 16);
    if (!v->sw_set[a]) {   /* датчикът: на една трета от хода в посоката на търсенето */
        v->sw[a] = steps + v->h_dir * (int32_t)(v->h_travel / 3 + 1);
        v->sw_set[a] = true;
    }
    double rate = v->h_phase == 1 ? v->h_search : v->h_phase == 2 ? v->h_latch : v->h_search;
    double d = rate * us / 1e6;
    int dir = v->h_phase == 1 ? -v->h_dir : v->h_dir;
    v->pos[a] += (int64_t)llround(dir * d * 65536);
    steps = (int32_t)(v->pos[a] >> 16);
    bool hit = v->h_dir > 0 ? steps >= v->sw[a] : steps <= v->sw[a];
    if (hit) v->local_in |= LK_IN_HOME(a); else v->local_in &= ~LK_IN_HOME(a);
    if (v->h_phase == 0) {
        if (hit) { v->h_phase = 1; v->h_start = v->pos[a]; }
        else if (llabs((v->pos[a] - v->h_start) >> 16) > (int64_t)v->h_travel) {
            v->homing = false; v->h_have = true; v->hd_req = v->h_req; v->hd_axis = a; v->hd_result = 1; v->hd_pos = steps;
        }
    } else if (v->h_phase == 1) {
        if (llabs((v->pos[a] - v->h_start) >> 16) >= (int64_t)v->h_pull) v->h_phase = 2;
    } else if (hit) {   /* бавното докосване: тази позиция се връща */
        v->pos[a] = (int64_t)v->sw[a] * 65536;
        v->homing = false; v->h_have = true; v->hd_req = v->h_req; v->hd_axis = a; v->hd_result = 0; v->hd_pos = v->sw[a];
        v->local_in &= ~LK_IN_HOME(a);
    }
}

void vmcu_advance(vmcu_t *v, uint32_t us)
{
    v->now_us += us;
    if (v->active && v->now_us - v->last_pkt_us > TIMEOUT_US) {   /* като firmware-а: грешка само в движение */
        v->active = false;
        if (v->n || v->homing) set_fault(v, LK_FAULT_LINK);
        else v->outputs = 0;
    }
    if ((v->local_in & LK_IN_ESTOP) && v->fault != LK_FAULT_ESTOP) { v->fault = 0; set_fault(v, LK_FAULT_ESTOP); }
    if (v->fault) return;
    if (v->homing) { home_step(v, us); return; }
    while (us && v->n) {
        seg_t *s = &v->q[v->head];
        if (v->cur_us == 0) memcpy(v->from, v->pos, sizeof v->from);
        uint32_t left = s->us - v->cur_us, d = us < left ? us : left;
        v->cur_us += d;
        us -= d;
        double f = s->us ? (double)v->cur_us / s->us : 1;
        for (int a = 0; a < LK_AXES; a++) v->pos[a] = v->from[a] + (int64_t)llround((double)(s->q[a] - v->from[a]) * f);
        v->exec_id = s->id;
        if (v->cur_us >= s->us) {
            v->last_end = (s->flags & LK_SEG_END) != 0;
            v->head = (v->head + 1) % QN;
            v->n--;
            v->cur_us = 0;
        }
    }
    if (us && !v->n && !v->last_end) set_fault(v, LK_FAULT_UNDERRUN);
}

static void now_from_clock(vmcu_t *v)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    if (!v->t0_set) { v->t0 = t; v->t0_set = true; return; }
    int64_t us = (t.tv_sec - v->t0.tv_sec) * 1000000LL + (t.tv_nsec - v->t0.tv_nsec) / 1000;
    if (us <= 0) return;
    v->t0 = t;
    if (us > 200000) us = 200000;   /* спряна програма (дебъгер): не прескача много */
    vmcu_advance(v, (uint32_t)us);
}

static void process(vmcu_t *v, const uint8_t *p)
{
    unsigned len = get16(p + 8);
    uint8_t flags = p[3];
    v->rx_seq = get16(p + 4);
    v->last_pkt_us = v->now_us;
    v->active = true;
    if (flags & LK_F_ESTOP_REQ) { if (v->fault != LK_FAULT_ESTOP) { v->fault = 0; set_fault(v, LK_FAULT_ESTOP); } }
    if ((flags & LK_F_RESET_REQ) && !(v->local_in & LK_IN_ESTOP) && !(flags & LK_F_ESTOP_REQ)) { v->fault = 0; v->cfg_fault = false; }
    for (unsigned i = 0; i + 2 <= len;) {
        const uint8_t *r = p + LK_HDR + i;
        unsigned type = r[0], size = r[1];
        const uint8_t *d = r + 2;
        if (!type || i + 2 + size > len) break;
        if (type == LK_T_SEGMENT && size == LK_SEG_SIZE) {
            uint32_t id = get32(d);
            if (id && id > v->last_rx_id) {
                if (v->homing || v->fault) v->last_rx_id = id;
                else if (v->n < QN) {
                    seg_t *s = &v->q[(v->head + v->n) % QN];
                    s->id = id;
                    s->us = get16(d + 4);
                    s->flags = get16(d + 6);
                    for (int a = 0; a < LK_AXES; a++) s->q[a] = get64(d + 8 + 8 * a);
                    v->n++;
                    v->last_rx_id = id;
                }
            }
        } else if (type == LK_T_MOTION_CTRL && size == 4) {
            if (d[0] == LK_CTRL_ABORT || d[0] == LK_CTRL_HOLD) { stop_all(v); }
        } else if (type == LK_T_HOME_START && size == 24) {
            if (!v->fault && !v->n) {
                v->homing = true;
                v->h_have = false;
                v->h_req = get32(d);
                v->h_axis = d[4] < LK_AXES ? d[4] : 0;
                v->h_dir = (int8_t)d[5] > 0 ? 1 : -1;
                v->h_search = get32(d + 8);
                v->h_latch = get32(d + 12);
                v->h_travel = get32(d + 16);
                v->h_pull = get32(d + 20);
                v->h_phase = 0;
                v->h_start = v->pos[v->h_axis];
            } else v->cfg_fault = true;
        } else if (type == LK_T_IO_SET && size == 12) {
            uint32_t val = get32(d), mask = get32(d + 4) & 0x3FFFFFFFu;
            if (!v->fault) v->outputs = (v->outputs & ~mask) | (val & mask);
        } else if (type == LK_T_SPINDLE && size == 12) {
            if (!v->fault) v->outputs = (v->outputs & 0x3FFFFFFFu) | (d[0] == 1 ? 1u << 30 : d[0] == 2 ? 1u << 31 : 0);
        }
        i += 2 + size;
    }
}

static void reply(vmcu_t *v, uint8_t *t)
{
    memset(t, 0, LK_PKT);
    uint8_t *b = t + LK_HDR, *s = b + 2;
    int fault = v->fault ? v->fault : v->cfg_fault ? LK_FAULT_BAD_CONFIG : 0;
    b[0] = LK_T_STATUS;
    b[1] = LK_STATUS_SIZE;
    s[0] = v->fault == LK_FAULT_ESTOP ? LK_ST_ESTOP : fault ? LK_ST_FAULT : (v->n || v->homing) ? LK_ST_RUN : LK_ST_READY;
    s[1] = fault ? 1 : 0;
    put16(s + 2, (uint32_t)(QN - v->n));
    put32(s + 4, v->exec_id);
    put32(s + 8, v->last_rx_id);
    put32(s + 12, (uint32_t)v->now_us);
    for (int a = 0; a < LK_AXES; a++) put32(s + 16 + 4 * a, (uint32_t)(int32_t)(v->pos[a] >> 16));
    put32(s + 96, v->local_in);
    put32(s + 100, v->outputs);
    put32(s + 104, v->rx_errors);
    unsigned len = 2 + LK_STATUS_SIZE;
    if (fault) {
        uint8_t *f = b + len;
        f[0] = LK_T_FAULT;
        f[1] = 8;
        f[2] = (uint8_t)fault;
        len += 10;
    }
    if (v->h_have) {
        uint8_t *f = b + len;
        f[0] = LK_T_HOME_DONE;
        f[1] = 12;
        put32(f + 2, v->hd_req);
        f[6] = (uint8_t)v->hd_axis;
        f[7] = (uint8_t)v->hd_result;
        put32(f + 10, (uint32_t)v->hd_pos);
        len += 14;
    }
    put16(t, LK_MAGIC);
    t[2] = LK_VERSION;
    t[3] = (uint8_t)((fault ? LK_F_FAULT : 0) | ((v->local_in & LK_IN_ESTOP) ? LK_F_ESTOP : 0));
    put16(t + 4, ++v->seq);
    put16(t + 6, v->rx_seq);
    put16(t + 8, len);
    put32(t + LK_PKT - 4, lk_crc32(t, LK_PKT - 4));
}

int vmcu_xfer(vmcu_t *v, const uint8_t tx[LK_PKT], uint8_t rx[LK_PKT])
{
    if (!v->manual) now_from_clock(v);
    if (get16(tx) == LK_MAGIC && get16(tx + 8) <= LK_BODY && lk_crc32(tx, LK_PKT - 4) == get32(tx + LK_PKT - 4)) {
        if (tx[2] == LK_VERSION) process(v, tx);
    } else v->rx_errors++;
    if (!v->lag) { reply(v, rx); return 0; }
    /* SPI: отговорът е подготвен преди транзакцията (на предишния пакет) */
    if (v->have_prev) memcpy(rx, v->prev, LK_PKT);
    else memset(rx, 0, LK_PKT);
    reply(v, v->prev);
    v->have_prev = true;
    return 0;
}

static int port_xfer(lk_port_t *p, const uint8_t tx[LK_PKT], uint8_t rx[LK_PKT]) { return vmcu_xfer(p->ctx, tx, rx); }
static void port_close(lk_port_t *p) { (void)p; }

lk_port_t *vmcu_port(vmcu_t *v)
{
    v->port.ctx = v;
    v->port.xfer = port_xfer;
    v->port.close = port_close;
    strcpy(v->port.name, "виртуален STM32");
    return &v->port;
}
