/* Интерфейсът на Linux платката: DRM/KMS за екрана, libinput за мишка и тъч.
 *   cnc-ui [--drm /dev/dri/card0] [--mode 1920x1200] [--rot 0|90|180|270] [--modes] [--prog файл.nc]
 * Без --mode и --rot се ползват Настройки → Дисплей (auto = родният режим на монитора).
 * Устройството може да се зададе и с CNC_DRM_DEVICE. Без него се търси автоматично.
 * Курсорът на мишката се рисува от LVGL (няма X сървър). */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lvgl.h"
#include "config.h"
#include "drm_disp.h"
#include "kbd_evdev.h"
#include "mouse_wheel.h"
#include "machine.h"
#include "program.h"
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

static volatile sig_atomic_t quit;
static void on_sig(int s) { (void)s; quit = 1; }

int main(int argc, char **argv)
{
    const char *dev = getenv("CNC_DRM_DEVICE"), *mode = NULL;
    int rot = -1, list = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--drm") && i + 1 < argc) dev = argv[++i];
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc) mode = argv[++i];
        else if (!strcmp(argv[i], "--rot") && i + 1 < argc) rot = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--modes")) list = 1;
        else if (!strcmp(argv[i], "--prog") && i + 1 < argc) {
            if (program_load_file(argv[++i])) { fprintf(stderr, "Файлът не може да се прочете: %s\n", argv[i]); return 1; }
        } else { fprintf(stderr, "Употреба: cnc-ui [--drm /dev/dri/cardN] [--mode ШxВ] [--rot 0|90|180|270] [--modes] [--prog файл.nc]\n"); return 2; }
    }

    lv_init();
    lv_tick_set_cb(tick_ms);

    char *found = NULL;
    if (!dev) dev = found = lv_linux_drm_find_device_path();
    if (!dev) { fprintf(stderr, "Не е намерено DRM устройство (/dev/dri/card*)\n"); return 1; }
    if (list) { drm_disp_list(dev); return 0; }

    /* настройките трябват преди екрана: режим и завъртане */
    cfg_init();
    cfg_set_file(ui_config_path());
    if (!mode) mode = cfg_str("disp.dRes");
    if (rot < 0) rot = (int)cfg_num("disp.dRot");
    lv_display_t *disp = drm_disp_create(dev, mode, rot);
    if (!disp) return 1;
    printf("Екран: %s, %dx%d, завъртане %d°\n", dev, (int)lv_display_get_horizontal_resolution(disp),
           (int)lv_display_get_vertical_resolution(disp), rot);
    if (found) lv_free(found);
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    /* мишка (с курсор) и тъч (без курсор) */
    char *mouse = lv_libinput_find_dev(LV_LIBINPUT_CAPABILITY_POINTER, true);
    if (mouse) {
        lv_indev_t *in = lv_libinput_create(LV_INDEV_TYPE_POINTER, mouse);
        if (in) {
            lv_indev_set_display(in, disp);
            lv_indev_set_cursor(in, make_cursor());
            mouse_wheel_start(mouse, in);
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
    else if (!mouse) fprintf(stderr, "Няма мишка (само тъч). Провери с: sudo libinput list-devices\n");

    ui_create(disp);
    machine_apply_cfg();

    /* клавиатури (може и няколко, напр. безжичен приемник) */
    char *kb[4] = {0};
    size_t nkb = lv_libinput_find_devs(LV_LIBINPUT_CAPABILITY_KEYBOARD, kb, 4, true);
    lv_indev_t *kbi = kbd_evdev_create(kb, (int)nkb);
    if (kbi) { lv_indev_set_display(kbi, disp); ui_keys_attach(kbi); }
    else printf("Няма клавиатура (не е задължителна)\n");
    printf("Работи. Спира се с Ctrl+C.\n");

    while (!quit) {
        uint32_t idle = lv_timer_handler();
        if (idle > 10) idle = 10;
        usleep(idle * 1000);
    }
    drm_disp_close();
    printf("\nСпряно.\n");
    return 0;
}
