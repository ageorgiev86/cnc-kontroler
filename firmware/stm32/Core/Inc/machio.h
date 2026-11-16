/*
 * Изходи, шпиндел, MPG колело, аналогови входове и спиране при краен изключвател.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Кое съществува зависи от платката (board.h / board_test3.h): изходи направо
 * на пинове (BOARD_OUTPUTS), колело на таймер (BOARD_MPG_TIM), аналогов вход
 * (BOARD_AIN1_*). Липсващото връща 0 и не прави нищо.
 *
 * Безопасност: при грешка на генератора (връзка, авариен стоп, лимит, ...)
 * всички изходи и шпинделът се изключват и остават изключени до новата команда
 * след RESET.
 */
#ifndef MACHIO_H
#define MACHIO_H

#include <stdint.h>

#define SPINDLE_STOP 0u
#define SPINDLE_CW   1u
#define SPINDLE_CCW  2u

void machio_init(void);
void machio_poll(void);

void     machio_set_outputs(uint32_t value, uint32_t mask);
uint32_t machio_outputs(void);      /* бит 0 = OUT1; бит 30 шпиндел CW, бит 31 шпиндел CCW */
void     machio_spindle(uint8_t dir);

uint32_t machio_pendant(void);      /* полето pendant в STATUS (docs/spi-protokol.md) */
int32_t  machio_mpg_count(void);
uint16_t machio_ain_mv(unsigned ch); /* напрежение на датчика, mV (0, ако няма вход) */

#endif /* MACHIO_H */
