/*
 * Нива на прекъсванията (NVIC). По-малко число = по-висок приоритет.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Генераторът на стъпки е най-важен: закъснение там е грешка в движението.
 * USB е само за съобщения при отстраняване на грешки и е почти най-ниско.
 */
#ifndef IRQ_PRIO_H
#define IRQ_PRIO_H

#define IRQ_PRIO_STEPGEN   1   /* DMA на генератора на стъпки (половин/цял буфер) */
#define IRQ_PRIO_SPI       4   /* SPI връзката с Linux (резерв) */
#define IRQ_PRIO_USB      12   /* USB CDC */
/* SysTick (HAL_GetTick) е на TICK_INT_PRIORITY = 15 в stm32h7xx_hal_conf.h */

#endif /* IRQ_PRIO_H */
