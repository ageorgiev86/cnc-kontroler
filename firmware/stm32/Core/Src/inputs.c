/*
 * Собствените входове на STM32.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#include "inputs.h"
#include "main.h"

#include <stddef.h>

struct in_pin {
  GPIO_TypeDef *port;
  uint16_t pin;
};

/* редът съвпада с битовете в inputs.h; списъкът е в board.h / board_test3.h */
static const struct in_pin pins[] = BOARD_INPUTS;

#define N_PINS (sizeof(pins) / sizeof(pins[0]))

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
  for (unsigned i = 0; i < N_PINS; i++) {
    if (pins[i].port != NULL) {
      g.Pin = pins[i].pin;
      HAL_GPIO_Init(pins[i].port, &g);
    }
  }
}

uint32_t inputs_read(void)
{
  uint32_t v = 0;

  for (unsigned i = 0; i < N_PINS && i < 32u; i++) {
    if (pins[i].port != NULL && (pins[i].port->IDR & pins[i].pin) == 0u) {
      v |= 1u << i;
    }
  }
  return v ^ (BOARD_IN_INVERT);
}
