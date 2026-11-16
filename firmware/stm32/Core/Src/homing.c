/*
 * Реферирване (HOME) в STM32.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Фази: чакане осите да спрат → търсене (с ускорение до search_rate) →
 * спиране по рампа → отдръпване, докато входът изчезне и още pull стъпки →
 * спиране → бавно докосване (latch_rate), позицията се запомня → спиране.
 * Ако при старта входът вече е активен, първо се отдръпва.
 */
#include "homing.h"
#include "stepgen.h"
#include "inputs.h"
#include "main.h"

#define FEED_AHEAD   6u            /* сегменти по 1 ms напред: бързо спиране при сработване */
#define SEG_TICKS    (STEPGEN_TICK_HZ / 1000u)
#define HOME_SEG_ID  0x80000000u   /* номерата на сегментите на реферирането */

enum phase { PH_IDLE, PH_WAIT, PH_SEARCH, PH_STOP_FOUND, PH_BACKOFF, PH_STOP_BACK, PH_LATCH, PH_STOP_LATCH, PH_STOP_FAIL };

static struct home_req R;
static enum phase ph = PH_IDLE;
static int64_t hold[STEPGEN_AXES];  /* целта на сегментите, Q16 */
static double  vel;                 /* стъпки/s */
static double  acc;                 /* стъпки/s² */
static int32_t start_pos;
static int32_t latch_pos;
static uint8_t fail_result;
static uint32_t seg_id;
static struct home_done last;
static int have_last;

static int home_on(void)
{
  return (inputs_read() & IN_HOME(R.ch)) != 0u;
}

static void finish(uint8_t result)
{
  last.req    = R.req;
  last.axis   = R.axis;
  last.result = result;
  last.pos    = latch_pos;
  have_last   = 1;
  ph          = PH_IDLE;
}

/* започва фаза на движение от сегашната позиция */
static void begin_move(enum phase p, const struct stepgen_status *st)
{
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    hold[i] = (int64_t)st->position[i] * 65536;
  }
  start_pos = st->position[R.axis];
  vel       = 0.0;
  ph        = p;
}

static void feed(int dir, uint32_t rate)
{
  while ((STEPGEN_QUEUE - stepgen_queue_free()) < FEED_AHEAD) {
    struct stepgen_segment s;
    vel += acc * 0.001;
    if (vel > (double)rate) {
      vel = (double)rate;
    }
    hold[R.axis] += (int64_t)((double)dir * vel * 65.536);   /* стъпки за 1 ms в Q16 */
    s.id    = HOME_SEG_ID | (++seg_id & 0x7FFFFFFFu);
    s.ticks = SEG_TICKS;
    s.flags = 0;
    for (unsigned i = 0; i < STEPGEN_AXES; i++) {
      s.target[i] = hold[i];
    }
    if (stepgen_push(&s) != 0) {
      return;
    }
  }
}

static int32_t travel(const struct stepgen_status *st)
{
  int32_t d = st->position[R.axis] - start_pos;
  return (d < 0) ? -d : d;
}

int homing_start(const struct home_req *r)
{
  struct stepgen_axis_config c;

  if (ph != PH_IDLE || r->axis >= STEPGEN_AXES || r->ch >= 6u || (r->dir != 1 && r->dir != -1) ||
      r->search_rate == 0u || r->latch_rate == 0u || r->max_travel == 0u ||
      stepgen_get_config(r->axis, &c) != 0) {
    return -1;
  }
  R         = *r;
  acc       = (double)c.max_acc;
  latch_pos = 0;
  ph        = PH_WAIT;
  return 0;
}

void homing_abort(void)
{
  if (ph != PH_IDLE) {
    finish(HOME_ABORTED);
  }
}

int homing_active(void)
{
  return ph != PH_IDLE;
}

int homing_last(struct home_done *d)
{
  if (!have_last) {
    return 0;
  }
  *d = last;
  return 1;
}

void homing_service(void)
{
  struct stepgen_status st;

  if (ph == PH_IDLE) {
    return;
  }
  stepgen_get_status(&st);
  if (st.fault != STEPGEN_FAULT_NONE) {
    finish(HOME_ABORTED);
    return;
  }
  int idle = !st.running && st.queue_used == 0u;

  switch (ph) {
  case PH_WAIT:
    if (idle) {
      begin_move(home_on() ? PH_BACKOFF : PH_SEARCH, &st);
    }
    break;
  case PH_SEARCH:
    if (home_on()) {
      stepgen_stop();
      ph = PH_STOP_FOUND;
    } else if ((uint32_t)travel(&st) > R.max_travel) {
      stepgen_stop();
      fail_result = HOME_NOT_FOUND;
      ph = PH_STOP_FAIL;
    } else {
      feed(R.dir, R.search_rate);
    }
    break;
  case PH_STOP_FOUND:
    if (idle) {
      begin_move(PH_BACKOFF, &st);
    }
    break;
  case PH_BACKOFF:
    if (!home_on() && (uint32_t)travel(&st) >= R.pull) {
      stepgen_stop();
      ph = PH_STOP_BACK;
    } else if ((uint32_t)travel(&st) > R.max_travel) {
      stepgen_stop();
      fail_result = HOME_NOT_FOUND;                /* входът не изчезва: залепнал датчик */
      ph = PH_STOP_FAIL;
    } else {
      feed(-R.dir, R.search_rate);
    }
    break;
  case PH_STOP_BACK:
    if (idle) {
      begin_move(PH_LATCH, &st);
    }
    break;
  case PH_LATCH:
    if (home_on()) {
      latch_pos = st.position[R.axis];
      stepgen_stop();
      ph = PH_STOP_LATCH;
    } else if ((uint32_t)travel(&st) > 2u * R.pull + 1000u) {
      stepgen_stop();
      fail_result = HOME_NOT_FOUND;
      ph = PH_STOP_FAIL;
    } else {
      feed(R.dir, R.latch_rate);
    }
    break;
  case PH_STOP_LATCH:
    if (idle) {
      finish(HOME_OK);
    }
    break;
  case PH_STOP_FAIL:
    if (idle) {
      finish(fail_result);
    }
    break;
  case PH_IDLE:
  default:
    break;
  }
}
