/*
 * Прекъсвания на ядрото.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#include "main.h"
#include "stm32h7xx_it.h"

void NMI_Handler(void)        { for (;;) {} }
void HardFault_Handler(void)  { for (;;) {} }
void MemManage_Handler(void)  { for (;;) {} }
void BusFault_Handler(void)   { for (;;) {} }
void UsageFault_Handler(void) { for (;;) {} }
void SVC_Handler(void)        {}
void DebugMon_Handler(void)   {}
void PendSV_Handler(void)     {}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
