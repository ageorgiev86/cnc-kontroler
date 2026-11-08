/* Собствен DRM/KMS дисплей за LVGL: избор на режим и завъртане (виж drm_disp.c). */
#ifndef CNC_DRM_DISP_H
#define CNC_DRM_DISP_H

#include "lvgl.h"

/* want: "1920x1200", "auto" или NULL; rot_deg: 0, 90, 180, 270 */
lv_display_t *drm_disp_create(const char *dev, const char *want, int rot_deg);
void drm_disp_list(const char *dev);   /* конекторите и режимите им (за --modes) */
/* Намира истинския графичен изход сред /dev/dri/card*: със свързан HDMI/DVI, не simpledrm
 * (временният екран от U-Boot, който остава като отделна карта). Връща malloc низ или NULL. */
char *drm_disp_find(void);
void drm_disp_close(void);             /* връща конзолата */

#endif
