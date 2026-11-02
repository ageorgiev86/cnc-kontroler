/*
 * Всичко, което зависи от конкретната платка WAVGAT STM32H723ZGT6.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Платката: надпис на гърба „LXB723ZG-P1“ (鹿小班), продава се от WAVGAT.
 * Проверено по схемата на LXB743ZI-P1: същата платка със STM32H743ZIT6.
 * Корпусът (LQFP144) и GPIO пиновете са еднакви с H723ZG.
 *
 *   Кварци        25 MHz (U2, PH0/PH1, кварц, не генератор), 32,768 kHz (X1, PC14/PC15)
 *   Ядро          LDO: 2 × 4,7 µF на VCAP (пинове 71 и 106)
 *   Захранване    USB-C (VBUS = +5V) → преобразувател RY3420 → 3,3 V
 *   Светодиоди    LED22 (червен, захранване); LED1 (син) на PG7, свети при 0
 *   Бутони        BOOT0 (към 3,3 V, 10 kΩ към GND), RST
 *   USB-C         PA11 (D−), PA12 (D+); CC1/CC2 с 5,1 kΩ към GND; VBUS не е към PA9
 *   Конектор H1   1 CLK (PA14), 2 DIO (PA13), 3 GND, 4 +5V,
 *                 5 RXD (PA10, USART1_RX), 6 TXD (PA9, USART1_TX), 7 RST (през 1 kΩ), 8 3,3 V
 *
 * Заети пинове (да не се ползват за стъпки, енкодери и входове):
 *   флаш W25Q64 (запоена)   OCTOSPI: PF6 IO3, PF7 IO2, PF8 IO0, PF9 IO1, PF10 CLK, PG6 NCS
 *   microSD (гнездо)         SDMMC1: PC8 D0, PC9 D1, PC10 D2, PC11 D3, PC12 CLK, PD2 CMD
 *   LCD (FPC 10 пина)        SPI6: PG13 SCK, PG14 MOSI, PG8 NSS; PG15 DC, PG12 подсветка
 *   CAMERA (FPC 24 пина)     DCMI: PA4, PA6, PG9, PC6, PC7, PG10, PG11, PE4, PD3, PE5, PE6;
 *                            PF13 PWDN, PF14/PF15 SCCB, PA5
 *   други                    PA13/PA14 SWD, PA11/PA12 USB, PG7 светодиод,
 *                            PA9/PA10 USART1 (на H1), PC14/PC15 и PH0/PH1 кварци
 * Пиновете на LCD и CAMERA са свободни, докато в конекторите няма нищо,
 * но е по-добре да остават за резерв.
 *
 * ОСТАВА да се провери на самата платка: че чипът е H723ZG (Device ID 0x483)
 * и че светодиодът мига (PG7).
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32h7xx_hal.h"

/* Светодиод LED1 (син): 3,3 V → 1 kΩ → светодиод → PG7, свети при 0 */
#define BOARD_LED_PORT        GPIOG
#define BOARD_LED_PIN         GPIO_PIN_7
#define BOARD_LED_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define BOARD_LED_ACTIVE_LOW  1

/* Захранване на ядрото: LDO (кондензатори на VCAP по схемата) */
#define BOARD_SUPPLY          PWR_LDO_SUPPLY

/*
 * Стъпки и посока. Всички STEP и DIR пинове трябва да са на ЕДИН порт:
 * DMA пише в един регистър BSRR (до 16 пина = 6 оси × 2 + резерв).
 * PG2 и PG3 са свободни по схемата и са изведени на конектора U10.
 * За 6 оси на порт G няма 12 свободни пина (PG6 флаш, PG7 светодиод,
 * PG8–PG15 LCD и камера), затова при разпределението на пиновете за
 * всички оси портът ще се смени (вероятно порт D).
 */
#define BOARD_STEP_PORT        GPIOG
#define BOARD_STEP_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define BOARD_AXIS0_STEP_PIN   GPIO_PIN_2   /* X STEP */
#define BOARD_AXIS0_DIR_PIN    GPIO_PIN_3   /* X DIR */

/*
 * USB: PA11 (D−) и PA12 (D+), фиксирани в чипа; описани са в usb_cdc.c.
 */

#endif /* BOARD_H */
