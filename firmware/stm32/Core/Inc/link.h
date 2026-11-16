/*
 * Връзка с горното ниво по протокола от docs/spi-protokol.md.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * link_process() обработва един приет пакет и подготвя отговора. Не зависи
 * от преноса: сега пакетите идват по USB (за проба от макета), по-късно по SPI.
 * По USB пакетите вървят в същия поток като текстовата конзола: пакетът
 * започва с байтовете 43 4E („CN“), които не могат да са начало на команда.
 */
#ifndef LINK_H
#define LINK_H

#include <stddef.h>
#include <stdint.h>

#define LINK_PKT_SIZE      256u
#define LINK_MAGIC         0x4E43u          /* байтове 43 4E */
#define LINK_VERSION       1u
#define LINK_HDR_SIZE      12u
#define LINK_BODY_MAX      240u

/* флагове Linux → STM32 */
#define LINK_F_ESTOP_REQ   0x01u
#define LINK_F_RESET_REQ   0x02u
/* флагове STM32 → Linux */
#define LINK_F_FAULT       0x01u
#define LINK_F_ESTOP       0x02u
#define LINK_F_EVENT       0x04u

/* записи */
#define LINK_T_MOTION_SEGMENT 0x01u
#define LINK_T_IO_SET         0x02u
#define LINK_T_SPINDLE        0x03u
#define LINK_T_AIN            0x87u
#define LINK_AIN_SIZE         8u
#define LINK_T_HOME_START     0x05u
#define LINK_T_HOME_DONE      0x84u
#define LINK_HOME_START_SIZE  24u
#define LINK_HOME_DONE_SIZE   12u
#define LINK_T_AXIS_CONFIG    0x07u
#define LINK_T_MOTION_CTRL    0x08u
#define LINK_T_STATUS         0x81u
#define LINK_T_FAULT          0x83u

#define LINK_SEG_SIZE      56u
#define LINK_STATUS_SIZE   120u

/* MOTION_CTRL */
#define LINK_CTRL_HOLD     1u
#define LINK_CTRL_RESUME   2u
#define LINK_CTRL_ABORT    3u

/* състояние в STATUS */
#define LINK_ST_IDLE       1u
#define LINK_ST_READY      2u
#define LINK_ST_RUN        3u
#define LINK_ST_STOPPING   5u
#define LINK_ST_FAULT      6u
#define LINK_ST_ESTOP      7u

#define LINK_FAULT_BAD_CONFIG 8u
#define LINK_FAULT_VERSION    9u

/* по USB браузърът не държи 1 ms такт, затова времето е по-дълго от това по SPI (10 ms) */
#define LINK_USB_TIMEOUT_MS 300u

uint32_t link_crc32(const uint8_t *p, size_t n);

/* Проверява пакета (magic, версия, дължина, CRC). 0 = валиден. */
int link_check(const uint8_t pkt[LINK_PKT_SIZE]);

/* Обработва валиден пакет и пише отговора в tx (LINK_PKT_SIZE байта). */
void link_process(const uint8_t rx[LINK_PKT_SIZE], uint8_t tx[LINK_PKT_SIZE]);

/* --- пренос по USB (в същия поток като конзолата) --- */

/* 1, ако в момента се приема пакет: следващите байтове са за link_usb_byte() */
int link_usb_capturing(void);

/* Приема един байт от USB; при цял пакет отговаря по USB. */
void link_usb_byte(uint8_t b);

/* Вика се от главния цикъл: време за изчакване на връзката и на непълен пакет. */
void link_poll(void);

/* 1, ако горното ниво праща пакети (връзката е активна) */
int link_active(void);

/* --- общо за преносите --- */

/* валиден пакет по даден пренос: watchdog-ът на връзката тръгва с това време */
void link_rx_ok(int spi, uint32_t tmo_ms);
/* отхвърлен пакет (CRC, дължина, непълен): брои се в STATUS.rx_errors */
void link_rx_error(void);
/* отговор без приет пакет (първият по SPI) */
void link_idle_reply(uint8_t tx[LINK_PKT_SIZE]);
/* 1, ако връзката е по SPI (тогава пакетите по USB се пренебрегват) */
int link_via_spi(void);

/* --- пренос по SPI4 (slave, link_spi.c) --- */

#define LINK_SPI_TIMEOUT_MS 10u   /* docs/spi-protokol.md, раздел 6 */

void link_spi_init(void);
/* от главния цикъл: обработва приетия пакет и подготвя отговора за следващата транзакция */
void link_spi_poll(void);

#endif /* LINK_H */
