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
 * ---------------------------------------------------------------------------
 * Разпределение на пиновете. Таблицата и мотивите: docs/stm32-pinove.md.
 * Сигнал, който е само GPIO, е дефиниран като двойка „порт, пин“ и се
 * ползва направо: HAL_GPIO_ReadPin(BOARD_PROBE).
 * ---------------------------------------------------------------------------
 */

/* Стъпки и посока: всички на порт D (DMA пише в един регистър BSRR).
 * Редът на осите: X, Y, Z, A, B, C. */
#define BOARD_STEP_PORT         GPIOD
#define BOARD_STEP_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define BOARD_STEP_PINS  { GPIO_PIN_4, GPIO_PIN_6, GPIO_PIN_8, GPIO_PIN_10, GPIO_PIN_12, GPIO_PIN_14 }
#define BOARD_DIR_PINS   { GPIO_PIN_5, GPIO_PIN_7, GPIO_PIN_9, GPIO_PIN_11, GPIO_PIN_13, GPIO_PIN_15 }
#define BOARD_AXES_MAX          6

#define BOARD_DRV_EN            GPIOB, GPIO_PIN_11   /* разрешение на драйверите (изход) */
#define BOARD_DRV_FAULT         GPIOE, GPIO_PIN_3    /* грешка от драйвер (вход, EXTI3) */

/* Енкодери: таймер в encoder mode, A = CH1, B = CH2, индекс Z = CH3 (input capture) */
#define BOARD_ENC_X_TIM         TIM5                 /* 32 бита */
#define BOARD_ENC_X_AF          GPIO_AF2_TIM5
#define BOARD_ENC_X_A           GPIOA, GPIO_PIN_0
#define BOARD_ENC_X_B           GPIOA, GPIO_PIN_1
#define BOARD_ENC_X_Z           GPIOA, GPIO_PIN_2

#define BOARD_ENC_Y_TIM         TIM2                 /* 32 бита */
#define BOARD_ENC_Y_AF          GPIO_AF1_TIM2
#define BOARD_ENC_Y_A           GPIOA, GPIO_PIN_15
#define BOARD_ENC_Y_B           GPIOB, GPIO_PIN_3
#define BOARD_ENC_Y_Z           GPIOB, GPIO_PIN_10

#define BOARD_ENC_Z_TIM         TIM23                /* 32 бита */
#define BOARD_ENC_Z_AF          GPIO_AF13_TIM23
#define BOARD_ENC_Z_A           GPIOF, GPIO_PIN_0
#define BOARD_ENC_Z_B           GPIOF, GPIO_PIN_1
#define BOARD_ENC_Z_Z           GPIOF, GPIO_PIN_2

#define BOARD_ENC_A_TIM         TIM3                 /* 16 бита */
#define BOARD_ENC_A_AF          GPIO_AF2_TIM3
#define BOARD_ENC_A_A           GPIOB, GPIO_PIN_4
#define BOARD_ENC_A_B           GPIOB, GPIO_PIN_5
#define BOARD_ENC_A_Z           GPIOB, GPIO_PIN_0

#define BOARD_ENC_B_TIM         TIM4                 /* 16 бита */
#define BOARD_ENC_B_AF          GPIO_AF2_TIM4
#define BOARD_ENC_B_A           GPIOB, GPIO_PIN_6
#define BOARD_ENC_B_B           GPIOB, GPIO_PIN_7
#define BOARD_ENC_B_Z           GPIOB, GPIO_PIN_8

#define BOARD_ENC_C_TIM         TIM1                 /* 16 бита; PA9/PA10 са и на H1 (TXD/RXD) */
#define BOARD_ENC_C_AF          GPIO_AF1_TIM1
#define BOARD_ENC_C_A           GPIOA, GPIO_PIN_8
#define BOARD_ENC_C_B           GPIOA, GPIO_PIN_9
#define BOARD_ENC_C_Z           GPIOA, GPIO_PIN_10

#define BOARD_ENC_SP_TIM        TIM24                /* шпиндел, 32 бита */
#define BOARD_ENC_SP_AF         GPIO_AF14_TIM24
#define BOARD_ENC_SP_A          GPIOF, GPIO_PIN_11
#define BOARD_ENC_SP_B          GPIOF, GPIO_PIN_12
#define BOARD_ENC_SP_Z          GPIOF, GPIO_PIN_13   /* на конектора CAMERA (празен) */

#define BOARD_ENC_RES_TIM       TIM8                 /* резерв: гантри X2 или MPG */
#define BOARD_ENC_RES_AF        GPIO_AF3_TIM8
#define BOARD_ENC_RES_A         GPIOC, GPIO_PIN_6    /* на конектора CAMERA */
#define BOARD_ENC_RES_B         GPIOC, GPIO_PIN_7

/* Връзка с Linux: SPI4 slave */
#define BOARD_LINK_SPI          SPI4
#define BOARD_LINK_AF           GPIO_AF5_SPI4
#define BOARD_LINK_NSS          GPIOE, GPIO_PIN_11
#define BOARD_LINK_SCK          GPIOE, GPIO_PIN_12
#define BOARD_LINK_MISO         GPIOE, GPIO_PIN_13
#define BOARD_LINK_MOSI         GPIOE, GPIO_PIN_14
#define BOARD_LINK_READY        GPIOE, GPIO_PIN_15   /* изход: готов за пакет */
#define BOARD_LINK_IRQ          GPIOE, GPIO_PIN_10   /* изход: спешно събитие */

/* Входове и изходи 32 + 32, ADC и DAC зад изолатора: SPI2 master */
#define BOARD_IO_SPI            SPI2
#define BOARD_IO_AF             GPIO_AF5_SPI2
#define BOARD_IO_SCK            GPIOB, GPIO_PIN_13
#define BOARD_IO_MISO           GPIOB, GPIO_PIN_14
#define BOARD_IO_MOSI           GPIOB, GPIO_PIN_15
#define BOARD_IO_LATCH          GPIOB, GPIO_PIN_12   /* 74HC595 */
#define BOARD_IO_LOAD           GPIOE, GPIO_PIN_7    /* 74HC165 */
#define BOARD_IO_OE             GPIOE, GPIO_PIN_8    /* 74HC595, активен при 0 */
#define BOARD_IO_ADC_CS         GPIOE, GPIO_PIN_9
#define BOARD_IO_DAC_CS         GPIOE, GPIO_PIN_2

/* Бързи входове (по EXTI) */
#define BOARD_PROBE             GPIOA, GPIO_PIN_7    /* EXTI7; и TIM14_CH1 (AF9) */
#define BOARD_TOOLSETTER        GPIOB, GPIO_PIN_1    /* EXTI1 */
#define BOARD_ESTOP             GPIOC, GPIO_PIN_13   /* EXTI13, наблюдение на веригата */

/* Крайни изключватели (NC) и HOME; редът на осите: X, Y, Z, A, B, C */
#define BOARD_LIMIT_X           GPIOF, GPIO_PIN_3
#define BOARD_LIMIT_Y           GPIOF, GPIO_PIN_4
#define BOARD_LIMIT_Z           GPIOF, GPIO_PIN_5
#define BOARD_LIMIT_A           GPIOG, GPIO_PIN_0
#define BOARD_LIMIT_B           GPIOG, GPIO_PIN_1
#define BOARD_LIMIT_C           GPIOG, GPIO_PIN_2
#define BOARD_HOME_X            GPIOG, GPIO_PIN_3
#define BOARD_HOME_Y            GPIOG, GPIO_PIN_4
#define BOARD_HOME_Z            GPIOG, GPIO_PIN_5
#define BOARD_HOME_A            GPIOE, GPIO_PIN_0
#define BOARD_HOME_B            GPIOE, GPIO_PIN_1
#define BOARD_HOME_C            GPIOB, GPIO_PIN_2

/* Шпиндел */
#define BOARD_SPINDLE_PWM_TIM   TIM17                /* CH1 */
#define BOARD_SPINDLE_PWM_AF    GPIO_AF1_TIM17
#define BOARD_SPINDLE_PWM       GPIOB, GPIO_PIN_9
#define BOARD_SPINDLE_DIR       GPIOC, GPIO_PIN_4
#define BOARD_SPINDLE_EN        GPIOC, GPIO_PIN_5

/* Аналогови входове (4) и изходи (2) за опитите: вградените ADC и DAC.
 * На машината са зад изолатора по SPI2 (BOARD_IO_ADC_CS, BOARD_IO_DAC_CS). */
#define BOARD_AIN1              GPIOC, GPIO_PIN_0    /* ADC1_INP10 */
#define BOARD_AIN2              GPIOC, GPIO_PIN_1    /* ADC1_INP11 */
#define BOARD_AIN3_ADC3_CH      0                    /* PC2_C, ADC3_INP0 (само аналогов) */
#define BOARD_AIN4_ADC3_CH      1                    /* PC3_C, ADC3_INP1 (само аналогов) */
#define BOARD_AOUT1             GPIOA, GPIO_PIN_4    /* DAC1_OUT1; на конектора CAMERA */
#define BOARD_AOUT2             GPIOA, GPIO_PIN_5    /* DAC1_OUT2; на конектора CAMERA */

/* Резерв: PA3 (ADC1_INP15, TIM5_CH4); PD0/PD1 FDCAN1 RX/TX (AF9) */

/*
 * USB: PA11 (D−) и PA12 (D+), фиксирани в чипа; описани са в usb_cdc.c.
 */

#endif /* BOARD_H */
