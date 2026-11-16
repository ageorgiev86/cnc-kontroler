/* Пакетите на връзката с STM32 (docs/spi-protokol.md) и преносът по spidev. */
#include "cnc/link.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#endif

uint32_t lk_crc32(const uint8_t *p, size_t n)
{
    static uint32_t t[256];
    static int init;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        init = 1;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = t[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

const char *lk_fault_text(int code)
{
    switch (code) {
    case LK_FAULT_LINK: return "връзката с Linux прекъсна";
    case LK_FAULT_UNDERRUN: return "празна опашка на сегментите (UNDERRUN)";
    case LK_FAULT_FERROR: return "следящата грешка е над лимита";
    case LK_FAULT_LIMIT: return "сработи краен изключвател";
    case LK_FAULT_DRIVER: return "аларма от драйвер";
    case LK_FAULT_ESTOP: return "авариен стоп";
    case LK_FAULT_OVERSPEED: return "поисканата скорост е над възможното (OVERSPEED)";
    case LK_FAULT_BAD_CONFIG: return "невалидна настройка (BAD_CONFIG)";
    case LK_FAULT_VERSION: return "различна версия на протокола";
    case LK_FAULT_ENCODER: return "грешка на енкодер";
    default: return "неизвестна грешка";
    }
}

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static void put64(uint8_t *p, uint64_t v) { put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

void lk_begin(lk_tx_t *t) { memset(t, 0, sizeof *t); }
int lk_room(const lk_tx_t *t) { return LK_BODY - t->len; }

int lk_add(lk_tx_t *t, uint8_t type, const void *data, uint8_t size)
{
    if (t->len + 2 + size > LK_BODY) return -1;
    uint8_t *r = t->b + LK_HDR + t->len;
    r[0] = type;
    r[1] = size;
    memcpy(r + 2, data, size);
    t->len += 2 + size;
    return 0;
}

void lk_finish(lk_tx_t *t, uint8_t flags, uint16_t seq, uint16_t ack)
{
    put16(t->b, LK_MAGIC);
    t->b[2] = LK_VERSION;
    t->b[3] = flags;
    put16(t->b + 4, seq);
    put16(t->b + 6, ack);
    put16(t->b + 8, (uint32_t)t->len);
    put32(t->b + LK_PKT - 4, lk_crc32(t->b, LK_PKT - 4));
}

void lk_seg(lk_seg_t *s, uint32_t id, uint16_t dur_us, uint16_t flags, const int64_t target[LK_AXES])
{
    put32(s->d, id);
    put16(s->d + 4, dur_us);
    put16(s->d + 6, flags);
    for (int a = 0; a < LK_AXES; a++) put64(s->d + 8 + 8 * a, (uint64_t)target[a]);
}

void lk_axis_cfg(uint8_t d[8], int axis, int param, int32_t value)
{
    memset(d, 0, 8);
    d[0] = (uint8_t)axis;
    d[1] = (uint8_t)param;
    put32(d + 4, (uint32_t)value);
}

void lk_ctrl(uint8_t d[4], int cmd) { memset(d, 0, 4); d[0] = (uint8_t)cmd; }

void lk_io(uint8_t d[12], uint32_t value, uint32_t mask)
{
    memset(d, 0, 12);
    put32(d, value);
    put32(d + 4, mask);
}

void lk_spindle(uint8_t d[12], int dir) { memset(d, 0, 12); d[0] = (uint8_t)dir; }

void lk_home_start(uint8_t d[24], uint32_t req, int axis, int dir, int ch, uint32_t search_sps,
                   uint32_t latch_sps, uint32_t travel_steps, uint32_t pull_steps)
{
    memset(d, 0, 24);
    put32(d, req);
    d[4] = (uint8_t)axis;
    d[5] = (uint8_t)(int8_t)(dir > 0 ? 1 : -1);
    d[6] = (uint8_t)ch;
    put32(d + 8, search_sps);
    put32(d + 12, latch_sps);
    put32(d + 16, travel_steps);
    put32(d + 20, pull_steps);
}

int lk_parse(const uint8_t p[LK_PKT], lk_rx_t *r)
{
    memset(r, 0, sizeof *r);
    if (get16(p) != LK_MAGIC || p[2] != LK_VERSION) return -1;
    unsigned len = get16(p + 8);
    if (len > LK_BODY) return -1;
    if (lk_crc32(p, LK_PKT - 4) != get32(p + LK_PKT - 4)) return -1;
    r->flags = p[3];
    r->seq = get16(p + 4);
    r->ack = get16(p + 6);
    for (unsigned i = 0; i + 2 <= len;) {
        const uint8_t *t = p + LK_HDR + i;
        unsigned type = t[0], size = t[1];
        const uint8_t *d = t + 2;
        if (!type || i + 2 + size > len) break;
        if (type == LK_T_STATUS && size >= LK_STATUS_SIZE) {
            r->ok = true;
            r->state = d[0];
            r->faults = d[1];
            r->queue_free = get16(d + 2);
            r->exec_id = get32(d + 4);
            r->last_rx_id = get32(d + 8);
            r->time_us = get32(d + 12);
            for (int a = 0; a < LK_AXES; a++) {
                r->cmd[a] = (int32_t)get32(d + 16 + 4 * a);
                r->enc[a] = (int32_t)get32(d + 40 + 4 * a);
                r->ferr[a] = (int32_t)get32(d + 64 + 4 * a);
            }
            r->inputs = (uint64_t)get32(d + 88) | (uint64_t)get32(d + 92) << 32;
            r->local_in = get32(d + 96);
            r->outputs = get32(d + 100);
            r->rx_errors = get32(d + 104);
            r->pendant = get32(d + 112);
            r->mpg = (int32_t)get32(d + 116);
        } else if (type == LK_T_FAULT && size >= 2) {
            r->fault = d[0];
            r->fault_axis = d[1];
        } else if (type == LK_T_HOME_DONE && size >= 12) {
            r->home_valid = true;
            r->home_req = get32(d);
            r->home_axis = d[4];
            r->home_result = d[5];
            r->home_pos = (int32_t)get32(d + 8);
        } else if (type == LK_T_AIN && size >= 8) {
            for (int k = 0; k < 4; k++) r->ain_mv[k] = get16(d + 2 * k);
        }
        i += 2 + size;
    }
    return r->ok ? 0 : -1;
}

/* ---------- spidev ---------- */
#ifdef __linux__
typedef struct { int fd; uint32_t hz; } spi_t;

static int spi_xfer(lk_port_t *p, const uint8_t tx[LK_PKT], uint8_t rx[LK_PKT])
{
    spi_t *s = p->ctx;
    struct spi_ioc_transfer t;
    memset(&t, 0, sizeof t);
    t.tx_buf = (uintptr_t)tx;
    t.rx_buf = (uintptr_t)rx;
    t.len = LK_PKT;
    t.speed_hz = s->hz;
    t.bits_per_word = 8;
    return ioctl(s->fd, SPI_IOC_MESSAGE(1), &t) == LK_PKT ? 0 : -1;
}

static void spi_close(lk_port_t *p)
{
    spi_t *s = p->ctx;
    close(s->fd);
    free(s);
    free(p);
}

lk_port_t *lk_spi_open(const char *dev, uint32_t hz, char *err, size_t errn)
{
    int fd = open(dev, O_RDWR | O_CLOEXEC);
    if (fd < 0) { snprintf(err, errn, "%s: %s", dev, strerror(errno)); return NULL; }
    uint8_t mode = SPI_MODE_0, bits = 8;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 || ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0) {
        snprintf(err, errn, "%s: настройката не мина (%s)", dev, strerror(errno));
        close(fd);
        return NULL;
    }
    lk_port_t *p = calloc(1, sizeof *p);
    spi_t *s = calloc(1, sizeof *s);
    if (!p || !s) { free(p); free(s); close(fd); snprintf(err, errn, "няма памет"); return NULL; }
    s->fd = fd;
    s->hz = hz;
    p->ctx = s;
    p->xfer = spi_xfer;
    p->close = spi_close;
    snprintf(p->name, sizeof p->name, "SPI %s, %u kHz", dev, (unsigned)(hz / 1000));
    return p;
}
#else
lk_port_t *lk_spi_open(const char *dev, uint32_t hz, char *err, size_t errn)
{
    (void)dev; (void)hz;
    snprintf(err, errn, "SPI има само под Linux");
    return NULL;
}
#endif
