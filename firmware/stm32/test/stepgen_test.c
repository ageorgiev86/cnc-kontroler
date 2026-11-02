/*
 * Проверка на генератора на стъпки на компютъра (без платка).
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * stepgen.c се компилира със заместители на HAL (test/mock/main.h).
 * Думите за BSRR се „изпълняват“ върху симулиран порт D и за всяка от
 * 6-те оси се проверява: броят стъпки, дължината на импулса и паузата,
 * времето между смяна на посоката и импулса, съгласуваното движение по
 * права линия, спирането по рампа и грешките.
 *
 * Компилиране и пускане (Linux, MSYS2 или всеки gcc):
 *   cd firmware/stm32/test && make
 */
#include "../Core/Src/stepgen.c"

#include <stdio.h>
#include <stdlib.h>

GPIO_TypeDef mock_gpiod;
DWT_Type mock_dwt;
CoreDebug_Type mock_coredebug;

void Error_Handler(void)
{
  fprintf(stderr, "Error_Handler\n");
  exit(2);
}

#define N STEPGEN_AXES
static const uint16_t STEP[N] = BOARD_STEP_PINS;
static const uint16_t DIR[N]  = BOARD_DIR_PINS;
static const char NAME[N] = { 'X', 'Y', 'Z', 'A', 'B', 'C' };

/* ---- симулиран порт ---- */

struct axis_sim {
  long steps;
  long rise, fall, dir_change, last_rise;
  long min_high, min_low, min_dir_setup, min_period;
};

static uint32_t level;
static long tick;
static struct axis_sim ax_sim[N];
static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  ГРЕШКА: " __VA_ARGS__); printf("\n"); } } while (0)

static void sim_reset_stats(void)
{
  for (unsigned i = 0; i < N; i++) {
    ax_sim[i].min_high = ax_sim[i].min_low = ax_sim[i].min_dir_setup = ax_sim[i].min_period = 1L << 30;
    ax_sim[i].rise = ax_sim[i].fall = ax_sim[i].dir_change = ax_sim[i].last_rise = -1000000;
  }
}

static void apply(uint32_t word)
{
  uint32_t prev = level;

  if ((word & 0xFFFFu) & (word >> 16)) {
    failures++;
    printf("  ГРЕШКА: едновременно вдигане и сваляне на пин (такт %ld)\n", tick);
  }
  level |= word & 0xFFFFu;
  level &= ~(word >> 16);

  for (unsigned i = 0; i < N; i++) {
    struct axis_sim *s = &ax_sim[i];
    if ((prev ^ level) & DIR[i]) {
      CHECK(!(level & STEP[i]), "%c: смяна на посоката по време на импулс (такт %ld)", NAME[i], tick);
      s->dir_change = tick;
    }
    if (!(prev & STEP[i]) && (level & STEP[i])) {
      long low = tick - s->fall, setup = tick - s->dir_change, period = tick - s->last_rise;
      if (low < s->min_low) s->min_low = low;
      if (setup < s->min_dir_setup) s->min_dir_setup = setup;
      if (period < s->min_period) s->min_period = period;
      s->rise = s->last_rise = tick;
      s->steps += (level & DIR[i]) ? 1 : -1;
    }
    if ((prev & STEP[i]) && !(level & STEP[i])) {
      long high = tick - s->rise;
      if (high < s->min_high) s->min_high = high;
      s->fall = tick;
    }
  }
  tick++;
}

/* Наблюдател, викан на всяка 1 ms (за проверка на правата линия). */
static void (*observer)(void);

/* Пуска генератора за ms милисекунди, както DMA + главния цикъл. */
static void run_ms(long ms)
{
  const long per_ms = (long)STEPGEN_TICK_HZ / 1000 / STEPGEN_BLOCK;
  long n = ms * per_ms;

  for (long b = 0; b < n; b++) {
    stepgen_service();
    if ((tick / STEPGEN_BLOCK) % 2 == 0) {
      STEP_DMA_HalfCplt(&hdma);
      for (unsigned k = 0; k < STEPGEN_BLOCK; k++) apply(dma_buf[k]);
    } else {
      STEP_DMA_Cplt(&hdma);
      for (unsigned k = 0; k < STEPGEN_BLOCK; k++) apply(dma_buf[STEPGEN_BLOCK + k]);
    }
    if (observer && (b % per_ms) == per_ms - 1) {
      observer();
    }
  }
}

static void wait_idle(long max_ms)
{
  struct stepgen_status st;
  for (long t = 0; t < max_ms; t += 10) {
    run_ms(10);
    stepgen_get_status(&st);
    if (!st.running && !test.active) {
      run_ms(2);                /* изоставането на изхода */
      return;
    }
  }
}

static void report(const char *name)
{
  struct stepgen_status st;
  stepgen_get_status(&st);
  printf("%s (грешка %u)\n", name, st.fault);
  for (unsigned i = 0; i < N; i++) {
    if (ax_sim[i].min_high == (1L << 30)) {
      continue;                 /* оста още не е мърдала */
    }
    printf("   %c: позиция %7ld, импулс ≥ %ld, пауза ≥ %ld, посока→импулс ≥ %ld, период ≥ %ld такта\n",
           NAME[i], ax_sim[i].steps, ax_sim[i].min_high, ax_sim[i].min_low, ax_sim[i].min_dir_setup,
           ax_sim[i].min_period);
  }
}

static void check_axes(void)
{
  struct stepgen_status st;
  stepgen_get_status(&st);
  for (unsigned i = 0; i < N; i++) {
    CHECK(ax_sim[i].min_high >= cfg[i].step_len, "%c: импулсът е по-къс от %u такта", NAME[i], cfg[i].step_len);
    CHECK(ax_sim[i].min_low >= cfg[i].step_space, "%c: паузата е по-къса от %u такта", NAME[i], cfg[i].step_space);
    CHECK(ax_sim[i].min_dir_setup >= cfg[i].dir_setup, "%c: посоката е сменена по-малко от %u такта преди импулса",
          NAME[i], cfg[i].dir_setup);
    CHECK(ax_sim[i].steps == st.position[i], "%c: симулацията (%ld) и генераторът (%ld) се разминават",
          NAME[i], ax_sim[i].steps, (long)st.position[i]);
  }
}

static int move(int32_t x, int32_t y, int32_t z, int32_t a, int32_t b, int32_t c, int32_t rate, int32_t acc)
{
  const int32_t d[N] = { x, y, z, a, b, c };
  return stepgen_test_move(d, rate, acc);
}

/* права линия: всяка ос трябва да е на своята част от пътя на водещата (X) */
static const double *line_ratio;
static long line_start[N];
static long line_dev_max;

static void line_observer(void)
{
  long lead = ax_sim[0].steps - line_start[0];
  for (unsigned i = 1; i < N; i++) {
    double w  = line_ratio[i] * (double)lead;
    long want = (long)(w >= 0 ? w + 0.5 : w - 0.5);
    long dev  = labs((ax_sim[i].steps - line_start[i]) - want);
    if (dev > line_dev_max) {
      line_dev_max = dev;
    }
  }
}

int main(void)
{
  struct stepgen_status st;
  struct stepgen_segment s;
  long before;

  stepgen_init();
  sim_reset_stats();

  /* 1. X напред 1000 */
  CHECK(move(1000, 0, 0, 0, 0, 0, 20000, 0) == 0, "отказ");
  wait_idle(2000);
  report("1. X +1000 при 20 kHz");
  CHECK(ax_sim[0].steps == 1000, "очаквани 1000 стъпки по X");
  check_axes();

  /* 2. X назад 1500 (смяна на посоката) */
  CHECK(move(-1500, 0, 0, 0, 0, 0, 30000, 200000) == 0, "отказ");
  wait_idle(2000);
  report("2. X -1500 при 30 kHz");
  CHECK(ax_sim[0].steps == -500, "очаквана позиция -500");
  check_axes();

  /* 3. права линия по 6 оси, X водеща при 100 kHz */
  {
    static const double ratio[N] = { 1.0, -0.5, 0.25, 0.8, -1.0, 0.1 };
    const int32_t L = 40000;
    for (unsigned i = 0; i < N; i++) line_start[i] = ax_sim[i].steps;
    line_ratio   = ratio;
    line_dev_max = 0;
    observer     = line_observer;
    CHECK(move(L, -L / 2, L / 4, (int32_t)(0.8 * L), -L, L / 10, 100000, 500000) == 0, "отказ");
    wait_idle(5000);
    observer = NULL;
    report("3. права линия по 6 оси, 100 kHz");
    for (unsigned i = 0; i < N; i++) {
      long want = line_start[i] + (long)(ratio[i] * L);
      CHECK(ax_sim[i].steps == want, "%c: очаквани %ld, а са %ld", NAME[i], want, ax_sim[i].steps);
    }
    printf("   най-голямо отклонение от правата: %ld стъпки\n", line_dev_max);
    CHECK(line_dev_max <= 2, "отклонение от правата %ld > 2 стъпки", line_dev_max);
    check_axes();
  }

  /* 4. над възможното и празно движение → отказ */
  {
    int r = move(1000, 0, 0, 0, 0, 0, 150000, 0);
    CHECK(r == 4, "X при 150 kHz трябва да се откаже (код 4), а е %d", r);
    r = move(1000, 1000, 0, 0, 0, 0, 120000, 0);
    CHECK(r == 4, "Y при 120 kHz трябва да се откаже (код 4), а е %d", r);
    r = move(0, 0, 0, 0, 0, 0, 1000, 0);
    CHECK(r == 3, "движение без преместване трябва да се откаже (код 3), а е %d", r);
    printf("4. над възможното и празно движение: отказ\n");
  }

  /* 5. спиране по рампа по средата на движение по X и Y */
  CHECK(move(100000, 50000, 0, 0, 0, 0, 50000, 100000) == 0, "отказ");
  run_ms(300);
  before = ax_sim[0].steps;
  long before_y = ax_sim[1].steps;
  stepgen_stop();
  wait_idle(3000);
  report("5. спиране по рампа");
  stepgen_get_status(&st);
  /* рампа с max_acc = 50000 стъпки/s² за всяка ос; X е на около 30 kHz → v²/2a ≈ 9000,
   * Y е на половината скорост → около 2250 */
  CHECK(st.fault == 0, "спирането не е грешка");
  CHECK(ax_sim[0].steps - before > 8000 && ax_sim[0].steps - before < 10500,
        "X: пътят при спиране %ld е извън 8000–10500", ax_sim[0].steps - before);
  CHECK(ax_sim[1].steps - before_y > 1500 && ax_sim[1].steps - before_y < 3000,
        "Y: пътят при спиране %ld е извън 1500–3000", ax_sim[1].steps - before_y);
  check_axes();

  /* 6. празна опашка по време на движение на Z → UNDERRUN и плавно спиране */
  {
    int64_t p[N];
    stepgen_get_status(&st);
    for (unsigned i = 0; i < N; i++) p[i] = (int64_t)st.position[i] * 65536;
    for (uint32_t k = 1; k <= 20; k++) {
      s.id    = 1000 + k;
      s.ticks = 400;
      s.flags = 0;
      p[2] += (int64_t)20 * 65536;                   /* Z: 20 стъпки/ms = 20 kHz */
      for (unsigned i = 0; i < N; i++) s.target[i] = p[i];
      CHECK(stepgen_push(&s) == 0, "push");
    }
    before = ax_sim[2].steps;
    wait_idle(2000);
    stepgen_get_status(&st);
    report("6. празна опашка");
    CHECK(st.fault == STEPGEN_FAULT_UNDERRUN, "очаквана грешка UNDERRUN");
    CHECK(ax_sim[2].steps - before > 400, "очаквано спиране след 400 стъпки");
    CHECK(stepgen_push(&s) == -2, "при грешка сегментите се отказват");
    stepgen_clear_fault();
    stepgen_get_status(&st);
    CHECK(st.fault == 0, "грешката не се изчисти");
    check_axes();
  }

  /* 7. сегмент по-бърз от генератора по ос B → OVERSPEED, спират всички оси */
  {
    long before_x = ax_sim[0].steps;
    before = ax_sim[4].steps;
    stepgen_get_status(&st);
    s.id    = 2000;
    s.ticks = 400;
    s.flags = SEG_END_OF_MOVE;
    for (unsigned i = 0; i < N; i++) s.target[i] = (int64_t)st.position[i] * 65536;
    s.target[0] += (int64_t)10 * 65536;              /* X: 10 стъпки за 1 ms, нормално */
    s.target[4] += (int64_t)1000 * 65536;            /* B: 1000 стъпки за 1 ms, невъзможно */
    CHECK(stepgen_push(&s) == 0, "push");
    wait_idle(3000);
    stepgen_get_status(&st);
    report("7. твърде бърз сегмент по B");
    CHECK(st.fault == STEPGEN_FAULT_OVERSPEED, "очаквана грешка OVERSPEED");
    CHECK(!st.running, "след OVERSPEED трябва да спре");
    CHECK(ax_sim[4].steps - before < 101000, "B: спирането е твърде дълго (%ld)", ax_sim[4].steps - before);
    /* X върви с 10 kHz и спира по своята рампа: v²/2a = 10000² / (2 × 50000) = 1000 стъпки */
    CHECK(ax_sim[0].steps - before_x <= 1100, "X: спирането е твърде дълго (%ld)", ax_sim[0].steps - before_x);
    check_axes();
  }

  printf("%s\n", failures ? "ИМА ГРЕШКИ" : "Всичко е наред.");
  return failures ? 1 : 0;
}
