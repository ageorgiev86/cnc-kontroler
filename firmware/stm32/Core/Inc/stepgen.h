/*
 * Генериране на стъпки: един таймер (TIM15) + DMA към GPIO BSRR.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Принцип: на всеки такт на генератора (400 kHz, 2,5 µs) DMA записва една
 * 32-битова дума в регистъра BSRR на порта със стъпките. Долните 16 бита
 * вдигат пинове, горните ги свалят. Думите се смятат предварително в буфер
 * с две половини: докато DMA изпраща едната, прекъсването пълни другата.
 * Така времето на импулсите е точно до такта и не зависи от прекъсванията.
 *
 * Движението идва като сегменти с абсолютна крайна позиция (виж
 * docs/spi-protokol.md, MOTION_SEGMENT). Генераторът интерполира линейно
 * в рамките на сегмента.
 *
 * 6 оси (X, Y, Z, A, B, C). Всички пинове STEP и DIR са на един порт
 * (BOARD_STEP_PORT, порт D), защото DMA пише в един BSRR: всички оси се
 * променят в един и същ такт, без разминаване между тях. Пиновете са в
 * board.h (BOARD_STEP_PINS, BOARD_DIR_PINS).
 */
#ifndef STEPGEN_H
#define STEPGEN_H

#include <stdint.h>

#define STEPGEN_AXES       6u        /* X, Y, Z, A, B, C */
#define STEPGEN_TICK_HZ    400000u   /* такт на генератора */
#define STEPGEN_BLOCK      100u      /* тактове в половин буфер (250 µs) */
#define STEPGEN_QUEUE      256u      /* сегменти в опашката (степен на 2) */

/* флагове на сегмента (както в SPI протокола) */
#define SEG_END_OF_MOVE    0x0001u   /* в края скоростта е 0: празна опашка след него е нормална */

/* грешки (кодовете съвпадат с FAULT в SPI протокола) */
#define STEPGEN_FAULT_NONE      0u
#define STEPGEN_FAULT_LINK      1u   /* връзката с горното ниво е прекъснала (LINK_TIMEOUT) */
#define STEPGEN_FAULT_UNDERRUN  2u
#define STEPGEN_FAULT_ESTOP     6u   /* авариен стоп, поискан от горното ниво */
#define STEPGEN_FAULT_OVERSPEED 7u

struct stepgen_segment {
  uint32_t id;
  uint32_t ticks;                    /* продължителност в тактове на генератора */
  uint16_t flags;
  int64_t  target[STEPGEN_AXES];     /* абсолютна позиция в края, 1/65536 стъпка */
};

struct stepgen_axis_config {
  uint8_t  step_invert;              /* 1: импулсът е ниско ниво */
  uint8_t  dir_invert;               /* 1: обърната посока */
  uint8_t  step_len;                 /* дължина на импулса, тактове (≥ 1) */
  uint8_t  step_space;               /* минимална пауза след импулса, тактове (≥ 1) */
  uint8_t  dir_setup;                /* от смяна на посоката до импулса, тактове */
  uint32_t max_acc;                  /* стъпки/s², за спиране по рампа */
};

struct stepgen_status {
  int32_t  position[STEPGEN_AXES];   /* изведени стъпки */
  uint16_t queue_used;
  uint16_t queue_size;
  uint8_t  fault;
  uint8_t  running;
  uint8_t  stopping;                 /* спира по рампа */
  uint32_t exec_id;                  /* сегментът, който се изпълнява */
  uint32_t blocks;                   /* попълнени половини на буфера */
  uint32_t fill_cycles_max;          /* най-дълго попълване, тактове на процесора */
};

void stepgen_init(void);

/* Настройка на ос; само когато генераторът стои. Връща 0 или -1. */
int stepgen_configure(unsigned axis, const struct stepgen_axis_config *cfg);
int stepgen_get_config(unsigned axis, struct stepgen_axis_config *cfg);

/* Грешка отвън (връзка, авариен стоп): спиране по рампа и изчистване на опашката. */
void stepgen_fault(uint8_t code);

/* Добавя сегмент. Връща 0, -1 (опашката е пълна), -2 (активна грешка или спиране), -3 (грешни данни). */
int stepgen_push(const struct stepgen_segment *seg);

unsigned stepgen_queue_free(void);

/* Спиране по рампа с max_acc и изчистване на опашката. */
void stepgen_stop(void);

/* Изчиства грешката; само когато осите стоят. */
void stepgen_clear_fault(void);

void stepgen_get_status(struct stepgen_status *st);

/* Вика се от главния цикъл: подава сегментите на тестовото движение. */
void stepgen_service(void);

/* Тестово движение (без Linux), от конзолата: права линия по всички оси
 * едновременно, с трапецовидна скорост. steps[i] е преместването по ос i;
 * rate е скоростта (стъпки/s) на оста с най-голямо преместване, accel е
 * нейното ускорение (0 → max_acc от настройката). Връща 0 или код на отказ. */
int stepgen_test_move(const int32_t steps[STEPGEN_AXES], int32_t rate, int32_t accel);

#endif /* STEPGEN_H */
