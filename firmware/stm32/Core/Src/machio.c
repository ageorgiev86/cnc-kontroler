/*
 * Изходи, шпиндел, MPG колело, аналогови входове и спиране при краен изключвател.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#include "machio.h"
#include "main.h"
#include "inputs.h"
#include "stepgen.h"
#include "homing.h"

#include <stddef.h>

struct out_pin {
  GPIO_TypeDef *port;
  uint16_t pin;
};

#ifdef BOARD_OUTPUTS
static const struct out_pin outs[] = BOARD_OUTPUTS;
static const unsigned n_outs = sizeof(outs) / sizeof(outs[0]);
#else
static const struct out_pin outs[1] = { { NULL, 0 } };
static const unsigned n_outs = 0;   /* изходите на основната платка са през 74HCT595 */
#endif

static uint32_t out_state;
static uint8_t  sp_dir;
static uint32_t prev_lim;

#ifdef BOARD_MPG_TIM
static TIM_HandleTypeDef htim_mpg;
#endif

#ifdef BOARD_AIN1_CHANNEL
static ADC_HandleTypeDef hadc;
static int      adc_busy;
static uint32_t adc_t;
static uint32_t ain_filt;           /* суровата стойност × 16 (плъзгаща средна) */
#endif

static void pin_out(GPIO_TypeDef *port, uint16_t pin)
{
  GPIO_InitTypeDef g = {0};

  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  g.Pin   = pin;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(port, &g);
}

static void apply_outputs(void)
{
  for (unsigned i = 0; i < n_outs; i++) {
    HAL_GPIO_WritePin(outs[i].port, outs[i].pin, (out_state >> i) & 1u ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
  HAL_GPIO_WritePin(BOARD_SPINDLE_EN, sp_dir == SPINDLE_CW ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BOARD_SPINDLE_DIR, sp_dir == SPINDLE_CCW ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

#ifdef BOARD_MPG_TIM
static void mpg_init(void)
{
  GPIO_InitTypeDef g = {0};
  TIM_Encoder_InitTypeDef e = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_PULLUP;
  g.Speed     = GPIO_SPEED_FREQ_LOW;
  g.Alternate = BOARD_MPG_AF;
  HAL_GPIO_Init(GPIOC, &g);

  BOARD_MPG_TIM_CLK_ENABLE();
  htim_mpg.Instance               = BOARD_MPG_TIM;
  htim_mpg.Init.Prescaler         = 0;
  htim_mpg.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim_mpg.Init.Period            = 0xFFFF;
  htim_mpg.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim_mpg.Init.RepetitionCounter = 0;
  htim_mpg.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  e.EncoderMode = TIM_ENCODERMODE_TI12;
  e.IC1Polarity = TIM_ICPOLARITY_RISING;
  e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  e.IC1Prescaler = TIM_ICPSC_DIV1;
  e.IC1Filter    = 10;              /* срещу смущения по кабела */
  e.IC2Polarity = TIM_ICPOLARITY_RISING;
  e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  e.IC2Prescaler = TIM_ICPSC_DIV1;
  e.IC2Filter    = 10;
  if (HAL_TIM_Encoder_Init(&htim_mpg, &e) != HAL_OK) {
    Error_Handler();
  }
  HAL_TIM_Encoder_Start(&htim_mpg, TIM_CHANNEL_ALL);
}
#endif

#ifdef BOARD_AIN1_CHANNEL
static void adc_init(void)
{
  RCC_PeriphCLKInitTypeDef pc = {0};
  GPIO_InitTypeDef g = {0};
  ADC_ChannelConfTypeDef c = {0};

  pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  pc.AdcClockSelection    = RCC_ADCCLKSOURCE_CLKP;     /* per_ck = HSI 64 MHz */
  if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) {
    Error_Handler();
  }
  __HAL_RCC_ADC12_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  g.Pin  = BOARD_AIN1_PIN;
  g.Mode = GPIO_MODE_ANALOG;
  g.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOARD_AIN1_PORT, &g);

  hadc.Instance                      = ADC1;
  hadc.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV4;   /* 16 MHz */
  hadc.Init.Resolution               = ADC_RESOLUTION_16B;
  hadc.Init.ScanConvMode             = ADC_SCAN_DISABLE;
  hadc.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait         = DISABLE;
  hadc.Init.ContinuousConvMode       = DISABLE;
  hadc.Init.NbrOfConversion          = 1;
  hadc.Init.DiscontinuousConvMode    = DISABLE;
  hadc.Init.ExternalTrigConv         = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;
  hadc.Init.LeftBitShift             = ADC_LEFTBITSHIFT_NONE;
  hadc.Init.OversamplingMode         = DISABLE;
  if (HAL_ADC_Init(&hadc) != HAL_OK) {
    Error_Handler();
  }
  (void)HAL_ADCEx_Calibration_Start(&hadc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
  c.Channel      = BOARD_AIN1_CHANNEL;
  c.Rank         = ADC_REGULAR_RANK_1;
  c.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;
  c.SingleDiff   = ADC_SINGLE_ENDED;
  c.OffsetNumber = ADC_OFFSET_NONE;
  if (HAL_ADC_ConfigChannel(&hadc, &c) != HAL_OK) {
    Error_Handler();
  }
}

static void adc_poll(void)
{
  uint32_t now = HAL_GetTick();

  if (!adc_busy) {
    if (now - adc_t >= 5u) {        /* 200 измервания в секунда */
      adc_t = now;
      if (HAL_ADC_Start(&hadc) == HAL_OK) {
        adc_busy = 1;
      }
    }
  } else if (__HAL_ADC_GET_FLAG(&hadc, ADC_FLAG_EOC)) {
    uint32_t raw = HAL_ADC_GetValue(&hadc);
    ain_filt = (ain_filt == 0u) ? raw * 16u : ain_filt - ain_filt / 16u + raw;
    adc_busy = 0;
  }
}
#endif

void machio_init(void)
{
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  for (unsigned i = 0; i < n_outs; i++) {
    pin_out(outs[i].port, outs[i].pin);
  }
  pin_out(BOARD_SPINDLE_EN);
  pin_out(BOARD_SPINDLE_DIR);
  out_state = 0;
  sp_dir    = SPINDLE_STOP;
#ifdef BOARD_MPG_TIM
  mpg_init();
#endif
#ifdef BOARD_AIN1_CHANNEL
  adc_init();
#endif
  prev_lim = inputs_read() & BOARD_LIM_MASK;
}

void machio_poll(void)
{
  struct stepgen_status st;
  uint32_t lim = inputs_read() & BOARD_LIM_MASK;

  /* краен изключвател: спиране при сработване (по фронта, за да може оста да се изведе след RESET) */
  if (lim & ~prev_lim) {
    homing_abort();
    stepgen_fault(STEPGEN_FAULT_LIMIT);
  }
  prev_lim = lim;

  /* при грешка всичко се изключва */
  stepgen_get_status(&st);
  if (st.fault != STEPGEN_FAULT_NONE && (out_state != 0u || sp_dir != SPINDLE_STOP)) {
    out_state = 0;
    sp_dir    = SPINDLE_STOP;
    apply_outputs();
  }
#ifdef BOARD_AIN1_CHANNEL
  adc_poll();
#endif
}

void machio_set_outputs(uint32_t value, uint32_t mask)
{
  struct stepgen_status st;

  stepgen_get_status(&st);
  if (st.fault != STEPGEN_FAULT_NONE) {
    return;                                   /* при грешка изходите остават изключени */
  }
  out_state = (out_state & ~mask) | (value & mask);
  if (n_outs < 32u) {
    out_state &= (1u << n_outs) - 1u;
  }
  apply_outputs();
}

void machio_spindle(uint8_t dir)
{
  struct stepgen_status st;

  stepgen_get_status(&st);
  if (st.fault != STEPGEN_FAULT_NONE || dir > SPINDLE_CCW) {
    dir = SPINDLE_STOP;
  }
  if (dir != sp_dir) {
    sp_dir = SPINDLE_STOP;                    /* без мигновено обръщане: първо и двете релета изключени */
    apply_outputs();
    sp_dir = dir;
    apply_outputs();
  }
}

uint32_t machio_outputs(void)
{
  return out_state | (sp_dir == SPINDLE_CW ? (1u << 30) : 0u) | (sp_dir == SPINDLE_CCW ? (1u << 31) : 0u);
}

uint32_t machio_pendant(void)
{
  uint32_t in = inputs_read();
  uint32_t p  = 0;

  if (in & IN_MPG_X) {
    p = 1;
  } else if (in & IN_MPG_Y) {
    p = 2;
  } else if (in & IN_MPG_Z) {
    p = 3;
  }
  if (in & IN_MPG_X100) {
    p |= 3u << 3;
  } else if (in & IN_MPG_X10) {
    p |= 2u << 3;
  } else if (in & IN_MPG_X1) {
    p |= 1u << 3;
  }
  if (in & IN_BTN_START) {
    p |= 1u << 6;
  }
  if (in & IN_BTN_STOP) {
    p |= 1u << 8;
  }
  return p;
}

int32_t machio_mpg_count(void)
{
#ifdef BOARD_MPG_TIM
  static uint16_t last;
  static int32_t total;
  uint16_t c = (uint16_t)__HAL_TIM_GET_COUNTER(&htim_mpg);
  total += (int16_t)(uint16_t)(c - last);     /* разширяване на 16-битовия брояч */
  last = c;
  return total;
#else
  return 0;
#endif
}

uint16_t machio_ain_mv(unsigned ch)
{
#ifdef BOARD_AIN1_CHANNEL
  if (ch == 0u) {
    uint32_t pin_mv = (ain_filt / 16u) * 3300u / 65535u;
    return (uint16_t)(pin_mv * BOARD_AIN1_SCALE_X1000 / 1000u);
  }
#endif
  (void)ch;
  return 0;
}
