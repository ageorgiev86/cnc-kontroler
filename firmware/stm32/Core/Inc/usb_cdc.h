/*
 * USB CDC (виртуален сериен порт) за съобщения при отстраняване на грешки.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Собствена реализация върху HAL_PCD (USB OTG_HS с вградения Full Speed PHY,
 * пинове PA11/PA12). Windows 10 го разпознава без драйвер (usbser.sys).
 *
 * Не блокира: ако няма свързан терминал или буферът е пълен, излишното
 * се изхвърля. Затова може да се вика спокойно от главния цикъл.
 * НЕ се вика от прекъсвания с по-висок приоритет от USB.
 */
#ifndef USB_CDC_H
#define USB_CDC_H

#include <stddef.h>
#include <stdint.h>

void usb_cdc_init(void);

/* 1, ако терминалът на компютъра е отворил порта (сигнал DTR) */
int usb_cdc_connected(void);

/* Записва до len байта в буфера за изпращане; връща колко са приети. */
size_t usb_cdc_write(const void *data, size_t len);

/* Чете до max приети байта; връща колко са прочетени (0, ако няма). */
size_t usb_cdc_read(void *data, size_t max);

/* Брой изхвърлени байтове заради пълен буфер (за диагностика). */
uint32_t usb_cdc_dropped(void);

#endif /* USB_CDC_H */
