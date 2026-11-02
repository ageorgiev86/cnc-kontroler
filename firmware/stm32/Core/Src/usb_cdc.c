/*
 * USB CDC ACM (виртуален сериен порт) върху HAL_PCD.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * USB OTG_HS на STM32H723 работи с вградения Full Speed PHY (12 Mbit/s) на
 * PA11 (D−) и PA12 (D+). Тези пинове са фиксирани в чипа, не зависят от
 * платката. Тактът 48 MHz е от вътрешния HSI48, донастройван от CRS по
 * USB SOF пакетите, затова не е нужен кварц.
 *
 * Крайни точки:
 *   EP0        управление (64 B)
 *   EP1 OUT    данни от компютъра (bulk, 64 B)
 *   EP1 IN     данни към компютъра (bulk, 64 B)
 *   EP2 IN     известия на CDC (interrupt, 8 B; не се ползва, но е задължителна)
 *
 * DMA на USB е изключен: FIFO се пълни от процесора в прекъсването,
 * затова буферите могат да са в обикновената (кеширана) памет.
 */
#include "usb_cdc.h"
#include "main.h"
#include "irq_prio.h"
#include "stm32h7xx_it.h"

#include <string.h>

/* Идентификатори. 0x0483/0x5740 е виртуалният COM порт на ST; ползва се
 * само за разработката. За серийните машини трябва собствен VID/PID. */
#define USB_VID 0x0483u
#define USB_PID 0x5740u

#define EP0_SIZE     64u
#define EP_DATA_OUT  0x01u
#define EP_DATA_IN   0x81u
#define EP_NOTIFY    0x82u
#define DATA_SIZE    64u
#define NOTIFY_SIZE  8u

#define TX_RING_SIZE 4096u   /* степен на 2 */
#define RX_RING_SIZE 512u    /* степен на 2 */

PCD_HandleTypeDef hpcd_usb;

/* ---- дескриптори ---- */

static const uint8_t dev_desc[18] = {
  18, 0x01,                 /* bLength, DEVICE */
  0x00, 0x02,               /* USB 2.0 */
  0x02, 0x00, 0x00,         /* клас CDC (на ниво устройство) */
  EP0_SIZE,
  USB_VID & 0xFF, USB_VID >> 8,
  USB_PID & 0xFF, USB_PID >> 8,
  0x00, 0x01,               /* bcdDevice 1.00 */
  1, 2, 3,                  /* низове: производител, продукт, сериен номер */
  1                         /* една конфигурация */
};

#define CFG_TOTAL 67u
static const uint8_t cfg_desc[CFG_TOTAL] = {
  /* конфигурация */
  9, 0x02, CFG_TOTAL, 0, 2, 1, 0, 0x80, 50,      /* 2 интерфейса, захранване от USB, 100 mA */
  /* интерфейс 0: управление на CDC */
  9, 0x04, 0, 0, 1, 0x02, 0x02, 0x01, 0,
  5, 0x24, 0x00, 0x10, 0x01,                     /* Header, CDC 1.10 */
  5, 0x24, 0x01, 0x00, 1,                        /* Call Management */
  4, 0x24, 0x02, 0x02,                           /* ACM: line coding + control line state */
  5, 0x24, 0x06, 0, 1,                           /* Union: управление 0, данни 1 */
  7, 0x05, EP_NOTIFY, 0x03, NOTIFY_SIZE, 0, 16,  /* interrupt IN, 16 ms */
  /* интерфейс 1: данни */
  9, 0x04, 1, 0, 2, 0x0A, 0x00, 0x00, 0,
  7, 0x05, EP_DATA_OUT, 0x02, DATA_SIZE, 0, 0,   /* bulk OUT */
  7, 0x05, EP_DATA_IN, 0x02, DATA_SIZE, 0, 0,    /* bulk IN */
};

static const char *const str_ascii[] = {
  NULL,                          /* 0: езици (отделно) */
  "CNC kontroler",               /* 1: производител */
  "CNC MCU STM32H723 debug",     /* 2: продукт */
};

/* ---- състояние ---- */

enum ep0_state { EP0_IDLE, EP0_DATA_IN, EP0_DATA_OUT, EP0_STATUS_IN, EP0_STATUS_OUT };

static volatile enum ep0_state ep0_state;
static const uint8_t *ep0_ptr;
static uint32_t ep0_rem;
static uint32_t ep0_chunk;
static int ep0_zlp;
static uint8_t ep0_req;                         /* заявка, чакаща данни (OUT) */
static uint8_t ep0_buf[128] __attribute__((aligned(4)));

static uint8_t line_coding[7] = { 0x00, 0xC2, 0x01, 0x00, 0, 0, 8 }; /* 115200 8N1 */
static volatile uint8_t configured;
static volatile uint8_t dtr;

static uint8_t tx_ring[TX_RING_SIZE];
static volatile uint32_t tx_head;               /* пише главният цикъл */
static volatile uint32_t tx_tail;               /* пише прекъсването */
static volatile uint8_t tx_busy;
static uint8_t tx_zlp;
static uint8_t tx_pkt[DATA_SIZE] __attribute__((aligned(4)));
static volatile uint32_t tx_dropped;

static uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head;               /* пише прекъсването */
static volatile uint32_t rx_tail;               /* пише главният цикъл */
static volatile uint8_t rx_paused;
static uint8_t rx_pkt[DATA_SIZE] __attribute__((aligned(4)));

/* ---- EP0 ---- */

static void ep0_stall(void)
{
  HAL_PCD_EP_SetStall(&hpcd_usb, 0x80);
  HAL_PCD_EP_SetStall(&hpcd_usb, 0x00);
  ep0_state = EP0_IDLE;
}

static void ep0_status_in(void)
{
  ep0_state = EP0_STATUS_IN;
  HAL_PCD_EP_Transmit(&hpcd_usb, 0x00, NULL, 0);
}

/* Изпраща данни по EP0; HAL пуска по един пакет, затова продължава в DataIn. */
static void ep0_send(const uint8_t *data, uint32_t len, uint32_t wlength)
{
  if (len > wlength) {
    len = wlength;
  }
  ep0_ptr   = data;
  ep0_rem   = len;
  ep0_zlp   = (len > 0u) && (len < wlength) && ((len % EP0_SIZE) == 0u);
  ep0_chunk = (len > EP0_SIZE) ? EP0_SIZE : len;
  ep0_state = EP0_DATA_IN;
  HAL_PCD_EP_Transmit(&hpcd_usb, 0x00, (uint8_t *)data, ep0_chunk);
}

static uint32_t string_desc(uint8_t index)
{
  static const char hex[] = "0123456789ABCDEF";
  char serial[25];
  const char *s;
  uint32_t n = 0;

  if (index == 0) {
    ep0_buf[0] = 4;
    ep0_buf[1] = 0x03;
    ep0_buf[2] = 0x09;                          /* английски (САЩ) */
    ep0_buf[3] = 0x04;
    return 4;
  }
  if (index == 3) {                             /* сериен номер от уникалния номер на чипа */
    const uint32_t uid[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
    for (int w = 0; w < 3; w++) {
      for (int i = 0; i < 8; i++) {
        serial[w * 8 + i] = hex[(uid[w] >> (28 - 4 * i)) & 0xFu];
      }
    }
    serial[24] = '\0';
    s = serial;
  } else if (index < sizeof(str_ascii) / sizeof(str_ascii[0])) {
    s = str_ascii[index];
  } else {
    return 0;
  }
  while (s[n] != '\0' && (2u + 2u * (n + 1u)) <= sizeof(ep0_buf)) {
    ep0_buf[2 + 2 * n]     = (uint8_t)s[n];
    ep0_buf[2 + 2 * n + 1] = 0;
    n++;
  }
  ep0_buf[0] = (uint8_t)(2u + 2u * n);
  ep0_buf[1] = 0x03;
  return ep0_buf[0];
}

static void open_data_endpoints(void)
{
  HAL_PCD_EP_Open(&hpcd_usb, EP_DATA_OUT, DATA_SIZE, EP_TYPE_BULK);
  HAL_PCD_EP_Open(&hpcd_usb, EP_DATA_IN, DATA_SIZE, EP_TYPE_BULK);
  HAL_PCD_EP_Open(&hpcd_usb, EP_NOTIFY, NOTIFY_SIZE, EP_TYPE_INTR);
  rx_paused = 0;
  tx_busy   = 0;
  tx_zlp    = 0;
  tx_tail   = tx_head;                          /* старите съобщения не се пращат */
  HAL_PCD_EP_Receive(&hpcd_usb, EP_DATA_OUT, rx_pkt, DATA_SIZE);
  configured = 1;
}

static void close_data_endpoints(void)
{
  if (configured) {
    HAL_PCD_EP_Close(&hpcd_usb, EP_DATA_OUT);
    HAL_PCD_EP_Close(&hpcd_usb, EP_DATA_IN);
    HAL_PCD_EP_Close(&hpcd_usb, EP_NOTIFY);
  }
  configured = 0;
  dtr        = 0;
}

static void handle_standard(const uint8_t *s, uint16_t wvalue, uint16_t windex, uint16_t wlength)
{
  uint8_t recipient = s[0] & 0x1Fu;

  switch (s[1]) {
  case 0x06: {                                  /* GET_DESCRIPTOR */
    uint8_t type = (uint8_t)(wvalue >> 8);
    if (type == 0x01) {
      ep0_send(dev_desc, sizeof(dev_desc), wlength);
    } else if (type == 0x02) {
      ep0_send(cfg_desc, sizeof(cfg_desc), wlength);
    } else if (type == 0x03) {
      uint32_t n = string_desc((uint8_t)wvalue);
      if (n == 0) {
        ep0_stall();
      } else {
        ep0_send(ep0_buf, n, wlength);
      }
    } else {
      ep0_stall();                              /* вкл. DEVICE_QUALIFIER: устройството е само Full Speed */
    }
    break;
  }
  case 0x05:                                    /* SET_ADDRESS */
    HAL_PCD_SetAddress(&hpcd_usb, (uint8_t)(wvalue & 0x7Fu));
    ep0_status_in();
    break;
  case 0x09:                                    /* SET_CONFIGURATION */
    if (wvalue == 1) {
      if (!configured) {
        open_data_endpoints();
      }
    } else if (wvalue == 0) {
      close_data_endpoints();
    } else {
      ep0_stall();
      break;
    }
    ep0_status_in();
    break;
  case 0x08:                                    /* GET_CONFIGURATION */
    ep0_buf[0] = configured ? 1 : 0;
    ep0_send(ep0_buf, 1, wlength);
    break;
  case 0x00:                                    /* GET_STATUS */
    ep0_buf[0] = 0;
    ep0_buf[1] = 0;
    ep0_send(ep0_buf, 2, wlength);
    break;
  case 0x0A:                                    /* GET_INTERFACE */
    ep0_buf[0] = 0;
    ep0_send(ep0_buf, 1, wlength);
    break;
  case 0x0B:                                    /* SET_INTERFACE: няма алтернативни */
    ep0_status_in();
    break;
  case 0x01:                                    /* CLEAR_FEATURE */
    if (recipient == 2 && wvalue == 0) {        /* ENDPOINT_HALT */
      HAL_PCD_EP_ClrStall(&hpcd_usb, (uint8_t)windex);
    }
    ep0_status_in();
    break;
  case 0x03:                                    /* SET_FEATURE */
    if (recipient == 2 && wvalue == 0) {
      HAL_PCD_EP_SetStall(&hpcd_usb, (uint8_t)windex);
    }
    ep0_status_in();
    break;
  default:
    ep0_stall();
    break;
  }
}

static void handle_class(const uint8_t *s, uint16_t wvalue, uint16_t wlength)
{
  switch (s[1]) {
  case 0x20:                                    /* SET_LINE_CODING: следват 7 байта */
    if (wlength != sizeof(line_coding)) {
      ep0_stall();
      break;
    }
    ep0_req   = 0x20;
    ep0_state = EP0_DATA_OUT;
    HAL_PCD_EP_Receive(&hpcd_usb, 0x00, ep0_buf, wlength);
    break;
  case 0x21:                                    /* GET_LINE_CODING */
    ep0_send(line_coding, sizeof(line_coding), wlength);
    break;
  case 0x22:                                    /* SET_CONTROL_LINE_STATE */
    dtr = (wvalue & 1u) ? 1 : 0;
    ep0_status_in();
    break;
  case 0x23:                                    /* SEND_BREAK */
    ep0_status_in();
    break;
  default:
    ep0_stall();
    break;
  }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
  const uint8_t *s = (const uint8_t *)hpcd->Setup;
  uint16_t wvalue  = (uint16_t)(s[2] | (s[3] << 8));
  uint16_t windex  = (uint16_t)(s[4] | (s[5] << 8));
  uint16_t wlength = (uint16_t)(s[6] | (s[7] << 8));

  ep0_state = EP0_IDLE;
  switch (s[0] & 0x60u) {
  case 0x00:
    handle_standard(s, wvalue, windex, wlength);
    break;
  case 0x20:
    if ((s[0] & 0x1Fu) == 1u && windex == 0u) { /* до интерфейс 0 */
      handle_class(s, wvalue, wlength);
    } else {
      ep0_stall();
    }
    break;
  default:
    ep0_stall();
    break;
  }
}

/* ---- данни ---- */

/* Вика се в прекъсването на USB или със забранено прекъсване на USB. */
static void tx_start(void)
{
  uint32_t avail = tx_head - tx_tail;
  uint32_t n     = (avail > DATA_SIZE) ? DATA_SIZE : avail;

  if (!configured) {
    tx_busy = 0;
    return;
  }
  if (n == 0) {
    if (tx_zlp) {                               /* пълен пакет без продължение: край на прехвърлянето */
      tx_zlp  = 0;
      tx_busy = 1;
      HAL_PCD_EP_Transmit(&hpcd_usb, EP_DATA_IN, NULL, 0);
    } else {
      tx_busy = 0;
    }
    return;
  }
  for (uint32_t i = 0; i < n; i++) {
    tx_pkt[i] = tx_ring[(tx_tail + i) & (TX_RING_SIZE - 1u)];
  }
  __DMB();
  tx_tail = tx_tail + n;
  tx_zlp  = (n == DATA_SIZE) ? 1 : 0;
  tx_busy = 1;
  HAL_PCD_EP_Transmit(&hpcd_usb, EP_DATA_IN, tx_pkt, n);
}

static void rx_rearm(void)
{
  if (configured && (RX_RING_SIZE - (rx_head - rx_tail)) >= DATA_SIZE) {
    rx_paused = 0;
    HAL_PCD_EP_Receive(&hpcd_usb, EP_DATA_OUT, rx_pkt, DATA_SIZE);
  } else {
    rx_paused = 1;
  }
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  (void)hpcd;
  if (epnum == 0) {
    if (ep0_state == EP0_DATA_IN) {
      ep0_ptr += ep0_chunk;
      ep0_rem -= ep0_chunk;
      if (ep0_rem > 0) {
        ep0_chunk = (ep0_rem > EP0_SIZE) ? EP0_SIZE : ep0_rem;
        HAL_PCD_EP_Transmit(&hpcd_usb, 0x00, (uint8_t *)ep0_ptr, ep0_chunk);
      } else if (ep0_zlp) {
        ep0_zlp   = 0;
        ep0_chunk = 0;
        HAL_PCD_EP_Transmit(&hpcd_usb, 0x00, NULL, 0);
      } else {
        ep0_state = EP0_STATUS_OUT;             /* хостът потвърждава с празен OUT пакет */
        HAL_PCD_EP_Receive(&hpcd_usb, 0x00, NULL, 0);
      }
    } else if (ep0_state == EP0_STATUS_IN) {
      ep0_state = EP0_IDLE;
    }
  } else if (epnum == (EP_DATA_IN & 0x7Fu)) {
    tx_start();
  }
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  if (epnum == 0) {
    if (ep0_state == EP0_DATA_OUT) {
      if (ep0_req == 0x20 && HAL_PCD_EP_GetRxCount(hpcd, 0x00) == sizeof(line_coding)) {
        memcpy(line_coding, ep0_buf, sizeof(line_coding));
      }
      ep0_req = 0;
      ep0_status_in();
    } else if (ep0_state == EP0_STATUS_OUT) {
      ep0_state = EP0_IDLE;
    }
  } else if (epnum == EP_DATA_OUT) {
    uint32_t n = HAL_PCD_EP_GetRxCount(hpcd, EP_DATA_OUT);
    for (uint32_t i = 0; i < n; i++) {
      if ((rx_head - rx_tail) >= RX_RING_SIZE) {
        break;                                  /* не би трябвало: приема се само при място за цял пакет */
      }
      rx_ring[rx_head & (RX_RING_SIZE - 1u)] = rx_pkt[i];
      __DMB();
      rx_head = rx_head + 1u;
    }
    rx_rearm();
  }
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
  (void)hpcd;
  configured = 0;
  dtr        = 0;
  tx_busy    = 0;
  ep0_state  = EP0_IDLE;
  HAL_PCD_EP_Open(&hpcd_usb, 0x00, EP0_SIZE, EP_TYPE_CTRL);
  HAL_PCD_EP_Open(&hpcd_usb, 0x80, EP0_SIZE, EP_TYPE_CTRL);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
  (void)hpcd;
  dtr = 0;                                      /* кабелът е изваден или компютърът спи */
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
  (void)hpcd;
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
  (void)hpcd;
  configured = 0;
  dtr        = 0;
}

/* ---- MSP и прекъсване ---- */

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
  GPIO_InitTypeDef g = {0};

  if (hpcd->Instance != USB_OTG_HS) {
    return;
  }
  __HAL_RCC_GPIOA_CLK_ENABLE();
  g.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_NOPULL;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF10_OTG1_HS;
  HAL_GPIO_Init(GPIOA, &g);

  __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
  HAL_NVIC_SetPriority(OTG_HS_IRQn, IRQ_PRIO_USB, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
}

void OTG_HS_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_usb);
}

/* ---- публични функции ---- */

static void usb_clock_init(void)
{
  RCC_OscInitTypeDef osc       = {0};
  RCC_PeriphCLKInitTypeDef clk = {0};
  RCC_CRSInitTypeDef crs       = {0};

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  osc.HSI48State     = RCC_HSI48_ON;
  osc.PLL.PLLState   = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    Error_Handler();
  }
  clk.PeriphClockSelection = RCC_PERIPHCLK_USB;
  clk.UsbClockSelection    = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    Error_Handler();
  }

  /* CRS донастройва HSI48 по SOF пакетите (на всяка 1 ms) */
  __HAL_RCC_CRS_CLK_ENABLE();
  crs.Prescaler             = RCC_CRS_SYNC_DIV1;
  crs.Source                = RCC_CRS_SYNC_SOURCE_USB1;
  crs.Polarity              = RCC_CRS_SYNC_POLARITY_RISING;
  crs.ReloadValue           = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000u, 1000u);
  crs.ErrorLimitValue       = 34;
  crs.HSI48CalibrationValue = 32;
  HAL_RCCEx_CRSConfig(&crs);

  HAL_PWREx_EnableUSBVoltageDetector();
}

void usb_cdc_init(void)
{
  usb_clock_init();

  hpcd_usb.Instance                     = USB_OTG_HS;
  hpcd_usb.Init.dev_endpoints           = 9;
  hpcd_usb.Init.speed                   = PCD_SPEED_FULL;
  hpcd_usb.Init.dma_enable              = DISABLE;
  hpcd_usb.Init.phy_itface              = PCD_PHY_EMBEDDED;
  hpcd_usb.Init.Sof_enable              = DISABLE;
  hpcd_usb.Init.low_power_enable        = DISABLE;
  hpcd_usb.Init.lpm_enable              = DISABLE;
  hpcd_usb.Init.battery_charging_enable = DISABLE;
  hpcd_usb.Init.vbus_sensing_enable     = DISABLE;  /* VBUS не е непременно на PA9 */
  hpcd_usb.Init.use_dedicated_ep1       = DISABLE;
  hpcd_usb.Init.use_external_vbus       = DISABLE;
  if (HAL_PCD_Init(&hpcd_usb) != HAL_OK) {
    Error_Handler();
  }

  /* FIFO (в 32-битови думи, общо до 1024): приемане 512 B, EP0 256 B, EP1 512 B, EP2 64 B */
  HAL_PCDEx_SetRxFiFo(&hpcd_usb, 0x80);
  HAL_PCDEx_SetTxFiFo(&hpcd_usb, 0, 0x40);
  HAL_PCDEx_SetTxFiFo(&hpcd_usb, 1, 0x80);
  HAL_PCDEx_SetTxFiFo(&hpcd_usb, 2, 0x10);

  HAL_PCD_Start(&hpcd_usb);
}

int usb_cdc_connected(void)
{
  return configured && dtr;
}

size_t usb_cdc_write(const void *data, size_t len)
{
  const uint8_t *p = data;
  uint32_t space;
  size_t n;

  if (!usb_cdc_connected()) {
    return 0;
  }
  space = TX_RING_SIZE - (tx_head - tx_tail);
  n     = (len > space) ? space : len;
  if (n < len) {
    tx_dropped += (uint32_t)(len - n);
  }
  for (size_t i = 0; i < n; i++) {
    tx_ring[(tx_head + i) & (TX_RING_SIZE - 1u)] = p[i];
  }
  __DMB();
  tx_head = tx_head + (uint32_t)n;

  /* стартира изпращането, ако USB не праща в момента */
  NVIC_DisableIRQ(OTG_HS_IRQn);
  if (!tx_busy) {
    tx_start();
  }
  NVIC_EnableIRQ(OTG_HS_IRQn);
  return n;
}

size_t usb_cdc_read(void *data, size_t max)
{
  uint8_t *p = data;
  size_t n   = 0;

  while (n < max && rx_tail != rx_head) {
    p[n++] = rx_ring[rx_tail & (RX_RING_SIZE - 1u)];
    __DMB();
    rx_tail = rx_tail + 1u;
  }
  if (rx_paused) {
    NVIC_DisableIRQ(OTG_HS_IRQn);
    if (rx_paused) {
      rx_rearm();
    }
    NVIC_EnableIRQ(OTG_HS_IRQn);
  }
  return n;
}

uint32_t usb_cdc_dropped(void)
{
  return tx_dropped;
}
