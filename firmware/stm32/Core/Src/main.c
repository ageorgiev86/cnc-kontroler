/*
 * CNC контролер — firmware за STM32H723ZG (изпълнителната част).
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Етап 0: стартиране, такт 400 MHz от вътрешния осцилатор HSI, кешове, MPU
 * и мигащ светодиод.
 * Етап 1: USB CDC конзола за съобщения при отстраняване на грешки.
 * Етап 2: генериране на стъпки с TIM15 + DMA за 6 оси (тест от конзолата).
 * Редът на прекъснатата програма се пази в backup SRAM (resume.c).
 * Следващите модули (енкодери, SPI към Linux платката, watchdog) се добавят
 * върху тази основа.
 */
#include "main.h"
#include "console.h"
#include "resume.h"
#include "stepgen.h"
#include "usb_cdc.h"

static void MPU_Config(void);
static void SystemClock_Config(void);
static void LED_Init(void);
static void LED_Set(int on);

int main(void)
{
  MPU_Config();
  SCB_EnableICache();
  SCB_EnableDCache();

  HAL_Init();
  SystemClock_Config();
  LED_Init();
  resume_init();
  usb_cdc_init();
  stepgen_init();

  uint32_t led_time = HAL_GetTick();
  int on = 0;
  for (;;) {
    console_poll();
    stepgen_service();

    /* светодиодът мига 2 пъти в секунда: главният цикъл не е забил */
    if (HAL_GetTick() - led_time >= 250u) {
      led_time += 250u;
      on = !on;
      LED_Set(on);
    }
  }
}

/*
 * Такт: HSI 64 MHz → PLL1 (M=4, N=50, P=2) = 400 MHz.
 *   SYSCLK 400 MHz, AXI/AHB 200 MHz, APB1–APB4 100 MHz.
 * Ниво на напрежението VOS1, 3 цикъла изчакване на flash (с резерв).
 * 550 MHz изискват VOS0; ще се включат, след като се провери кварцът (HSE).
 */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  if (HAL_PWREx_ConfigSupply(BOARD_SUPPLY) != HAL_OK) {
    Error_Handler();
  }
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
  }

  osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState            = RCC_HSI_DIV1;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState        = RCC_PLL_ON;
  osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  osc.PLL.PLLM            = 4;    /* 64 / 4 = 16 MHz вход */
  osc.PLL.PLLN            = 50;   /* VCO = 800 MHz */
  osc.PLL.PLLP            = 2;    /* 400 MHz */
  osc.PLL.PLLQ            = 8;    /* 100 MHz (периферия) */
  osc.PLL.PLLR            = 2;
  osc.PLL.PLLRGE          = RCC_PLL1VCIRANGE_3;
  osc.PLL.PLLVCOSEL       = RCC_PLL1VCOWIDE;
  osc.PLL.PLLFRACN        = 0;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    Error_Handler();
  }

  clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                       RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;
  clk.AHBCLKDivider  = RCC_HCLK_DIV2;
  clk.APB3CLKDivider = RCC_APB3_DIV2;
  clk.APB1CLKDivider = RCC_APB1_DIV2;
  clk.APB2CLKDivider = RCC_APB2_DIV2;
  clk.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) {
    Error_Handler();
  }
}

/*
 * MPU: забранява спекулативния достъп до неизползваното адресно пространство
 * (стандартна защита при Cortex-M7 с включен кеш) и прави RAM_D2 и backup
 * SRAM некеширани.
 */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef r = {0};

  HAL_MPU_Disable();
  r.Enable           = MPU_REGION_ENABLE;
  r.Number           = MPU_REGION_NUMBER0;
  r.BaseAddress      = 0x0;
  r.Size             = MPU_REGION_SIZE_4GB;
  r.SubRegionDisable = 0x87;
  r.TypeExtField     = MPU_TEX_LEVEL0;
  r.AccessPermission = MPU_REGION_NO_ACCESS;
  r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  r.IsShareable      = MPU_ACCESS_SHAREABLE;
  r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
  r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&r);

  /* RAM_D2 (буфери за DMA, .dma_buffer): без кеш, за да виждат процесорът и
   * DMA едни и същи данни без ръчно изчистване на кеша */
  r.Number           = MPU_REGION_NUMBER1;
  r.BaseAddress      = 0x30000000;
  r.Size             = MPU_REGION_SIZE_32KB;
  r.SubRegionDisable = 0x00;
  r.TypeExtField     = MPU_TEX_LEVEL1;
  r.AccessPermission = MPU_REGION_FULL_ACCESS;
  r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  r.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
  r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&r);

  /* backup SRAM (редът на прекъснатата програма): без кеш и без буфер за
   * запис, за да е записът в паметта още преди следващата инструкция */
  r.Number           = MPU_REGION_NUMBER2;
  r.BaseAddress      = 0x38800000;
  r.Size             = MPU_REGION_SIZE_4KB;
  r.SubRegionDisable = 0x00;
  r.TypeExtField     = MPU_TEX_LEVEL0;
  r.AccessPermission = MPU_REGION_FULL_ACCESS;
  r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  r.IsShareable      = MPU_ACCESS_SHAREABLE;
  r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
  r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&r);
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void LED_Init(void)
{
  GPIO_InitTypeDef g = {0};

  BOARD_LED_CLK_ENABLE();
  LED_Set(0);
  g.Pin   = BOARD_LED_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BOARD_LED_PORT, &g);
}

static void LED_Set(int on)
{
  GPIO_PinState level = (on ^ BOARD_LED_ACTIVE_LOW) ? GPIO_PIN_SET : GPIO_PIN_RESET;
  HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, level);
}

void Error_Handler(void)
{
  __disable_irq();
  for (;;) {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
  Error_Handler();
}
#endif
