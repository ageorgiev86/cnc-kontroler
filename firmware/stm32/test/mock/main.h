/*
 * Заместители на HAL и CMSIS за проверка на собствения код на компютъра.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Съдържа само това, което ползва stepgen.c. Не е за firmware-а.
 */
#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
typedef int IRQn_Type;
#define DMA1_Stream0_IRQn 11

typedef struct { volatile uint32_t BSRR; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
extern GPIO_TypeDef mock_gpiod;
#define GPIOD (&mock_gpiod)
#define GPIO_PIN_4  0x0010u
#define GPIO_PIN_5  0x0020u
#define GPIO_PIN_6  0x0040u
#define GPIO_PIN_7  0x0080u
#define GPIO_PIN_8  0x0100u
#define GPIO_PIN_9  0x0200u
#define GPIO_PIN_10 0x0400u
#define GPIO_PIN_11 0x0800u
#define GPIO_PIN_12 0x1000u
#define GPIO_PIN_13 0x2000u
#define GPIO_PIN_14 0x4000u
#define GPIO_PIN_15 0x8000u
#define GPIO_MODE_OUTPUT_PP 1u
#define GPIO_NOPULL 0u
#define GPIO_SPEED_FREQ_HIGH 2u

typedef struct __DMA_HandleTypeDef {
  void *Instance;
  struct { uint32_t Request, Direction, PeriphInc, MemInc, PeriphDataAlignment,
           MemDataAlignment, Mode, Priority, FIFOMode; } Init;
  void (*XferHalfCpltCallback)(struct __DMA_HandleTypeDef *h);
  void (*XferCpltCallback)(struct __DMA_HandleTypeDef *h);
} DMA_HandleTypeDef;
#define DMA1_Stream0 ((void *)1)
#define DMA_REQUEST_TIM15_UP 106u
#define DMA_MEMORY_TO_PERIPH 1u
#define DMA_PINC_DISABLE 0u
#define DMA_MINC_ENABLE 1u
#define DMA_PDATAALIGN_WORD 2u
#define DMA_MDATAALIGN_WORD 2u
#define DMA_CIRCULAR 1u
#define DMA_PRIORITY_VERY_HIGH 3u
#define DMA_FIFOMODE_DISABLE 0u

typedef struct {
  void *Instance;
  struct { uint32_t Prescaler, CounterMode, Period, ClockDivision, RepetitionCounter,
           AutoReloadPreload; } Init;
} TIM_HandleTypeDef;
#define TIM15 ((void *)2)
#define TIM_COUNTERMODE_UP 0u
#define TIM_CLOCKDIVISION_DIV1 0u
#define TIM_AUTORELOAD_PRELOAD_ENABLE 1u
#define TIM_DMA_UPDATE 1u

typedef struct { volatile uint32_t CTRL, CYCCNT, LAR; } DWT_Type;
typedef struct { volatile uint32_t DEMCR; } CoreDebug_Type;
extern DWT_Type mock_dwt;
extern CoreDebug_Type mock_coredebug;
#define DWT (&mock_dwt)
#define CoreDebug (&mock_coredebug)
#define CoreDebug_DEMCR_TRCENA_Msk (1u << 24)
#define DWT_CTRL_CYCCNTENA_Msk 1u

#define __DMB() __sync_synchronize()
#define NVIC_DisableIRQ(n) ((void)(n))
#define NVIC_EnableIRQ(n) ((void)(n))
#define HAL_NVIC_SetPriority(n, p, s) ((void)(n), (void)(p), (void)(s))
#define HAL_NVIC_EnableIRQ(n) ((void)(n))
#define __HAL_RCC_DMA1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM15_CLK_ENABLE() ((void)0)
#define __HAL_TIM_ENABLE_DMA(h, d) ((void)(h), (void)(d))
#define __HAL_TIM_ENABLE(h) ((void)(h))

static inline void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g) { (void)p; (void)g; }
static inline HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *h) { (void)h; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *h, uint32_t s, uint32_t d, uint32_t n)
{ (void)h; (void)s; (void)d; (void)n; return HAL_OK; }
static inline void HAL_DMA_IRQHandler(DMA_HandleTypeDef *h) { (void)h; }
static inline HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *h) { (void)h; return HAL_OK; }
static inline uint32_t HAL_RCC_GetPCLK2Freq(void) { return 100000000u; }
static inline uint32_t HAL_RCC_GetHCLKFreq(void) { return 200000000u; }

void Error_Handler(void);

/* от board.h */
#define BOARD_STEP_PORT         GPIOD
#define BOARD_STEP_CLK_ENABLE() ((void)0)
#define BOARD_STEP_PINS  { GPIO_PIN_4, GPIO_PIN_6, GPIO_PIN_8, GPIO_PIN_10, GPIO_PIN_12, GPIO_PIN_14 }
#define BOARD_DIR_PINS   { GPIO_PIN_5, GPIO_PIN_7, GPIO_PIN_9, GPIO_PIN_11, GPIO_PIN_13, GPIO_PIN_15 }
#define BOARD_AXES_MAX          6
#define BOARD_STEP_ACTIVE_LOW   1
extern GPIO_TypeDef mock_gpioe;
#define GPIOE (&mock_gpioe)
#define GPIO_PIN_SET   1
#define GPIO_PIN_RESET 0
#define GPIO_SPEED_FREQ_LOW 0
#define BOARD_DRV_OE_PORT       GPIOE
#define BOARD_DRV_OE_PIN        GPIO_PIN_9
#define BOARD_DRV_OE_N          BOARD_DRV_OE_PORT, BOARD_DRV_OE_PIN
#define BOARD_DRV_OE_CLK_ENABLE() ((void)0)
static inline void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, int st) { (void)p; (void)pin; (void)st; }
/* тестът проверява ос X */
#define BOARD_AXIS0_STEP_PIN    GPIO_PIN_4
#define BOARD_AXIS0_DIR_PIN     GPIO_PIN_5

#endif /* MOCK_MAIN_H */
