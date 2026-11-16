/*
 * Собствените входове на STM32: крайни изключватели, HOME, пробник, toolsetter,
 * авариен стоп, аларма от драйвер.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * На основната платка всички минават през оптрон: 0 на пина = активен.
 * Пиновете са с вътрешно издърпване към 3,3 V, затова незакачен вход е „неактивен“.
 * Засега само се четат (за STATUS.local_in); реакцията (спиране при лимит,
 * засичане на пробника) идва по-късно.
 */
#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>

/* битове на inputs_read() и на STATUS.local_in в протокола (docs/spi-protokol.md) */
#define IN_LIM(k)        (1u << (k))         /* k = 0..5: LIM на канал 1–6 */
#define IN_HOME(k)       (1u << (6u + (k)))  /* k = 0..5: HOME на канал 1–6 */
#define IN_PROBE         (1u << 12)
#define IN_TOOLSETTER    (1u << 13)
#define IN_ESTOP         (1u << 14)
#define IN_DRV_FAULT     (1u << 15)
/* само на тестовата 3-осна машина (бутони и превключвателите на MPG колелото) */
#define IN_BTN_START     (1u << 16)
#define IN_BTN_STOP      (1u << 17)
#define IN_MPG_X         (1u << 18)
#define IN_MPG_Y         (1u << 19)
#define IN_MPG_Z         (1u << 20)
#define IN_MPG_X1        (1u << 21)
#define IN_MPG_X10       (1u << 22)
#define IN_MPG_X100      (1u << 23)

void inputs_init(void);

/* 1 = активен (пинът е на 0) */
uint32_t inputs_read(void);

#endif /* INPUTS_H */
