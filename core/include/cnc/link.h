/* Връзката с STM32 от страната на Linux: пакетите и записите от docs/spi-protokol.md.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Същият формат като във firmware-а (firmware/stm32/Core/Inc/link.h): 256 байта, little-endian,
 * записи TLV, CRC-32 (zlib). Преносът е отделен (lk_port_t): SPI (spidev) на машината,
 * виртуален STM32 (vmcu.c) в симулатора и в тестовете. */
#ifndef CNC_LINK_H
#define CNC_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LK_PKT        256
#define LK_HDR        12
#define LK_BODY       240
#define LK_MAGIC      0x4E43u
#define LK_VERSION    1u
#define LK_AXES       6

/* флагове Linux → STM32 */
#define LK_F_ESTOP_REQ  0x01u
#define LK_F_RESET_REQ  0x02u
/* флагове STM32 → Linux */
#define LK_F_FAULT      0x01u
#define LK_F_ESTOP      0x02u

/* записи */
enum {
    LK_T_SEGMENT = 0x01, LK_T_IO_SET = 0x02, LK_T_SPINDLE = 0x03, LK_T_HOME_START = 0x05,
    LK_T_AXIS_CONFIG = 0x07, LK_T_MOTION_CTRL = 0x08,
    LK_T_STATUS = 0x81, LK_T_FAULT = 0x83, LK_T_HOME_DONE = 0x84, LK_T_AIN = 0x87
};
#define LK_SEG_SIZE      56
#define LK_STATUS_SIZE   120
#define LK_SEG_END       0x0001u   /* END_OF_MOVE: в края скоростта е 0 */

/* състояние на STM32 */
enum { LK_ST_BOOT, LK_ST_IDLE, LK_ST_READY, LK_ST_RUN, LK_ST_HOLD, LK_ST_STOPPING, LK_ST_FAULT, LK_ST_ESTOP };
/* MOTION_CTRL */
enum { LK_CTRL_HOLD = 1, LK_CTRL_RESUME = 2, LK_CTRL_ABORT = 3 };
/* AXIS_CONFIG */
enum { LK_CFG_MODE = 1, LK_CFG_INVERT = 2, LK_CFG_STEP_NS = 3, LK_CFG_DIR_NS = 4, LK_CFG_MAX_VEL = 5, LK_CFG_MAX_ACC = 6,
       LK_CFG_SOFT_MIN = 12, LK_CFG_SOFT_MAX = 13 };
/* кодове на грешките */
enum { LK_FAULT_LINK = 1, LK_FAULT_UNDERRUN = 2, LK_FAULT_FERROR = 3, LK_FAULT_LIMIT = 4, LK_FAULT_DRIVER = 5,
       LK_FAULT_ESTOP = 6, LK_FAULT_OVERSPEED = 7, LK_FAULT_BAD_CONFIG = 8, LK_FAULT_VERSION = 9, LK_FAULT_ENCODER = 10 };
/* local_in: собствените входове на STM32 */
#define LK_IN_LIM(a)     (1u << (a))
#define LK_IN_HOME(a)    (1u << (6 + (a)))
#define LK_IN_PROBE      (1u << 12)
#define LK_IN_TOOLSETTER (1u << 13)
#define LK_IN_ESTOP      (1u << 14)
#define LK_IN_DRV_ALARM  (1u << 15)

uint32_t lk_crc32(const uint8_t *p, size_t n);
const char *lk_fault_text(int code);

/* ---------- пакет за изпращане ---------- */
typedef struct {
    uint8_t b[LK_PKT];
    int len;               /* заетите байтове в записите */
} lk_tx_t;

void lk_begin(lk_tx_t *t);
int lk_room(const lk_tx_t *t);                                  /* свободни байтове за записи */
int lk_add(lk_tx_t *t, uint8_t type, const void *data, uint8_t size);   /* 0 = добавен, -1 = няма място */
void lk_finish(lk_tx_t *t, uint8_t flags, uint16_t seq, uint16_t ack);

/* записите (data за lk_add) */
typedef struct { uint8_t d[LK_SEG_SIZE]; } lk_seg_t;
void lk_seg(lk_seg_t *s, uint32_t id, uint16_t dur_us, uint16_t flags, const int64_t target[LK_AXES]);
void lk_axis_cfg(uint8_t d[8], int axis, int param, int32_t value);
void lk_ctrl(uint8_t d[4], int cmd);
void lk_io(uint8_t d[12], uint32_t value, uint32_t mask);
void lk_spindle(uint8_t d[12], int dir);                        /* 0 спрян, 1 по часовника, 2 обратно */
void lk_home_start(uint8_t d[24], uint32_t req, int axis, int dir, int ch, uint32_t search_sps,
                   uint32_t latch_sps, uint32_t travel_steps, uint32_t pull_steps);

/* ---------- приет пакет ---------- */
typedef struct {
    bool ok;               /* валиден пакет със STATUS */
    uint8_t flags;
    uint16_t seq, ack;
    int state, faults;
    int queue_free;
    uint32_t exec_id, last_rx_id, time_us;
    int32_t cmd[LK_AXES], enc[LK_AXES], ferr[LK_AXES];
    uint64_t inputs;
    uint32_t local_in, outputs, rx_errors, pendant;
    int32_t mpg;
    int fault, fault_axis;                 /* 0 = няма */
    bool home_valid;
    uint32_t home_req;
    int home_axis, home_result;
    int32_t home_pos;
    uint16_t ain_mv[4];
} lk_rx_t;

int lk_parse(const uint8_t pkt[LK_PKT], lk_rx_t *r);   /* 0 = валиден (magic, версия, CRC, STATUS) */

/* ---------- пренос ---------- */
/* Една транзакция: праща tx и връща приетото в същото време (при SPI това е отговорът на предишния). */
typedef struct lk_port {
    int (*xfer)(struct lk_port *p, const uint8_t tx[LK_PKT], uint8_t rx[LK_PKT]);   /* 0 = ok */
    void (*close)(struct lk_port *p);
    void *ctx;
    char name[64];
} lk_port_t;

/* spidev: dev = "/dev/spidev0.0", hz = такт (напр. 8000000); NULL при грешка (текстът в err) */
lk_port_t *lk_spi_open(const char *dev, uint32_t hz, char *err, size_t errn);

#ifdef __cplusplus
}
#endif

#endif
