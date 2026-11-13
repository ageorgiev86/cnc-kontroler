/* Мишката и тъчът на Linux платката при завъртян екран.
 * Мишката се чете направо от /dev/input: движението е по екрана, както го вижда човекът (и при 90°/270°),
 * колелцето превърта областта под курсора (ui_wheel). Драйверът на LVGL за libinput държи курсора в рамките
 * на незавъртения екран и не подава колелцето, затова не се ползва за мишката.
 * Тъчът остава през libinput, но точките му се обръщат от екрана на монитора към завъртяния. */
#ifndef CNC_MOUSE_EVDEV_H
#define CNC_MOUSE_EVDEV_H

#include "lvgl.h"

lv_indev_t *mouse_evdev_create(const char *path, lv_display_t *disp);
void touch_rotate_attach(lv_indev_t *touch);

#endif
