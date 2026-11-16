/*
 * Генериране на стъпки: TIM15 + DMA1 Stream0 → GPIO BSRR.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Позицията се пази с фиксирана точка Q32.32 (цели стъпки в горните 32 бита).
 * На всеки такт: позиция += скорост; закръглената позиция се сравнява с
 * изведените стъпки и при разлика се прави импулс (или първо смяна на
 * посоката с изчакване dir_setup). Импулсът трае step_len такта, след него
 * има поне step_space такта пауза. Затова най-високата честота на стъпките
 * е STEPGEN_TICK_HZ / (step_len + step_space): 200 kHz при 1 + 1,
 * 100 kHz при 2 + 2 (по подразбиране, подходящо за DM542).
 *
 * Изчисленията са в прекъсването на DMA (половин или цял буфер, на всеки
 * 250 µs). Изходът изостава от командата с до 2 половини (≤ 500 µs),
 * постоянно и предвидимо.
 */
#include "stepgen.h"
#include "main.h"
#include "irq_prio.h"
#include "stm32h7xx_it.h"

#include <string.h>

#define SEG_TICKS_MIN        (STEPGEN_TICK_HZ / 10000u)   /* 100 µs */
#define SEG_TICKS_MAX        (STEPGEN_TICK_HZ / 100u)     /* 10 ms */
#define OVERSPEED_LAG        8                             /* стъпки изоставане → грешка */
#define POS_LIMIT_Q16        ((int64_t)1 << 46)            /* ±2^30 стъпки */
#define TEST_AHEAD           40u                           /* сегменти напред при тестово движение */
#define DEFAULT_ACC          50000u                        /* стъпки/s² */

enum sg_state { SG_IDLE, SG_RUN, SG_STOPPING };

struct axis_rt {
  /* настройка */
  uint32_t step_set, step_clr;        /* BSRR маски за начало и край на импулса */
  uint32_t dir_fwd, dir_rev;          /* BSRR маски за двете посоки */
  uint8_t  step_len, step_space, dir_setup;
  int64_t  dec_q32;                   /* забавяне при спиране, Q32 стъпки/такт² */
  /* състояние */
  int64_t  pos;                       /* заповядана позиция, Q32.32 стъпки */
  int64_t  vel;                       /* скорост, Q32.32 стъпки/такт */
  int64_t  target;                    /* край на текущия сегмент, Q32.32 */
  int32_t  out;                       /* изведени стъпки */
  uint8_t  dir;                       /* текуща посока: 1 напред */
  uint8_t  busy;                      /* тактове до края на импулса + паузата */
  uint8_t  dir_wait;                  /* тактове до разрешение след смяна на посоката */
};

/* буфер за DMA: в RAM_D2, некеширан (виж MPU_Config в main.c) */
static uint32_t dma_buf[2 * STEPGEN_BLOCK] __attribute__((section(".dma_buffer"), aligned(32)));

static TIM_HandleTypeDef htim;
static DMA_HandleTypeDef hdma;

static struct axis_rt ax[STEPGEN_AXES];
static struct stepgen_axis_config cfg[STEPGEN_AXES];

static struct stepgen_segment queue[STEPGEN_QUEUE];
static volatile uint32_t q_head;      /* пише главният цикъл */
static volatile uint32_t q_tail;      /* пише прекъсването */

static volatile enum sg_state state;
static volatile uint8_t fault;
static volatile uint8_t stop_req;
static uint32_t seg_left;             /* оставащи тактове на сегмента; 0 = няма сегмент */
static uint16_t seg_flags;
static volatile uint32_t exec_id;
static volatile int32_t out_pos[STEPGEN_AXES];
static volatile uint32_t blocks;
static volatile uint32_t fill_max;

/* тестово движение (главен цикъл) */
static struct {
  int     active;
  double  length;                     /* преместване на водещата ос, стъпки */
  double  ratio[STEPGEN_AXES];        /* преместване на ос i / length */
  double  p, v, vmax, a;              /* път, скорост и ускорение по водещата ос */
  int64_t start_q16[STEPGEN_AXES];
  uint32_t id;
} test;

static void STEP_DMA_HalfCplt(DMA_HandleTypeDef *h);
static void STEP_DMA_Cplt(DMA_HandleTypeDef *h);

/* ---- прекъсване: смятане на думите за BSRR ---- */

static void enter_stopping(void)
{
  state    = SG_STOPPING;
  seg_left = 0;
  q_tail   = q_head;                  /* изхвърля опашката */
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    ax[i].target = ax[i].pos;
  }
}

static void set_fault(uint8_t code)
{
  if (fault == STEPGEN_FAULT_NONE) {
    fault = code;
  }
  if (state == SG_RUN) {
    enter_stopping();
  }
}

static int load_segment(void)
{
  const struct stepgen_segment *s;

  if (q_tail == q_head) {
    return 0;
  }
  s = &queue[q_tail & (STEPGEN_QUEUE - 1u)];
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    ax[i].target = s->target[i] * 65536;            /* Q16 → Q32 */
    ax[i].vel    = (ax[i].target - ax[i].pos) / (int64_t)s->ticks;
  }
  seg_left  = s->ticks;
  seg_flags = s->flags;
  exec_id   = s->id;
  __DMB();
  q_tail = q_tail + 1u;
  return 1;
}

/* Движение на заповяданата позиция с един такт. */
static void advance(void)
{
  if (stop_req) {
    stop_req = 0;
    if (state == SG_RUN) {
      enter_stopping();
    } else {
      q_tail = q_head;
    }
  }

  if (state == SG_IDLE && fault == STEPGEN_FAULT_NONE && q_tail != q_head) {
    state = SG_RUN;
  }

  if (state == SG_RUN) {
    if (seg_left == 0 && !load_segment()) {
      if (seg_flags & SEG_END_OF_MOVE) {
        state = SG_IDLE;
        for (unsigned i = 0; i < STEPGEN_AXES; i++) {
          ax[i].vel = 0;
        }
      } else {
        set_fault(STEPGEN_FAULT_UNDERRUN);          /* продължава в SG_STOPPING */
      }
    }
    if (state == SG_RUN) {
      seg_left--;
      for (unsigned i = 0; i < STEPGEN_AXES; i++) {
        ax[i].pos = (seg_left == 0) ? ax[i].target : ax[i].pos + ax[i].vel;
      }
      return;
    }
  }

  if (state == SG_STOPPING) {
    int moving = 0;
    for (unsigned i = 0; i < STEPGEN_AXES; i++) {
      struct axis_rt *a = &ax[i];
      if (a->vel > 0) {
        a->vel = (a->vel > a->dec_q32) ? a->vel - a->dec_q32 : 0;
      } else if (a->vel < 0) {
        a->vel = (-a->vel > a->dec_q32) ? a->vel + a->dec_q32 : 0;
      }
      a->pos += a->vel;
      moving |= (a->vel != 0);
    }
    if (!moving) {
      state     = SG_IDLE;
      seg_flags = SEG_END_OF_MOVE;
    }
  }
}

/* Импулсите за един такт; връща думата за BSRR. */
static uint32_t output_tick(void)
{
  uint32_t word = 0;

  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    struct axis_rt *a = &ax[i];
    int32_t want = (int32_t)((a->pos + ((int64_t)1 << 31)) >> 32);  /* закръгляне */
    int32_t lag  = want - a->out;

    if (a->busy > 0) {
      a->busy--;
      if (a->busy == a->step_space) {
        word |= a->step_clr;                        /* край на импулса */
      }
    }
    out_pos[i] = a->out;
    if (a->dir_wait > 0) {
      a->dir_wait--;
      continue;
    }
    if (a->busy > 0 || lag == 0) {
      continue;
    }
    if ((lag > 0) != (a->dir != 0)) {               /* първо посоката */
      a->dir      = (lag > 0) ? 1 : 0;
      word       |= a->dir ? a->dir_fwd : a->dir_rev;
      a->dir_wait = a->dir_setup;
      continue;
    }
    word   |= a->step_set;
    a->out += a->dir ? 1 : -1;
    a->busy = (uint8_t)(a->step_len + a->step_space);
    out_pos[i] = a->out;
    if (lag > OVERSPEED_LAG || lag < -OVERSPEED_LAG) {
      /* Поисканото е по-бързо от възможното. Моторът реално е на най-високата
       * честота на генератора, затова рампата започва оттам и от изведената
       * позиция, а не от поисканата скорост (иначе спирането е безкрайно дълго). */
      int64_t vmax = ((int64_t)1 << 32) / (a->step_len + a->step_space);
      a->pos = (int64_t)a->out * ((int64_t)1 << 32);
      a->vel = (a->vel > vmax) ? vmax : (a->vel < -vmax) ? -vmax : a->vel;
      set_fault(STEPGEN_FAULT_OVERSPEED);
    }
  }
  return word;
}

static void fill(uint32_t *dst)
{
  uint32_t t0 = DWT->CYCCNT;

  for (unsigned k = 0; k < STEPGEN_BLOCK; k++) {
    advance();
    dst[k] = output_tick();
  }
  uint32_t dt = DWT->CYCCNT - t0;
  if (dt > fill_max) {
    fill_max = dt;
  }
  blocks = blocks + 1u;
}

static void STEP_DMA_HalfCplt(DMA_HandleTypeDef *h)
{
  (void)h;
  fill(&dma_buf[0]);
}

static void STEP_DMA_Cplt(DMA_HandleTypeDef *h)
{
  (void)h;
  fill(&dma_buf[STEPGEN_BLOCK]);
}

void DMA1_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma);
}

/* ---- настройка ---- */

static void apply_config(unsigned i)
{
  static const uint16_t step_pin[BOARD_AXES_MAX] = BOARD_STEP_PINS;
  static const uint16_t dir_pin[BOARD_AXES_MAX]  = BOARD_DIR_PINS;
  const struct stepgen_axis_config *c = &cfg[i];
  struct axis_rt *a = &ax[i];
  uint64_t dec;

  a->step_set   = c->step_invert ? ((uint32_t)step_pin[i] << 16) : step_pin[i];
  a->step_clr   = c->step_invert ? step_pin[i] : ((uint32_t)step_pin[i] << 16);
  a->dir_fwd    = c->dir_invert ? ((uint32_t)dir_pin[i] << 16) : dir_pin[i];
  a->dir_rev    = c->dir_invert ? dir_pin[i] : ((uint32_t)dir_pin[i] << 16);
  a->step_len   = c->step_len;
  a->step_space = c->step_space;
  a->dir_setup  = c->dir_setup;
  dec = ((uint64_t)c->max_acc << 32) / ((uint64_t)STEPGEN_TICK_HZ * STEPGEN_TICK_HZ);
  a->dec_q32 = (dec > 0) ? (int64_t)dec : 1;

  /* пиновете в покой: без импулс, посока „назад“ (както a->dir = 0) */
  BOARD_STEP_PORT->BSRR = a->step_clr | a->dir_rev;
}

int stepgen_configure(unsigned axis, const struct stepgen_axis_config *c)
{
  if (axis >= STEPGEN_AXES || state != SG_IDLE || c->step_len < 1 || c->step_space < 1 ||
      c->step_len > 50 || c->step_space > 50 || c->dir_setup > 100 || c->max_acc == 0) {
    return -1;
  }
  NVIC_DisableIRQ(DMA1_Stream0_IRQn);
  cfg[axis] = *c;
  apply_config(axis);
  NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  return 0;
}

static uint32_t timer_clock(void)
{
  /* при TIMPRE = 0: тактът на таймерите е 2 × PCLK, ако делителят на APB е > 1 */
  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
  return (pclk2 == HAL_RCC_GetHCLKFreq()) ? pclk2 : 2u * pclk2;
}

void stepgen_init(void)
{
  GPIO_InitTypeDef g = {0};

  /* брояч на тактовете на процесора (за измерване на времето за попълване) */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->LAR = 0xC5ACCE55u;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    cfg[i].step_invert = BOARD_STEP_ACTIVE_LOW;   /* общ анод: импулсът е 0 */
    cfg[i].dir_invert  = 0;
    cfg[i].step_len    = 2;           /* 5 µs */
    cfg[i].step_space  = 2;           /* 5 µs → до 100 kHz */
    cfg[i].dir_setup   = 4;           /* 10 µs */
    cfg[i].max_acc     = DEFAULT_ACC;
  }

  BOARD_STEP_CLK_ENABLE();
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    apply_config(i);
  }
  {
    static const uint16_t step_pin[BOARD_AXES_MAX] = BOARD_STEP_PINS;
    static const uint16_t dir_pin[BOARD_AXES_MAX]  = BOARD_DIR_PINS;
    g.Pin = 0;
    for (unsigned i = 0; i < STEPGEN_AXES; i++) {
      g.Pin |= (uint32_t)step_pin[i] | dir_pin[i];
    }
  }
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BOARD_STEP_PORT, &g);

  memset(dma_buf, 0, sizeof(dma_buf));       /* 0 = без промяна на пиновете */
  state     = SG_IDLE;
  seg_flags = SEG_END_OF_MOVE;

  /* DMA: буфер → BSRR, по една дума на заявка от TIM15, в кръг */
  __HAL_RCC_DMA1_CLK_ENABLE();
  hdma.Instance                 = DMA1_Stream0;
  hdma.Init.Request             = DMA_REQUEST_TIM15_UP;
  hdma.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma.Init.MemInc              = DMA_MINC_ENABLE;
  hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  hdma.Init.Mode                = DMA_CIRCULAR;
  hdma.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  hdma.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma) != HAL_OK) {
    Error_Handler();
  }
  hdma.XferHalfCpltCallback = STEP_DMA_HalfCplt;
  hdma.XferCpltCallback     = STEP_DMA_Cplt;
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, IRQ_PRIO_STEPGEN, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

  /* TIM15: само таймер с честота STEPGEN_TICK_HZ, заявка за DMA при всяко препълване */
  __HAL_RCC_TIM15_CLK_ENABLE();
  htim.Instance               = TIM15;
  htim.Init.Prescaler         = 0;
  htim.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim.Init.Period            = timer_clock() / STEPGEN_TICK_HZ - 1u;
  htim.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim.Init.RepetitionCounter = 0;
  htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_DMA_Start_IT(&hdma, (uint32_t)dma_buf, (uint32_t)&BOARD_STEP_PORT->BSRR,
                       2u * STEPGEN_BLOCK) != HAL_OK) {
    Error_Handler();
  }
  __HAL_TIM_ENABLE_DMA(&htim, TIM_DMA_UPDATE);
  __HAL_TIM_ENABLE(&htim);

  /* Изходите на буферите към драйверите (74ACT541) се разрешават чак сега, когато пиновете
   * вече са в покой. До този момент резистор държи OE = 1 и драйверите не получават нищо. */
  BOARD_DRV_OE_CLK_ENABLE();
  HAL_GPIO_WritePin(BOARD_DRV_OE_N, GPIO_PIN_SET);
  g.Pin   = BOARD_DRV_OE_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BOARD_DRV_OE_PORT, &g);
  HAL_GPIO_WritePin(BOARD_DRV_OE_N, GPIO_PIN_RESET);
}

/* ---- главен цикъл ---- */

int stepgen_push(const struct stepgen_segment *seg)
{
  if (fault != STEPGEN_FAULT_NONE || state == SG_STOPPING) {
    return -2;
  }
  if (seg->ticks < SEG_TICKS_MIN || seg->ticks > SEG_TICKS_MAX) {
    return -3;
  }
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    if (seg->target[i] >= POS_LIMIT_Q16 || seg->target[i] <= -POS_LIMIT_Q16) {
      return -3;
    }
  }
  if ((q_head - q_tail) >= STEPGEN_QUEUE) {
    return -1;
  }
  queue[q_head & (STEPGEN_QUEUE - 1u)] = *seg;
  __DMB();
  q_head = q_head + 1u;
  return 0;
}

unsigned stepgen_queue_free(void)
{
  return STEPGEN_QUEUE - (unsigned)(q_head - q_tail);
}

void stepgen_stop(void)
{
  test.active = 0;
  stop_req    = 1;
}

void stepgen_fault(uint8_t code)
{
  NVIC_DisableIRQ(DMA1_Stream0_IRQn);
  test.active = 0;
  set_fault(code);
  q_tail = q_head;                    /* и в покой: изхвърля чакащите сегменти */
  NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

void stepgen_clear_fault(void)
{
  if (state == SG_IDLE) {
    fault = STEPGEN_FAULT_NONE;
  }
}

int stepgen_get_config(unsigned axis, struct stepgen_axis_config *c)
{
  if (axis >= STEPGEN_AXES) {
    return -1;
  }
  *c = cfg[axis];
  return 0;
}

void stepgen_get_status(struct stepgen_status *st)
{
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    st->position[i] = out_pos[i];
  }
  st->queue_used      = (uint16_t)(q_head - q_tail);
  st->queue_size      = STEPGEN_QUEUE;
  st->fault           = fault;
  st->running         = (state != SG_IDLE);
  st->stopping        = (state == SG_STOPPING);
  st->exec_id         = exec_id;
  st->blocks          = blocks;
  st->fill_cycles_max = fill_max;
}

static int64_t round_i64(double x)
{
  return (int64_t)((x >= 0.0) ? x + 0.5 : x - 0.5);
}

/* Позицията на осите в покой (Q16): оттам започва тестовото движение. */
static void idle_position_q16(int64_t p[STEPGEN_AXES])
{
  NVIC_DisableIRQ(DMA1_Stream0_IRQn);
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    p[i] = ax[i].pos / 65536;
  }
  NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

int stepgen_test_move(const int32_t steps[STEPGEN_AXES], int32_t rate, int32_t accel)
{
  int32_t longest = 0;

  if (test.active || state != SG_IDLE || q_head != q_tail) {
    return 1;
  }
  if (fault != STEPGEN_FAULT_NONE) {
    return 2;
  }
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    int32_t d = (steps[i] < 0) ? -steps[i] : steps[i];
    if (d > 100000000) {
      return 3;
    }
    if (d > longest) {
      longest = d;
    }
  }
  if (longest == 0 || rate <= 0 || accel < 0) {
    return 3;
  }
  /* всяка ос трябва да може да следва своята част от скоростта */
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    double axis_rate = (double)rate * (double)((steps[i] < 0) ? -steps[i] : steps[i]) / (double)longest;
    if (axis_rate > (double)(STEPGEN_TICK_HZ / (uint32_t)(cfg[i].step_len + cfg[i].step_space))) {
      return 4;
    }
  }
  test.length = (double)longest;
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    test.ratio[i] = (double)steps[i] / test.length;
  }
  test.p    = 0.0;
  test.v    = 0.0;
  test.vmax = (double)rate;
  test.a    = (double)((accel > 0) ? (uint32_t)accel : cfg[0].max_acc);
  idle_position_q16(test.start_q16);
  test.active = 1;
  return 0;
}

void stepgen_service(void)
{
  const double dt = 0.001;            /* сегменти по 1 ms, както от Linux */

  while (test.active && (q_head - q_tail) < TEST_AHEAD) {
    struct stepgen_segment s;
    double rem = test.length - test.p;
    int last   = 0;

    if (rem <= test.v * test.v / (2.0 * test.a)) {
      test.v -= test.a * dt;                        /* забавяне */
    } else if (test.v < test.vmax) {
      test.v += test.a * dt;                        /* ускорение */
      if (test.v > test.vmax) {
        test.v = test.vmax;
      }
    }
    if (test.v < test.a * dt) {                     /* последните части от стъпката */
      test.v = test.a * dt;
    }
    test.p += test.v * dt;
    if (test.p >= test.length) {
      test.p = test.length;
      last   = 1;
    }

    s.id    = ++test.id;
    s.ticks = STEPGEN_TICK_HZ / 1000u;
    s.flags = last ? SEG_END_OF_MOVE : 0;
    for (unsigned i = 0; i < STEPGEN_AXES; i++) {
      s.target[i] = test.start_q16[i] + round_i64(test.ratio[i] * test.p * 65536.0);
    }
    if (stepgen_push(&s) != 0) {
      test.active = 0;                              /* грешка или спиране */
      return;
    }
    if (last) {
      test.active = 0;
    }
  }
}
