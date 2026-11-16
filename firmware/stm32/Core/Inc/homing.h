/*
 * Реферирване (HOME) в STM32: търсене на датчика, отдръпване, бавно докосване.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Горното ниво поръчва с HOME_START (docs/spi-protokol.md) и получава HOME_DONE
 * с позицията (в стъпки) в момента на бавното докосване. Машинната координата
 * я задава горното ниво. Движението се подава на генератора на стъпки като
 * сегменти по 1 ms; входът се следи в главния цикъл (закъснение под 1 стъпка
 * при скоростите на реферирване).
 */
#ifndef HOMING_H
#define HOMING_H

#include <stdint.h>

#define HOME_OK        0u   /* намерен */
#define HOME_NOT_FOUND 1u   /* датчикът не сработи в рамките на хода */
#define HOME_ABORTED   2u   /* спряно (ABORT, грешка, авариен стоп) */

struct home_req {
  uint32_t req;          /* номер на заявката */
  uint8_t  axis;         /* 0–5 */
  int8_t   dir;          /* +1 или −1: посока към датчика */
  uint8_t  ch;           /* вход HOME, канал 0–5 */
  uint32_t search_rate;  /* стъпки/s */
  uint32_t latch_rate;   /* стъпки/s */
  uint32_t max_travel;   /* стъпки: по-далеч → HOME_NOT_FOUND */
  uint32_t pull;         /* стъпки отдръпване след първото докосване */
};

struct home_done {
  uint32_t req;
  uint8_t  axis;
  uint8_t  result;
  int32_t  pos;          /* стъпки в момента на бавното докосване */
};

/* 0 = прието; −1 = вече реферира или невалидни данни */
int homing_start(const struct home_req *r);
void homing_abort(void);
int homing_active(void);
void homing_service(void);

/* последният резултат; 0, ако още няма */
int homing_last(struct home_done *d);

#endif /* HOMING_H */
