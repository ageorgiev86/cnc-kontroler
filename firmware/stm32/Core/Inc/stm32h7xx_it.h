/*
 * Прекъсвания на ядрото.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#ifndef STM32H7XX_IT_H
#define STM32H7XX_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void OTG_HS_IRQHandler(void);         /* usb_cdc.c */
void DMA1_Stream0_IRQHandler(void);   /* stepgen.c */
void DMA2_Stream0_IRQHandler(void);   /* link_spi.c: SPI4 RX */
void DMA2_Stream1_IRQHandler(void);   /* link_spi.c: SPI4 TX */
void SPI4_IRQHandler(void);           /* link_spi.c */

#endif /* STM32H7XX_IT_H */
