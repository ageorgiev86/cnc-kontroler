/* Клавиатура от /dev/input (виж kbd_evdev.c). */
#ifndef CNC_KBD_EVDEV_H
#define CNC_KBD_EVDEV_H

#include "lvgl.h"

lv_indev_t *kbd_evdev_create(char **paths, int n);

#endif
