/* Колелцето на мишката от /dev/input/event* (само четене, без да се взима устройството от libinput). */
#include "mouse_wheel.h"
#include "ui.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>

static int fd = -1;
static lv_indev_t *ptr;

static void poll_wheel(lv_timer_t *t)
{
    (void)t;
    struct input_event ev[32];
    ssize_t r;
    int notches = 0;
    while ((r = read(fd, ev, sizeof ev)) > 0)
        for (int k = 0; k < (int)(r / (ssize_t)sizeof ev[0]); k++)
            if (ev[k].type == EV_REL && ev[k].code == REL_WHEEL) notches += ev[k].value;   /* + = нагоре */
    if (notches) {
        lv_point_t p;
        lv_indev_get_point(ptr, &p);
        ui_wheel(p.x, p.y, notches);
    }
}

void mouse_wheel_start(const char *path, lv_indev_t *mouse)
{
    if (!path || !mouse || fd >= 0) return;
    fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { fprintf(stderr, "Колелце на мишката %s: не може да се отвори\n", path); return; }
    ptr = mouse;
    lv_timer_create(poll_wheel, 15, NULL);
}
