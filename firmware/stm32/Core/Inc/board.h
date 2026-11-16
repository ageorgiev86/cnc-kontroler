/*
 * Всичко, което зависи от основната платка на контролера (STM32H723ZGT6 директно на платката).
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Схемата и платката: hardware/kicad (osnovna_platka.py, platka.py). Процесорната част е
 * по схемата на WAVGAT LXB743ZI-P1:
 *   Кварци        25 MHz (PH0/PH1), 32,768 kHz (PC14/PC15)
 *   Ядро          LDO: 2 × 4,7 µF на VCAP (изводи 71 и 106)
 *   Захранване    24 V → LM2596S-5.0 → AP2112K-3.3; при програмиране 5 V и от USB
 *   Светодиод     PG7, свети при 0
 *   Бутони        BOOT0 (10 kΩ към маса), RESET
 *   USB-B         PA11 (D−), PA12 (D+); SWD гнездо: 3V3, SWCLK, GND, SWDIO, NRST
 *
 * Обърнати сигнали (заради схемата):
 *   - енкодерите минават през Шмитов инвертор 74LVC14 → полярност „обратна“ на входа на таймера;
 *   - пробник, toolsetter, авариен стоп, LIM и HOME минават през оптрон → 0 = активен;
 *   - драйверите са свързани с общ анод (+5V) → изход 0 включва оптрона в драйвера.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32h7xx_hal.h"

#if defined(BOARD_TEST3)
#include "board_test3.h"   /* тестовата 3-осна машина (конфигурация Test3) */
#else

#define BOARD_NAME            "основна платка"

/* Светодиод: 3,3 V → 1 kΩ → светодиод → PG7, свети при 0 */
#define BOARD_LED_PORT        GPIOG
#define BOARD_LED_PIN         GPIO_PIN_7
#define BOARD_LED_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define BOARD_LED_ACTIVE_LOW  1

/* Захранване на ядрото: LDO */
#define BOARD_SUPPLY          PWR_LDO_SUPPLY

/*
 * ---------------------------------------------------------------------------
 * Разпределение на пиновете (таблицата: docs/stm32-pinove.md).
 * Сигнал, който е само GPIO, е двойка „порт, пин“: HAL_GPIO_ReadPin(BOARD_PROBE).
 * Каналите 1–6 са общи: кой е X, Y1, Y2, Z, A, C се избира в Настройки.
 * ---------------------------------------------------------------------------
 */

/* Стъпки и посока: всички на порт D (DMA пише в един регистър BSRR). Канал 1–6. */
#define BOARD_STEP_PORT         GPIOD
#define BOARD_STEP_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define BOARD_STEP_PINS  { GPIO_PIN_4, GPIO_PIN_6, GPIO_PIN_8, GPIO_PIN_10, GPIO_PIN_12, GPIO_PIN_14 }
#define BOARD_DIR_PINS   { GPIO_PIN_5, GPIO_PIN_7, GPIO_PIN_9, GPIO_PIN_11, GPIO_PIN_13, GPIO_PIN_15 }
#define BOARD_AXES_MAX          6
#define BOARD_STEP_ACTIVE_LOW   1                    /* общ анод: импулсът е 0 */

#define BOARD_DRV_EN            GPIOB, GPIO_PIN_11   /* разрешение (ENA) на драйверите, изход */
#define BOARD_DRV_FAULT         GPIOE, GPIO_PIN_3    /* аларма (ALM) от драйвер, вход, 0 = аларма */
#define BOARD_DRV_OE_PORT       GPIOE                /* 0 = изходите на буферите 74ACT541 са разрешени */
#define BOARD_DRV_OE_PIN        GPIO_PIN_9
#define BOARD_DRV_OE_N          BOARD_DRV_OE_PORT, BOARD_DRV_OE_PIN
#define BOARD_DRV_OE_CLK_ENABLE() __HAL_RCC_GPIOE_CLK_ENABLE()

/* Енкодери (TTL, A и B): таймер в encoder mode, A = CH1, B = CH2 */
#define BOARD_ENC1_TIM          TIM5                 /* 32 бита */
#define BOARD_ENC1_AF           GPIO_AF2_TIM5
#define BOARD_ENC1_A            GPIOA, GPIO_PIN_0
#define BOARD_ENC1_B            GPIOA, GPIO_PIN_1

#define BOARD_ENC2_TIM          TIM2                 /* 32 бита */
#define BOARD_ENC2_AF           GPIO_AF1_TIM2
#define BOARD_ENC2_A            GPIOA, GPIO_PIN_15
#define BOARD_ENC2_B            GPIOB, GPIO_PIN_3

#define BOARD_ENC3_TIM          TIM23                /* 32 бита */
#define BOARD_ENC3_AF           GPIO_AF13_TIM23
#define BOARD_ENC3_A            GPIOF, GPIO_PIN_0
#define BOARD_ENC3_B            GPIOF, GPIO_PIN_1

#define BOARD_ENC4_TIM          TIM3                 /* 16 бита */
#define BOARD_ENC4_AF           GPIO_AF2_TIM3
#define BOARD_ENC4_A            GPIOB, GPIO_PIN_4
#define BOARD_ENC4_B            GPIOB, GPIO_PIN_5

#define BOARD_ENC5_TIM          TIM4                 /* 16 бита */
#define BOARD_ENC5_AF           GPIO_AF2_TIM4
#define BOARD_ENC5_A            GPIOB, GPIO_PIN_6
#define BOARD_ENC5_B            GPIOB, GPIO_PIN_7

#define BOARD_ENC6_TIM          TIM1                 /* 16 бита */
#define BOARD_ENC6_AF           GPIO_AF1_TIM1
#define BOARD_ENC6_A            GPIOA, GPIO_PIN_8
#define BOARD_ENC6_B            GPIOA, GPIO_PIN_9

#define BOARD_ENC_SP_TIM        TIM24                /* шпиндел, 32 бита */
#define BOARD_ENC_SP_AF         GPIO_AF14_TIM24
#define BOARD_ENC_SP_A          GPIOF, GPIO_PIN_11
#define BOARD_ENC_SP_B          GPIOF, GPIO_PIN_12

/* MPG пулт: отделен модул по RS-485 (THVD1450), гнездо 2 × 3: +24V, 0V, A, B, авариен стоп ×2.
 * Пултът праща на всяка 1 ms колело, ос, ×1/×10/×100, Старт, Пауза, Стоп; без пакети 20 ms → плавно спиране. */
#define BOARD_MPG_UART          UART4
#define BOARD_MPG_AF            GPIO_AF8_UART4
#define BOARD_MPG_RX            GPIOD, GPIO_PIN_0
#define BOARD_MPG_TX            GPIOD, GPIO_PIN_1
#define BOARD_MPG_DE            GPIOD, GPIO_PIN_3    /* 1 = предаване (DE и ~RE заедно) */

/* Крайни изключватели и HOME, канал 1–6 (през оптрон, 0 = активен) */
#define BOARD_LIM1              GPIOG, GPIO_PIN_8
#define BOARD_LIM2              GPIOG, GPIO_PIN_9
#define BOARD_LIM3              GPIOG, GPIO_PIN_10
#define BOARD_LIM4              GPIOG, GPIO_PIN_11
#define BOARD_LIM5              GPIOG, GPIO_PIN_12
#define BOARD_LIM6              GPIOG, GPIO_PIN_13
#define BOARD_HOME1             GPIOG, GPIO_PIN_14
#define BOARD_HOME2             GPIOG, GPIO_PIN_15
#define BOARD_HOME3             GPIOF, GPIO_PIN_6
#define BOARD_HOME4             GPIOF, GPIO_PIN_7
#define BOARD_HOME5             GPIOF, GPIO_PIN_8
#define BOARD_HOME6             GPIOF, GPIO_PIN_9

/* Бързи входове (по EXTI, през оптрон, 0 = активен) */
#define BOARD_PROBE             GPIOA, GPIO_PIN_7    /* EXTI7 */
#define BOARD_TOOLSETTER        GPIOB, GPIO_PIN_1    /* EXTI1 */
#define BOARD_ESTOP             GPIOC, GPIO_PIN_13   /* EXTI13, наблюдение на веригата */

/* Връзка с Linux: SPI4 slave */
#define BOARD_LINK_SPI          SPI4
#define BOARD_LINK_AF           GPIO_AF5_SPI4
#define BOARD_LINK_NSS          GPIOE, GPIO_PIN_11
#define BOARD_LINK_SCK          GPIOE, GPIO_PIN_12
#define BOARD_LINK_MISO         GPIOE, GPIO_PIN_13
#define BOARD_LINK_MOSI         GPIOE, GPIO_PIN_14
#define BOARD_LINK_READY        GPIOE, GPIO_PIN_15   /* изход: готов за пакет */
#define BOARD_LINK_IRQ          GPIOE, GPIO_PIN_10   /* изход: спешно събитие */

/* Входове и изходи 8 + 8 (и разширителните модули верижно): SPI2 master през ISO7761 */
#define BOARD_IO_SPI            SPI2
#define BOARD_IO_AF             GPIO_AF5_SPI2
#define BOARD_IO_SCK            GPIOB, GPIO_PIN_13
#define BOARD_IO_MISO           GPIOB, GPIO_PIN_14   /* от 74HCT165 */
#define BOARD_IO_MOSI           GPIOB, GPIO_PIN_15   /* към 74HCT595 */
#define BOARD_IO_LATCH          GPIOB, GPIO_PIN_12   /* 74HCT595 RCLK */
#define BOARD_IO_LOAD           GPIOE, GPIO_PIN_7    /* 74HCT165 ~PL */
#define BOARD_IO_OE             GPIOE, GPIO_PIN_8    /* 74HCT595 ~OE, 0 = изходите разрешени */

/* Изходите са свързани по физическия ред на изводите (без кръстосване), а номерацията е естествена:
 * клема k = Q_A..Q_H на 74HCT595 (изход 1 = Q_A). Еднакво на основната платка и на модулите.
 * Входовете: клема k → бит D на 74HCT165 (по физическия ред на изводите на чипа). */
#define BOARD_IN_MAP_MAIN       { 6, 7, 0, 1, 2, 3, 4, 5 }   /* основна платка: клема 1..8 → D */
#define BOARD_IN_MAP_MODULE     { 2, 3, 4, 5, 6, 7, 0, 1 }   /* разширителен модул: клема 1..8 → D */

/* Шпиндел: 0–10 V (PWM → филтър → LM358), FWD и REV през оптрон */
#define BOARD_SPINDLE_PWM_TIM   TIM17                /* CH1 */
#define BOARD_SPINDLE_PWM_AF    GPIO_AF1_TIM17
#define BOARD_SPINDLE_PWM       GPIOB, GPIO_PIN_9
#define BOARD_SPINDLE_DIR       GPIOC, GPIO_PIN_4    /* REV на регулатора */
#define BOARD_SPINDLE_EN        GPIOC, GPIO_PIN_5    /* FWD на регулатора */

/* Свободни (настройват се като аналогови): PA2–PA6, PA10, PB0, PB2, PB8, PB10, PC0–PC3, PC6–PC12,
 * PD2, PE0–PE2, PE4–PE6, PF2–PF5, PF10, PF13–PF15, PG0–PG6. USB: PA11/PA12 (usb_cdc.c). */

/* редът е битовете на STATUS.local_in (inputs.h) */
#define BOARD_INPUTS { \
  { BOARD_LIM1 }, { BOARD_LIM2 }, { BOARD_LIM3 }, { BOARD_LIM4 }, { BOARD_LIM5 }, { BOARD_LIM6 }, \
  { BOARD_HOME1 }, { BOARD_HOME2 }, { BOARD_HOME3 }, { BOARD_HOME4 }, { BOARD_HOME5 }, { BOARD_HOME6 }, \
  { BOARD_PROBE }, { BOARD_TOOLSETTER }, { BOARD_ESTOP }, { BOARD_DRV_FAULT } }
#define BOARD_IN_INVERT         0u
#define BOARD_LIM_MASK          0x3Fu
/* изходите са през 74HCT595 (SPI2), а не направо: BOARD_OUTPUTS не е дефиниран */

#endif /* BOARD_TEST3 */

#endif /* BOARD_H */
