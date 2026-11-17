/*
 * Пренос на пакетите по SPI4 (STM32 е slave): docs/spi-protokol.md, раздели 1 и 7.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Всяка транзакция (NSS долу) е точно 256 байта в двете посоки. DMA приема пакета от Linux
 * и едновременно праща отговора на предишния. След края главният цикъл (link_spi_poll)
 * проверява пакета, обработва го с link_process() (същото като по USB), подготвя отговора
 * и пуска DMA отново. Докато DMA е пуснат, READY е 1: горното ниво може да го чете,
 * за да не започне транзакция преди отговорът да е готов (по избор).
 * Непълна транзакция (NSS се вдига по-рано) се изхвърля и DMA се пуска наново.
 */
#include "link.h"
#include "main.h"
#include "board.h"
#include "irq_prio.h"
#include "stm32h7xx_it.h"

#include <string.h>

static SPI_HandleTypeDef hspi;
static DMA_HandleTypeDef hdma_rx;
static DMA_HandleTypeDef hdma_tx;

static uint8_t rx_buf[LINK_PKT_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
static uint8_t tx_buf[LINK_PKT_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
static uint8_t pkt[LINK_PKT_SIZE];

static volatile int done;
static volatile int failed;
static int armed;
static uint32_t partial_ms;                      /* от кога NSS е горе при недовършен пакет */
static struct link_spi_stats st;

static void gpio_af(GPIO_TypeDef *port, uint32_t pin, uint32_t pull)
{
  GPIO_InitTypeDef g = {0};
  g.Pin       = pin;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = pull;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = BOARD_LINK_AF;
  HAL_GPIO_Init(port, &g);
}

static void ready(int on)
{
  HAL_GPIO_WritePin(BOARD_LINK_READY, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void arm(void)
{
  done   = 0;
  failed = 0;
  armed  = (HAL_SPI_TransmitReceive_DMA(&hspi, tx_buf, rx_buf, LINK_PKT_SIZE) == HAL_OK);
  partial_ms = 0;
  ready(armed);
}

static void dma_init(DMA_HandleTypeDef *h, DMA_Stream_TypeDef *stream, uint32_t req, uint32_t dir)
{
  h->Instance                 = stream;
  h->Init.Request             = req;
  h->Init.Direction           = dir;
  h->Init.PeriphInc           = DMA_PINC_DISABLE;
  h->Init.MemInc              = DMA_MINC_ENABLE;
  h->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  h->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  h->Init.Mode                = DMA_NORMAL;
  h->Init.Priority            = DMA_PRIORITY_HIGH;
  h->Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(h) != HAL_OK) {
    Error_Handler();
  }
}

static void out_init(GPIO_TypeDef *port, uint32_t pin)
{
  GPIO_InitTypeDef g = {0};
  g.Pin   = pin;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(port, &g);
}

void link_spi_init(void)
{
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_SPI4_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  gpio_af(BOARD_LINK_NSS, GPIO_PULLUP);           /* незакачен = не е избран */
  gpio_af(BOARD_LINK_SCK, GPIO_PULLDOWN);
  gpio_af(BOARD_LINK_MISO, GPIO_NOPULL);
  gpio_af(BOARD_LINK_MOSI, GPIO_PULLDOWN);

  out_init(BOARD_LINK_READY);
  ready(0);

  hspi.Instance                        = BOARD_LINK_SPI;
  hspi.Init.Mode                       = SPI_MODE_SLAVE;
  hspi.Init.Direction                  = SPI_DIRECTION_2LINES;
  hspi.Init.DataSize                   = SPI_DATASIZE_8BIT;
  hspi.Init.CLKPolarity                = SPI_POLARITY_LOW;      /* режим 0 */
  hspi.Init.CLKPhase                   = SPI_PHASE_1EDGE;
  hspi.Init.NSS                        = SPI_NSS_HARD_INPUT;
  hspi.Init.FirstBit                   = SPI_FIRSTBIT_MSB;
  hspi.Init.TIMode                     = SPI_TIMODE_DISABLE;
  hspi.Init.CRCCalculation             = SPI_CRCCALCULATION_DISABLE;
  hspi.Init.NSSPolarity                = SPI_NSS_POLARITY_LOW;
  hspi.Init.FifoThreshold              = SPI_FIFO_THRESHOLD_01DATA;
  hspi.Init.MasterKeepIOState          = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi.Init.IOSwap                     = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi) != HAL_OK) {
    Error_Handler();
  }

  dma_init(&hdma_rx, DMA2_Stream0, DMA_REQUEST_SPI4_RX, DMA_PERIPH_TO_MEMORY);
  dma_init(&hdma_tx, DMA2_Stream1, DMA_REQUEST_SPI4_TX, DMA_MEMORY_TO_PERIPH);
  __HAL_LINKDMA(&hspi, hdmarx, hdma_rx);
  __HAL_LINKDMA(&hspi, hdmatx, hdma_tx);

  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, IRQ_PRIO_SPI, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, IRQ_PRIO_SPI, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  HAL_NVIC_SetPriority(SPI4_IRQn, IRQ_PRIO_SPI, 0);
  HAL_NVIC_EnableIRQ(SPI4_IRQn);

  link_idle_reply(tx_buf);                        /* първата транзакция връща STATUS без команди */
  arm();
}

static void restart(void)
{
  (void)HAL_SPI_Abort(&hspi);
  st.partial++;
  link_rx_error();
  link_idle_reply(tx_buf);
  arm();
}

void link_spi_poll(void)
{
  if (!armed) {
    arm();
    return;
  }
  if (failed) {
    st.hal_err++;
    st.partial--;                                 /* restart() брои прекъсната; тази е грешка на SPI/DMA */
    restart();
    return;
  }
  if (done) {
    ready(0);
    memcpy(pkt, rx_buf, LINK_PKT_SIZE);
    st.xfers++;
    memcpy(st.last_hdr, pkt, sizeof st.last_hdr);
    if (link_check(pkt) == 0) {
      st.good++;
      st.last_ok_ms = HAL_GetTick();
      st.last_seq = (uint16_t)(pkt[4] | (pkt[5] << 8));
      link_process(pkt, tx_buf);
      link_rx_ok(1, LINK_SPI_TIMEOUT_MS);
    } else {
      st.bad++;
      link_rx_error();
      link_idle_reply(tx_buf);
    }
    arm();
    return;
  }
  /* NSS горе, а пакетът е започнат и недовършен: горното ниво е прекъснало транзакцията */
  if (HAL_GPIO_ReadPin(BOARD_LINK_NSS) == GPIO_PIN_SET &&
      __HAL_DMA_GET_COUNTER(&hdma_rx) != LINK_PKT_SIZE) {
    uint32_t now = HAL_GetTick();
    if (partial_ms == 0) {
      partial_ms = now ? now : 1u;
    } else if (now - partial_ms > 2u) {
      restart();
    }
  } else {
    partial_ms = 0;
  }
}

void link_spi_stats(struct link_spi_stats *s)
{
  *s = st;
  s->nss_low = HAL_GPIO_ReadPin(BOARD_LINK_NSS) == GPIO_PIN_RESET;
  s->armed = armed;
}

void link_spi_clear_stats(void)
{
  memset(&st, 0, sizeof st);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *h)
{
  if (h == &hspi) {
    done = 1;
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *h)
{
  if (h == &hspi) {
    failed = 1;
  }
}

void DMA2_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_rx);
}

void DMA2_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_tx);
}

void SPI4_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi);
}
