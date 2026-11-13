/* Мишка от /dev/input/event* и завъртане на точките на тъча (виж mouse_evdev.h). */
#include "mouse_evdev.h"
#include "ui.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* размерите на монитора (незавъртени) и на екрана, както го вижда човекът */
static void sizes(lv_display_t *d, int32_t *pw, int32_t *ph, int32_t *lw, int32_t *lh)
{
    *lw = lv_display_get_horizontal_resolution(d);
    *lh = lv_display_get_vertical_resolution(d);
    lv_display_rotation_t r = lv_display_get_rotation(d);
    bool sw = r == LV_DISPLAY_ROTATION_90 || r == LV_DISPLAY_ROTATION_270;
    *pw = sw ? *lh : *lw;
    *ph = sw ? *lw : *lh;
}

/* точка от монитора → точка на завъртения екран (обратното на lv_display_rotate_area) */
static void phys_to_log(lv_display_t *d, lv_point_t *p)
{
    int32_t pw, ph, lw, lh;
    sizes(d, &pw, &ph, &lw, &lh);
    int32_t x = p->x, y = p->y;
    switch (lv_display_get_rotation(d)) {
    case LV_DISPLAY_ROTATION_90: p->x = ph - 1 - y; p->y = x; break;
    case LV_DISPLAY_ROTATION_180: p->x = pw - 1 - x; p->y = ph - 1 - y; break;
    case LV_DISPLAY_ROTATION_270: p->x = y; p->y = pw - 1 - x; break;
    default: break;
    }
}

/* ---------- мишка ---------- */
static struct {
    int fd;
    lv_display_t *disp;
    lv_indev_t *in;
    int32_t x, y;               /* курсорът по завъртения екран */
    bool down;
    bool abs;                   /* устройство с абсолютни координати (някои USB тъч панели се представят като мишка) */
    int32_t ax, ay;
    struct input_absinfo ix, iy;
} MS = {.fd = -1};

/* леко ускорение: бавното движение е точно, бързото прекосява екрана */
static int32_t accel(int v)
{
    int a = abs(v);
    return (int32_t)(a > 3 ? v * (1.0f + (a > 20 ? 20 : a) / 12.0f) : v);
}

static void clamp_pos(void)
{
    int32_t lw = lv_display_get_horizontal_resolution(MS.disp), lh = lv_display_get_vertical_resolution(MS.disp);
    if (MS.x < 0) MS.x = 0;
    if (MS.y < 0) MS.y = 0;
    if (MS.x > lw - 1) MS.x = lw - 1;
    if (MS.y > lh - 1) MS.y = lh - 1;
}

static void mouse_read(lv_indev_t *in, lv_indev_data_t *data)
{
    struct input_event ev[64];
    ssize_t r;
    int notches = 0;
    bool abs_moved = false;
    while ((r = read(MS.fd, ev, sizeof ev)) > 0) {
        for (int k = 0; k < (int)(r / (ssize_t)sizeof ev[0]); k++) {
            const struct input_event *e = &ev[k];
            if (e->type == EV_REL) {
                if (e->code == REL_X) MS.x += accel(e->value);
                else if (e->code == REL_Y) MS.y += accel(e->value);
                else if (e->code == REL_WHEEL) notches += e->value;   /* + = нагоре */
            } else if (e->type == EV_ABS && MS.abs) {
                if (e->code == ABS_X || e->code == ABS_MT_POSITION_X) { MS.ax = e->value; abs_moved = true; }
                else if (e->code == ABS_Y || e->code == ABS_MT_POSITION_Y) { MS.ay = e->value; abs_moved = true; }
            } else if (e->type == EV_KEY && (e->code == BTN_LEFT || e->code == BTN_TOUCH)) {
                MS.down = e->value != 0;
            }
        }
    }
    if (abs_moved) {   /* абсолютните координати са по монитора: мащаб и завъртане */
        int32_t pw, ph, lw, lh;
        sizes(MS.disp, &pw, &ph, &lw, &lh);
        int32_t sx = MS.ix.maximum - MS.ix.minimum, sy = MS.iy.maximum - MS.iy.minimum;
        lv_point_t p = {sx > 0 ? (int32_t)((int64_t)(MS.ax - MS.ix.minimum) * (pw - 1) / sx) : 0,
                        sy > 0 ? (int32_t)((int64_t)(MS.ay - MS.iy.minimum) * (ph - 1) / sy) : 0};
        phys_to_log(MS.disp, &p);
        MS.x = p.x;
        MS.y = p.y;
    }
    clamp_pos();
    data->point.x = MS.x;
    data->point.y = MS.y;
    data->state = MS.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (notches) ui_wheel(MS.x, MS.y, notches);
}

lv_indev_t *mouse_evdev_create(const char *path, lv_display_t *disp)
{
    if (!path || MS.fd >= 0) return NULL;
    MS.fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (MS.fd < 0) { fprintf(stderr, "Мишка %s: не може да се отвори\n", path); return NULL; }
    MS.disp = disp;
    MS.abs = ioctl(MS.fd, EVIOCGABS(ABS_X), &MS.ix) == 0 && ioctl(MS.fd, EVIOCGABS(ABS_Y), &MS.iy) == 0 && MS.ix.maximum > MS.ix.minimum;
    MS.x = lv_display_get_horizontal_resolution(disp) / 2;
    MS.y = lv_display_get_vertical_resolution(disp) / 2;
    lv_indev_t *in = MS.in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, mouse_read);
    lv_indev_set_display(in, disp);
    return in;
}

/* ---------- тъч: точките от libinput са по монитора ---------- */
static lv_indev_read_cb_t touch_orig;
static void touch_read(lv_indev_t *in, lv_indev_data_t *data)
{
    touch_orig(in, data);
    phys_to_log(lv_indev_get_display(in), &data->point);
}

void touch_rotate_attach(lv_indev_t *touch)
{
    if (!touch || touch_orig) return;
    touch_orig = lv_indev_get_read_cb(touch);
    if (touch_orig) lv_indev_set_read_cb(touch, touch_read);
}
