/*
 * Тестова 3-осна машина: модул FK723M1-ZGT6 + собствена тестова платка.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Компилира се с конфигурацията Test3 (cmake --preset Test3): CNC_BOARD=test3 → BOARD_TEST3.
 * Таблицата и схемите на входовете и изходите: docs/test-masina-3osi.md.
 *
 * Сигнали:
 *   - датчиците и бутоните са на 5 V, но само свалят пина към GND (механичен ключ или
 *     NPN с отворен колектор); пинът е с вътрешно издърпване към 3,3 V → 0 = активен;
 *   - драйверите 2HSS57 през 74ACT245 на 5 V, общ анод → импулсът е 0;
 *   - изходите (въздух, шпиндел) през ULN2803 към релетата и вентилите;
 *   - датчик за налягане 0–5 V през делител 12 kΩ / 20 kΩ към PC0.
 * Избягват се пиновете на флаш паметта (PF6–PF10, PG6) и на microSD (PC8–PC12, PD2).
 */
#ifndef BOARD_TEST3_H
#define BOARD_TEST3_H

#define BOARD_NAME            "тестова 3-осна машина (FK723M1)"

/* Светодиод на модула: PG7, свети при 0 */
#define BOARD_LED_PORT        GPIOG
#define BOARD_LED_PIN         GPIO_PIN_7
#define BOARD_LED_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define BOARD_LED_ACTIVE_LOW  1

#define BOARD_SUPPLY          PWR_LDO_SUPPLY

/* Стъпки и посока: X PD4/PD5, Y PD6/PD7, Z PD8/PD9 (PD10–PD15 за A, B, C остават свободни) */
#define BOARD_STEP_PORT         GPIOD
#define BOARD_STEP_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define BOARD_STEP_PINS  { GPIO_PIN_4, GPIO_PIN_6, GPIO_PIN_8, GPIO_PIN_10, GPIO_PIN_12, GPIO_PIN_14 }
#define BOARD_DIR_PINS   { GPIO_PIN_5, GPIO_PIN_7, GPIO_PIN_9, GPIO_PIN_11, GPIO_PIN_13, GPIO_PIN_15 }
#define BOARD_AXES_MAX          6
#define BOARD_STEP_ACTIVE_LOW   1                    /* 74ACT245 + общ анод: импулсът е 0 */

#define BOARD_DRV_EN            GPIOB, GPIO_PIN_11   /* ENA (още не се управлява) */
#define BOARD_DRV_FAULT         GPIOE, GPIO_PIN_3    /* ALM на трите драйвера успоредно, 0 = аларма */
#define BOARD_DRV_OE_PORT       GPIOE                /* ~OE на 74ACT245 (10 kΩ към 5 V), 0 = разрешени */
#define BOARD_DRV_OE_PIN        GPIO_PIN_9
#define BOARD_DRV_OE_N          BOARD_DRV_OE_PORT, BOARD_DRV_OE_PIN
#define BOARD_DRV_OE_CLK_ENABLE() __HAL_RCC_GPIOE_CLK_ENABLE()

/* Входове, 0 = активен */
#define BOARD_LIM1              GPIOG, GPIO_PIN_8    /* X */
#define BOARD_LIM2              GPIOG, GPIO_PIN_9    /* Y */
#define BOARD_LIM3              GPIOG, GPIO_PIN_10   /* Z */
#define BOARD_HOME1             GPIOG, GPIO_PIN_14   /* X */
#define BOARD_HOME2             GPIOG, GPIO_PIN_15   /* Y */
#define BOARD_HOME3             GPIOE, GPIO_PIN_10   /* Z (не PF6: флаш паметта) */
#define BOARD_PROBE             GPIOA, GPIO_PIN_7
#define BOARD_TOOLSETTER        GPIOB, GPIO_PIN_1
#define BOARD_ESTOP             GPIOC, GPIO_PIN_13   /* наблюдение; аварийният стоп е хардуерен */
#define BOARD_BTN_START         GPIOG, GPIO_PIN_0
#define BOARD_BTN_STOP          GPIOG, GPIO_PIN_1
#define BOARD_MPG_X             GPIOG, GPIO_PIN_2
#define BOARD_MPG_Y             GPIOG, GPIO_PIN_3
#define BOARD_MPG_Z             GPIOG, GPIO_PIN_4
#define BOARD_MPG_X1            GPIOF, GPIO_PIN_2
#define BOARD_MPG_X10           GPIOF, GPIO_PIN_3
#define BOARD_MPG_X100          GPIOF, GPIO_PIN_4

/* редът е битовете на STATUS.local_in (inputs.h); { NULL, 0 } = няма вход */
#define BOARD_INPUTS { \
  { BOARD_LIM1 }, { BOARD_LIM2 }, { BOARD_LIM3 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }, \
  { BOARD_HOME1 }, { BOARD_HOME2 }, { BOARD_HOME3 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }, \
  { BOARD_PROBE }, { BOARD_TOOLSETTER }, { BOARD_ESTOP }, { BOARD_DRV_FAULT }, \
  { BOARD_BTN_START }, { BOARD_BTN_STOP }, { BOARD_MPG_X }, { BOARD_MPG_Y }, { BOARD_MPG_Z }, \
  { BOARD_MPG_X1 }, { BOARD_MPG_X10 }, { BOARD_MPG_X100 } }
#define BOARD_IN_INVERT         0u                   /* битове, при които 1 на пина = активен (NC ключ) */
#define BOARD_LIM_MASK          0x07u                /* крайните изключватели, които спират машината */

/* MPG колело: TIM8 в encoder mode, A = PC6, B = PC7 (1 kΩ последователно) */
#define BOARD_MPG_TIM           TIM8
#define BOARD_MPG_TIM_CLK_ENABLE() __HAL_RCC_TIM8_CLK_ENABLE()
#define BOARD_MPG_AF            GPIO_AF3_TIM8
#define BOARD_MPG_A             GPIOC, GPIO_PIN_6
#define BOARD_MPG_B             GPIOC, GPIO_PIN_7

/* Изходи през ULN2803: бит 0 = OUT1 */
#define BOARD_OUTPUTS { { GPIOE, GPIO_PIN_0 } /* OUT1 въздух */, { GPIOE, GPIO_PIN_1 } /* OUT2 */, { GPIOE, GPIO_PIN_2 } /* OUT3 */ }

/* Шпиндел: релета FWD и REV (през ULN2803) */
#define BOARD_SPINDLE_DIR       GPIOC, GPIO_PIN_4    /* REV */
#define BOARD_SPINDLE_EN        GPIOC, GPIO_PIN_5    /* FWD */

/* Аналогов вход 1: датчик за налягане 0–5 V → делител 12 kΩ / 20 kΩ → PC0 (ADC1_INP10) */
#define BOARD_AIN1_PORT         GPIOC
#define BOARD_AIN1_PIN          GPIO_PIN_0
#define BOARD_AIN1_CHANNEL      ADC_CHANNEL_10
#define BOARD_AIN1_SCALE_X1000  1600u                /* (12 + 20) / 20: напрежение на датчика = на пина × 1,6 */

#endif /* BOARD_TEST3_H */
