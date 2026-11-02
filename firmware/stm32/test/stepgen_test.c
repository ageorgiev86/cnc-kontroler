/*
 * Проверка на генератора на стъпки на компютъра (без платка).
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * stepgen.c се компилира със заместители на HAL (test/mock/main.h).
 * Думите за BSRR се „изпълняват“ върху симулиран порт и се проверява:
 * броят стъпки, дължината на импулса и паузата, времето между смяна на
 * посоката и импулса, спирането по рампа и грешките.
 *
 * Компилиране и пускане (Linux, MSYS2 или всеки gcc):
 *   cd firmware/stm32/test && make
 */
#include "../Core/Src/stepgen.c"

#include <stdio.h>
#include <stdlib.h>

GPIO_TypeDef mock_gpiog;
DWT_Type mock_dwt;
CoreDebug_Type mock_coredebug;

void Error_Handler(void)
{
  fprintf(stderr, "Error_Handler\n");
  exit(2);
}

/* ---- симулиран изход ---- */

static struct {
  uint32_t level;             /* състояние на пиновете */
  long     tick;
  long     steps;             /* брой импулси, със знак по посоката */
  long     rise_tick, fall_tick, dir_tick;
  long     min_high, min_low, min_dir_setup;
  long     max_rate_ticks;    /* най-кратък период между импулсите */
  long     last_rise;
} sim;

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  ГРЕШКА: " __VA_ARGS__); printf("\n"); } } while (0)

static void sim_reset(void)
{
  memset(&sim, 0, sizeof(sim));
  sim.min_high = sim.min_low = sim.min_dir_setup = sim.max_rate_ticks = 1L << 30;
  sim.rise_tick = sim.fall_tick = sim.dir_tick = sim.last_rise = -1000000;
}

static void apply(uint32_t word)
{
  uint32_t prev = sim.level;

  sim.level |= word & 0xFFFFu;
  sim.level &= ~(word >> 16);
  if ((word & 0xFFFFu) & (word >> 16)) {
    failures++;
    printf("  ГРЕШКА: едновременно вдигане и сваляне на пин\n");
  }
  if ((prev ^ sim.level) & BOARD_AXIS0_DIR_PIN) {
    CHECK(!(sim.level & BOARD_AXIS0_STEP_PIN), "смяна на посоката по време на импулс (такт %ld)", sim.tick);
    sim.dir_tick = sim.tick;
  }
  if (!(prev & BOARD_AXIS0_STEP_PIN) && (sim.level & BOARD_AXIS0_STEP_PIN)) {
    long low = sim.tick - sim.fall_tick;
    long setup = sim.tick - sim.dir_tick;
    long period = sim.tick - sim.last_rise;
    if (low < sim.min_low) sim.min_low = low;
    if (setup < sim.min_dir_setup) sim.min_dir_setup = setup;
    if (period < sim.max_rate_ticks) sim.max_rate_ticks = period;
    sim.last_rise = sim.rise_tick = sim.tick;
    sim.steps += (sim.level & BOARD_AXIS0_DIR_PIN) ? 1 : -1;
  }
  if ((prev & BOARD_AXIS0_STEP_PIN) && !(sim.level & BOARD_AXIS0_STEP_PIN)) {
    long high = sim.tick - sim.rise_tick;
    if (high < sim.min_high) sim.min_high = high;
    sim.fall_tick = sim.tick;
  }
  sim.tick++;
}

/* Пуска генератора за ms милисекунди, както DMA + главния цикъл. */
static void run_ms(long ms, int feed)
{
  long n = ms * (long)STEPGEN_TICK_HZ / 1000 / STEPGEN_BLOCK;

  for (long b = 0; b < n; b++) {
    if (feed) {
      stepgen_service();
    }
    if ((b & 1) == 0) {
      STEP_DMA_HalfCplt(&hdma);
      for (unsigned k = 0; k < STEPGEN_BLOCK; k++) apply(dma_buf[k]);
    } else {
      STEP_DMA_Cplt(&hdma);
      for (unsigned k = 0; k < STEPGEN_BLOCK; k++) apply(dma_buf[STEPGEN_BLOCK + k]);
    }
  }
}

static void wait_idle(long max_ms)
{
  struct stepgen_status st;
  for (long t = 0; t < max_ms; t += 10) {
    run_ms(10, 1);
    stepgen_get_status(&st);
    if (!st.running && !test.active) {
      run_ms(2, 1);             /* изоставането на изхода */
      return;
    }
  }
}

static void report(const char *name)
{
  struct stepgen_status st;
  stepgen_get_status(&st);
  printf("%-34s стъпки %7ld, позиция %7ld, импулс ≥ %ld, пауза ≥ %ld, посока→импулс ≥ %ld, период ≥ %ld такта, грешка %u\n",
         name, sim.steps, (long)st.position, sim.min_high, sim.min_low, sim.min_dir_setup,
         sim.max_rate_ticks, st.fault);
}

static void check_timing(void)
{
  CHECK(sim.min_high >= cfg[0].step_len, "импулсът е по-къс от %u такта", cfg[0].step_len);
  CHECK(sim.min_low >= cfg[0].step_space, "паузата е по-къса от %u такта", cfg[0].step_space);
  CHECK(sim.min_dir_setup >= cfg[0].dir_setup, "посоката е сменена по-малко от %u такта преди импулса", cfg[0].dir_setup);
}

int main(void)
{
  struct stepgen_status st;
  struct stepgen_segment s;
  int r;

  stepgen_init();
  sim_reset();
  /* в покой: посока назад, без импулс */
  sim.level = 0;

  /* 1. напред 1000 стъпки */
  r = stepgen_test_move(1000, 20000, 0);
  CHECK(r == 0, "отказ %d", r);
  wait_idle(2000);
  report("1. +1000 при 20 kHz");
  stepgen_get_status(&st);
  CHECK(sim.steps == 1000 && st.position == 1000, "очаквани 1000 стъпки");
  CHECK(st.fault == 0, "неочаквана грешка");
  check_timing();

  /* 2. назад 1500 (смяна на посоката) */
  r = stepgen_test_move(-1500, 30000, 200000);
  CHECK(r == 0, "отказ %d", r);
  wait_idle(2000);
  report("2. -1500 при 30 kHz");
  CHECK(sim.steps == -500, "очаквана позиция -500");
  check_timing();

  /* 3. най-високата скорост при 2 + 2 такта: 100 kHz */
  r = stepgen_test_move(50000, 100000, 1000000);
  CHECK(r == 0, "отказ %d", r);
  wait_idle(5000);
  report("3. +50000 при 100 kHz");
  stepgen_get_status(&st);
  CHECK(sim.steps == 49500 && st.fault == 0, "очаквани 49500 без грешка");
  CHECK(sim.max_rate_ticks >= 4, "по-бързо от 100 kHz");
  check_timing();

  /* 4. над възможното → отказ */
  r = stepgen_test_move(1000, 150000, 0);
  CHECK(r != 0, "150 kHz трябва да се откаже при 2 + 2 такта");
  printf("4. 150 kHz при 2 + 2 такта: отказ %d (очакван)\n", r);

  /* 5. спиране по рампа по средата */
  r = stepgen_test_move(100000, 50000, 100000);
  CHECK(r == 0, "отказ %d", r);
  run_ms(300, 1);
  long before = sim.steps;
  stepgen_stop();
  wait_idle(3000);
  report("5. спиране по рампа");
  stepgen_get_status(&st);
  /* Рампата е с max_acc от настройката (50000 стъпки/s²), не с ускорението на
   * тестовото движение. Скоростта към 300 ms е около 30 kHz → v²/2a ≈ 9000. */
  CHECK(st.fault == 0, "спирането не е грешка");
  CHECK(sim.steps - before > 8000 && sim.steps - before < 10500, "пътят при спиране %ld е извън 8000–10500", sim.steps - before);
  CHECK(sim.steps < 49500 + 100000, "не трябва да стигне до края");
  check_timing();

  /* 6. празна опашка по време на движение → UNDERRUN и плавно спиране */
  {
    int64_t p = (int64_t)st.position * 65536;
    for (uint32_t i = 1; i <= 20; i++) {
      s.id = 1000 + i;
      s.ticks = 400;
      s.flags = 0;
      p += (int64_t)20 * 65536;   /* 20 стъпки/ms = 20 kHz */
      s.target[0] = p;
      CHECK(stepgen_push(&s) == 0, "push");
    }
    before = sim.steps;
    wait_idle(2000);
    stepgen_get_status(&st);
    report("6. празна опашка");
    CHECK(st.fault == STEPGEN_FAULT_UNDERRUN, "очаквана грешка UNDERRUN");
    CHECK(sim.steps - before > 400, "очаквано спиране след 400 стъпки");
    CHECK(stepgen_push(&s) == -2, "при грешка сегментите се отказват");
    stepgen_clear_fault();
    stepgen_get_status(&st);
    CHECK(st.fault == 0, "грешката не се изчисти");
    check_timing();
  }

  /* 7. сегмент по-бърз от генератора → OVERSPEED */
  long before7 = sim.steps;
  s.id = 2000;
  s.ticks = 400;
  s.flags = SEG_END_OF_MOVE;
  s.target[0] = (int64_t)(st.position + 1000) * 65536;   /* 1000 стъпки за 1 ms */
  CHECK(stepgen_push(&s) == 0, "push");
  wait_idle(3000);
  stepgen_get_status(&st);
  report("7. твърде бърз сегмент");
  CHECK(st.fault == STEPGEN_FAULT_OVERSPEED, "очаквана грешка OVERSPEED");
  CHECK(!st.running, "след OVERSPEED трябва да спре");
  /* рампа от 100 kHz с 50000 стъпки/s²: до 100000 стъпки */
  CHECK(sim.steps - before7 < 101000, "спирането след OVERSPEED е твърде дълго (%ld)", sim.steps - before7);
  check_timing();

  /* 8. изведените стъпки съвпадат с позицията на генератора */
  CHECK(sim.steps == st.position, "симулацията (%ld) и генераторът (%ld) се разминават", sim.steps, (long)st.position);

  printf("Попълване на половин буфер: %lu думи. %s\n", (unsigned long)STEPGEN_BLOCK,
         failures ? "ИМА ГРЕШКИ" : "Всичко е наред.");
  return failures ? 1 : 0;
}
