/* Интерфейсът на Linux платката: DRM/KMS за екрана, libinput за мишка и тъч.
 *   cnc-ui [--drm /dev/dri/card0]
 * Устройството може да се зададе и с CNC_DRM_DEVICE. Без него се търси автоматично.
 * Курсорът на мишката се рисува от LVGL (няма X сървър). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lvgl.h"
#include "ui.h"

static uint32_t tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

/* Стрелка 12×19: '#' контур, '.' запълване */
static const char *const ARROW[] = {
    "#           ", "##          ", "#.#         ", "#..#        ", "#...#       ",
    "#....#      ", "#.....#     ", "#......#    ", "#.......#   ", "#........#  ",
    "#.........# ", "#..........#", "#......#####", "#...#..#    ", "#..# #..#   ",
    "#.#  #..#   ", "##    #..#  ", "      #..#  ", "       ##   ",
};
#define ARROW_W 12
#define ARROW_H 19

static lv_obj_t *make_cursor(void)
{
    static uint32_t px[ARROW_W * ARROW_H];
    static lv_image_dsc_t dsc;
    for (int y = 0; y < ARROW_H; y++)
        for (int x = 0; x < ARROW_W; x++) {
            char c = ARROW[y][x];
            px[y * ARROW_W + x] = c == '#' ? 0xff000000u : c == '.' ? 0xffffffffu : 0;
        }
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    dsc.header.w = ARROW_W;
    dsc.header.h = ARROW_H;
    dsc.header.stride = ARROW_W * 4;
    dsc.data = (const uint8_t *)px;
    dsc.data_size = sizeof px;
    lv_obj_t *img = lv_image_create(lv_layer_sys());
    lv_image_set_src(img, &dsc);
    return img;
}

int main(int argc, char **argv)
{
    const char *dev = getenv("CNC_DRM_DEVICE");
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--drm") && i + 1 < argc) dev = argv[++i];
        else { fprintf(stderr, "Употреба: cnc-ui [--drm /dev/dri/cardN]\n"); return 2; }
    }

    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_linux_drm_create();
    char *found = NULL;
    if (!dev) dev = found = lv_linux_drm_find_device_path();
    if (!dev) { fprintf(stderr, "Не е намерено DRM устройство (/dev/dri/card*)\n"); return 1; }
    if (lv_linux_drm_set_file(disp, dev, -1) != LV_RESULT_OK) {
        fprintf(stderr, "DRM устройството %s не може да се отвори (свързан ли е екран? права?)\n", dev);
        return 1;
    }
    printf("Екран: %s, %dx%d\n", dev, (int)lv_display_get_horizontal_resolution(disp),
           (int)lv_display_get_vertical_resolution(disp));
    if (found) lv_free(found);

    /* мишка (с курсор) и тъч (без курсор) */
    char *mouse = lv_libinput_find_dev(LV_LIBINPUT_CAPABILITY_POINTER, true);
    if (mouse) {
        lv_indev_t *in = lv_libinput_create(LV_INDEV_TYPE_POINTER, mouse);
        if (in) {
            lv_indev_set_display(in, disp);
            lv_indev_set_cursor(in, make_cursor());
            printf("Мишка: %s\n", mouse);
        }
    }
    char *touch = lv_libinput_find_dev(LV_LIBINPUT_CAPABILITY_TOUCH, false);
    if (touch && (!mouse || strcmp(touch, mouse) != 0)) {
        lv_indev_t *in = lv_libinput_create(LV_INDEV_TYPE_POINTER, touch);
        if (in) {
            lv_indev_set_display(in, disp);
            printf("Тъч: %s\n", touch);
        }
    }
    if (!mouse && !touch) fprintf(stderr, "Няма мишка или тъч (libinput). Потребителят в група input ли е?\n");

    ui_create(disp);

    for (;;) {
        uint32_t idle = lv_timer_handler();
        if (idle > 10) idle = 10;
        usleep(idle * 1000);
    }
}
