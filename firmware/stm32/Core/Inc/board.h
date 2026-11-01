/*
 * Всичко, което зависи от конкретната платка WAVGAT STM32H723ZGT6.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * ВНИМАНИЕ: стойностите по-долу НЕ са проверени по схемата на платката.
 * Преди първото записване проверете:
 *   1. пина на светодиода (BOARD_LED_*) и дали свети при 0 или при 1;
 *   2. кварца (HSE_VALUE в stm32h7xx_hal_conf.h). Засега тактът е от вътрешния
 *      осцилатор HSI, затова кварцът не е нужен за първата проба;
 *   3. че ядрото е захранено през LDO (BOARD_SUPPLY). Ако платката е с SMPS,
 *      а тук остане LDO (или обратно), чипът спира веднага след старта и се
 *      възстановява само с BOOT0 = 1 и изтриване на паметта.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32h7xx_hal.h"

/* Светодиод: много core платки ползват PE3. ПРОВЕРИ. */
#define BOARD_LED_PORT        GPIOE
#define BOARD_LED_PIN         GPIO_PIN_3
#define BOARD_LED_CLK_ENABLE() __HAL_RCC_GPIOE_CLK_ENABLE()
#define BOARD_LED_ACTIVE_LOW  0     /* 1, ако светодиодът свети при логическа 0 */

/* Захранване на ядрото: STM32H723 няма SMPS, затова LDO. */
#define BOARD_SUPPLY          PWR_LDO_SUPPLY

#endif /* BOARD_H */
