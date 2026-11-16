/*
 * Собствените входове на STM32.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#include "inputs.h"
#include "main.h"

struct in_pin {
  GPIO_TypeDef *port;
  uint16_t pin;
};

/* редът съвпада с битовете в inputs.h */
static const struct in_pin pins[16] = {
  { BOARD_LIM1 }, { BOARD_LIM2 }, { BOARD_LIM3 }, { BOARD_LIM4 }, { BOARD_LIM5 }, { BOARD_LIM6 },
  { BOARD_HOME1 }, { BOARD_HOME2 }, { BOARD_HOME3 }, { BOARD_HOME4 }, { BOARD_HOME5 }, { BOARD_HOME6 },
  { BOARD_PROBE }, { BOARD_TOOLSETTER }, { BOARD_ESTOP }, { BOARD_DRV_FAULT },
};

void inputs_init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  g.Mode  = GPIO_MODE_INPUT;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    g.Pin = pins[i].pin;
    HAL_GPIO_Init(pins[i].port, &g);
  }
}

uint32_t inputs_read(void)
{
  uint32_t v = 0;

  for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    if ((pins[i].port->IDR & pins[i].pin) == 0u) {
      v |= 1u << i;
    }
  }
  return v;
}
