/*
 * Връзка с горното ниво: пакети, CRC, сегменти, STATUS, watchdog на връзката.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Форматът е в docs/spi-protokol.md. Многобайтовите полета са little-endian
 * и се четат и пишат байт по байт (без packed структури и без подравняване).
 */
#include "link.h"
#include "main.h"
#include "stepgen.h"
#include "inputs.h"
#include "usb_cdc.h"
#include "homing.h"
#include "machio.h"

#include <string.h>

static uint32_t rx_errors;
static uint32_t last_rx_id;
static uint16_t tx_seq;
static uint16_t rx_seq;
static uint8_t  cfg_fault;             /* BAD_CONFIG или VERSION, докато не дойде RESET_REQ */
static uint32_t last_ok_ms;
static int      active;

/* ---- CRC-32 (ISO-HDLC, както zlib), таблица по 4 бита ---- */

uint32_t link_crc32(const uint8_t *p, size_t n)
{
  static const uint32_t t[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
  };
  uint32_t c = 0xFFFFFFFFu;

  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    c = (c >> 4) ^ t[c & 0xFu];
    c = (c >> 4) ^ t[c & 0xFu];
  }
  return c ^ 0xFFFFFFFFu;
}

/* ---- little-endian ---- */

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int64_t  get64(const uint8_t *p) { return (int64_t)((uint64_t)get32(p) | ((uint64_t)get32(p + 4) << 32)); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }

int link_check(const uint8_t pkt[LINK_PKT_SIZE])
{
  if (get16(pkt) != LINK_MAGIC) {
    return -1;
  }
  if (get16(pkt + 8) > LINK_BODY_MAX) {
    return -3;
  }
  if (link_crc32(pkt, LINK_PKT_SIZE - 4u) != get32(pkt + LINK_PKT_SIZE - 4u)) {
    return -4;
  }
  return 0;
}

/* ---- записи Linux → STM32 ---- */

static void do_segment(const uint8_t *d)
{
  struct stepgen_segment s;
  uint32_t us = get16(d + 4);

  s.id    = get32(d);
  s.ticks = (us * (STEPGEN_TICK_HZ / 1000u) + 500u) / 1000u;
  s.flags = get16(d + 6);
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    s.target[i] = get64(d + 8 + 8 * i);
  }
  if (s.id == 0 || s.id <= last_rx_id) {
    return;                                       /* вече приет (повторено изпращане) */
  }
  if (homing_active()) {
    last_rx_id = s.id;                            /* по време на реферирването движи само STM32 */
    return;
  }
  if (stepgen_push(&s) == -1) {
    return;                                       /* опашката е пълна: горното ниво ще прати пак */
  }
  last_rx_id = s.id;                              /* приет или отказан заради грешка */
}

static void do_axis_config(const uint8_t *d)
{
  struct stepgen_axis_config c;
  unsigned axis  = d[0];
  unsigned param = d[1];
  int32_t value  = (int32_t)get32(d + 4);
  const uint32_t ns_per_tick = 1000000000u / STEPGEN_TICK_HZ;

  if (stepgen_get_config(axis, &c) != 0) {
    cfg_fault = LINK_FAULT_BAD_CONFIG;
    return;
  }
  switch (param) {
  case 2:                                         /* STEP_INVERT / DIR_INVERT */
    c.step_invert = (uint8_t)(BOARD_STEP_ACTIVE_LOW ^ (value & 1));
    c.dir_invert  = (uint8_t)((value >> 1) & 1);
    break;
  case 3:                                         /* STEP_LEN_NS */
    c.step_len   = (uint8_t)((value + (int32_t)ns_per_tick - 1) / (int32_t)ns_per_tick);
    c.step_space = c.step_len;
    break;
  case 4:                                         /* DIR_SETUP_NS */
    c.dir_setup = (uint8_t)((value + (int32_t)ns_per_tick - 1) / (int32_t)ns_per_tick);
    break;
  case 6:                                         /* MAX_ACC */
    c.max_acc = (value > 0) ? (uint32_t)value : 1u;
    break;
  case 1: case 5: case 7: case 8: case 9: case 10: case 11:
    return;                                       /* известни, за затворения контур: още не се ползват */
  default:
    cfg_fault = LINK_FAULT_BAD_CONFIG;
    return;
  }
  if (stepgen_configure(axis, &c) != 0) {
    cfg_fault = LINK_FAULT_BAD_CONFIG;            /* извън границите или осите се движат */
  }
}

static void do_motion_ctrl(const uint8_t *d)
{
  if (d[0] == LINK_CTRL_ABORT || d[0] == LINK_CTRL_HOLD) {
    homing_abort();
    stepgen_stop();                               /* HOLD засега спира като ABORT */
  }
}

static void do_home_start(const uint8_t *d)
{
  struct home_req r;

  r.req         = get32(d);
  r.axis        = d[4];
  r.dir         = (int8_t)d[5];
  r.ch          = d[6];
  r.search_rate = get32(d + 8);
  r.latch_rate  = get32(d + 12);
  r.max_travel  = get32(d + 16);
  r.pull        = get32(d + 20);
  if (homing_start(&r) != 0) {
    cfg_fault = LINK_FAULT_BAD_CONFIG;
  }
}

/* ---- отговор ---- */

static uint8_t link_state(const struct stepgen_status *st)
{
  if (st->fault == STEPGEN_FAULT_ESTOP) {
    return LINK_ST_ESTOP;
  }
  if (st->fault != STEPGEN_FAULT_NONE || cfg_fault) {
    return LINK_ST_FAULT;
  }
  if (st->stopping) {
    return LINK_ST_STOPPING;
  }
  return st->running ? LINK_ST_RUN : LINK_ST_READY;
}

static void build_reply(uint8_t tx[LINK_PKT_SIZE])
{
  struct stepgen_status st;
  uint32_t in = inputs_read();
  uint8_t *b  = tx + LINK_HDR_SIZE;
  uint8_t *s;
  uint8_t fault;
  unsigned len;

  stepgen_get_status(&st);
  fault = st.fault ? st.fault : cfg_fault;
  memset(tx, 0, LINK_PKT_SIZE);

  b[0] = LINK_T_STATUS;
  b[1] = LINK_STATUS_SIZE;
  s    = b + 2;
  s[0] = link_state(&st);
  s[1] = fault ? 1u : 0u;
  put16(s + 2, (uint16_t)(st.queue_size - st.queue_used));
  put32(s + 4, st.exec_id);
  put32(s + 8, last_rx_id);
  put32(s + 12, HAL_GetTick() * 1000u);
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    put32(s + 16 + 4 * i, (uint32_t)st.position[i]);  /* заповядана позиция, стъпки */
  }
  /* enc_pos (s + 40) и ferror (s + 64): още няма енкодери, остават 0 */
  /* inputs (s + 88, 8 байта): 8 + 8 по SPI2 още не се четат, остават 0 */
  put32(s + 96, in);                                  /* local_in */
  put32(s + 100, machio_outputs());                   /* outputs */
  put32(s + 104, rx_errors);
  /* rules (s + 108): резервирано */
  put32(s + 112, machio_pendant());
  put32(s + 116, (uint32_t)machio_mpg_count());
  len = 2u + LINK_STATUS_SIZE;

  if (fault) {
    uint8_t *f = b + len;
    f[0] = LINK_T_FAULT;
    f[1] = 8;
    f[2] = fault;
    len += 10u;
  }
  {
    uint8_t *f = b + len;                         /* аналоговите входове: напрежение на датчика, mV */
    f[0] = LINK_T_AIN;
    f[1] = LINK_AIN_SIZE;
    for (unsigned i = 0; i < 4u; i++) {
      put16(f + 2 + 2 * i, machio_ain_mv(i));
    }
    len += 2u + LINK_AIN_SIZE;
  }
  {
    struct home_done h;
    if (homing_last(&h)) {                        /* последният резултат, докато не дойде нова заявка */
      uint8_t *f = b + len;
      f[0] = LINK_T_HOME_DONE;
      f[1] = LINK_HOME_DONE_SIZE;
      put32(f + 2, h.req);
      f[6] = h.axis;
      f[7] = h.result;
      put32(f + 10, (uint32_t)h.pos);
      len += 2u + LINK_HOME_DONE_SIZE;
    }
  }

  put16(tx, LINK_MAGIC);
  tx[2] = LINK_VERSION;
  tx[3] = (uint8_t)((fault ? LINK_F_FAULT : 0u) | ((in & IN_ESTOP) ? LINK_F_ESTOP : 0u));
  put16(tx + 4, ++tx_seq);
  put16(tx + 6, rx_seq);
  put16(tx + 8, (uint16_t)len);
  put32(tx + LINK_PKT_SIZE - 4u, link_crc32(tx, LINK_PKT_SIZE - 4u));
}

void link_process(const uint8_t rx[LINK_PKT_SIZE], uint8_t tx[LINK_PKT_SIZE])
{
  unsigned len = get16(rx + 8);
  uint8_t flags = rx[3];

  rx_seq = get16(rx + 4);
  if (rx[2] != LINK_VERSION) {
    cfg_fault = LINK_FAULT_VERSION;               /* отговаря, но не изпълнява команди */
    build_reply(tx);
    return;
  }
  if (flags & LINK_F_ESTOP_REQ) {
    stepgen_fault(STEPGEN_FAULT_ESTOP);
  }
  if (flags & LINK_F_RESET_REQ) {
    stepgen_clear_fault();
    cfg_fault = 0;
  }
  for (unsigned i = 0; i + 2u <= len;) {
    const uint8_t *r = rx + LINK_HDR_SIZE + i;
    unsigned type = r[0];
    unsigned size = r[1];
    if (type == 0 || i + 2u + size > len) {
      break;
    }
    if (type == LINK_T_MOTION_SEGMENT && size == LINK_SEG_SIZE) {
      do_segment(r + 2);
    } else if (type == LINK_T_AXIS_CONFIG && size == 8u) {
      do_axis_config(r + 2);
    } else if (type == LINK_T_MOTION_CTRL && size == 4u) {
      do_motion_ctrl(r + 2);
    } else if (type == LINK_T_HOME_START && size == LINK_HOME_START_SIZE) {
      do_home_start(r + 2);
    } else if (type == LINK_T_IO_SET && size == 12u) {
      machio_set_outputs(get32(r + 2), get32(r + 6));   /* синхронизацията със сегмент (r + 10) още не се ползва */
    } else if (type == LINK_T_SPINDLE && size == 12u) {
      machio_spindle(r[2]);                             /* посока; оборотите и PWM още не се ползват */
    }                                             /* непознат запис: пропуска се по size */
    i += 2u + size;
  }
  build_reply(tx);
}

/* ---- пренос по USB ---- */

static uint8_t usb_rx[LINK_PKT_SIZE];
static uint8_t usb_tx[LINK_PKT_SIZE];
static unsigned usb_idx;
static uint32_t usb_t0;

int link_usb_capturing(void)
{
  return usb_idx > 0u;
}

void link_usb_byte(uint8_t b)
{
  if (usb_idx == 0u) {
    usb_t0 = HAL_GetTick();
  }
  usb_rx[usb_idx++] = b;
  if (usb_idx == 2u && get16(usb_rx) != LINK_MAGIC) {
    usb_idx = 0;                                  /* не е пакет */
    return;
  }
  if (usb_idx < LINK_PKT_SIZE) {
    return;
  }
  usb_idx = 0;
  if (link_check(usb_rx) != 0) {
    rx_errors++;
    return;
  }
  link_process(usb_rx, usb_tx);
  last_ok_ms = HAL_GetTick();
  active     = 1;
  (void)usb_cdc_write(usb_tx, LINK_PKT_SIZE);
}

void link_poll(void)
{
  uint32_t now = HAL_GetTick();

  if (usb_idx > 0u && now - usb_t0 > 50u) {
    usb_idx = 0;                                  /* непълен пакет */
    rx_errors++;
  }
  if (active && now - last_ok_ms > LINK_USB_TIMEOUT_MS) {
    active = 0;
    stepgen_fault(STEPGEN_FAULT_LINK);            /* спиране по рампа */
  }
}

int link_active(void)
{
  return active;
}
