/*
 * Всичко, което зависи от конкретната платка WAVGAT STM32H723ZGT6.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Платката: надпис на гърба „LXB723ZG-P1“ (鹿小班), продава се от WAVGAT.
 * Видяно на снимките (още без схема):
 *   - кварц с надпис „25.000“ → HSE 25 MHz (вероятно), до него 32,768 kHz (LSE);
 *   - USB-C → PA11/PA12 (единствените пинове на вградения USB PHY);
 *   - SWD конектор: 5V, GND, DIO, CLK (БЕЗ 3,3 V: VTref на ST-LINK се взима
 *     от пин 3V3 на другия конектор: RXD, TXD, RST, 3V3);
 *   - бутони BOOT0 и RST; гнездо за microSD на гърба (SDMMC1: PC8–PC12, PD2);
 *   - конектори LCD и CAMERA и 8-пинов чип (вероятно флаш памет по QSPI):
 *     пиновете им се избягват за стъпки и енкодери, докато не се провери схемата.
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

/*
 * Стъпки и посока. Всички STEP и DIR пинове трябва да са на ЕДИН порт:
 * DMA пише в един регистър BSRR (до 16 пина = 6 оси × 2 + резерв).
 * Порт G е избран, защото не се ползва от microSD, USB и SWD.
 * НЕ Е ПРОВЕРЕНО по схемата: PG2/PG3 трябва да са изведени на конекторите
 * и да не са вързани към LCD, CAMERA или флаш паметта.
 */
#define BOARD_STEP_PORT        GPIOG
#define BOARD_STEP_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define BOARD_AXIS0_STEP_PIN   GPIO_PIN_2   /* X STEP */
#define BOARD_AXIS0_DIR_PIN    GPIO_PIN_3   /* X DIR */

/*
 * USB: PA11 (D−) и PA12 (D+), фиксирани в чипа; описани са в usb_cdc.c.
 */

#endif /* BOARD_H */
