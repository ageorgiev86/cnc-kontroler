/* Колелцето на мишката на Linux платката: чете се направо от /dev/input (драйверът на LVGL за libinput
 * не го подава) и превърта областта под курсора чрез ui_wheel(). */
#ifndef CNC_MOUSE_WHEEL_H
#define CNC_MOUSE_WHEEL_H

#include "lvgl.h"

void mouse_wheel_start(const char *path, lv_indev_t *mouse);

#endif
