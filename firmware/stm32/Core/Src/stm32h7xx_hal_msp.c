/*
 * Начална настройка на периферията за HAL (MSP).
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#include "main.h"

void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
}
