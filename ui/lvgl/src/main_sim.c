/* Симулатор на интерфейса със SDL2.
 *   cnc-ui-sim [--res 800x1280] [--zoom 0.5]
 *   cnc-ui-sim --res 1920x1200 --shot екран.bmp [--after 500] [--reset] [--run] [--prog файл.nc]
 * --shot записва снимка на екрана след --after ms и излиза (ползва се и без дисплей:
 * SDL_VIDEODRIVER=dummy). --reset натиска RESET в началото, --run след реферирането пуска
 * програмата (демо или --prog). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lvgl.h"
#include "program.h"
#include "ui.h"

static void usage(void)
{
    fprintf(stderr, "Употреба: cnc-ui-sim [--res ШxВ] [--zoom к] [--shot файл.bmp] [--after ms] [--reset] [--run] [--prog файл.nc]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    int w = 800, h = 1280, after = 300;
    float zoom = 1.0f;
    const char *shot = NULL;
    int press_reset = 0, run = 0;
    const char *prog = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--res") && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w < 320 || h < 320) usage();
        } else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) {
            zoom = (float)atof(argv[++i]);
        } else if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
            shot = argv[++i];
        } else if (!strcmp(argv[i], "--after") && i + 1 < argc) {
            after = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--reset")) {
            press_reset = 1;
        } else if (!strcmp(argv[i], "--run")) {
            press_reset = run = 1;
        } else if (!strcmp(argv[i], "--prog") && i + 1 < argc) {
            prog = argv[++i];
        } else {
            usage();
        }
    }

    if (prog && program_load_file(prog)) { fprintf(stderr, "Файлът не може да се прочете: %s\n", prog); return 1; }
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(w, h);
    if (!disp) { fprintf(stderr, "SDL прозорецът не може да се създаде\n"); return 1; }
    lv_sdl_window_set_title(disp, "CNC интерфейс — симулатор");
    if (zoom > 0 && zoom != 1.0f) lv_sdl_window_set_zoom(disp, zoom);
    lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    lv_sdl_keyboard_create();

    ui_create(disp);
    if (press_reset) machine_reset(lv_tick_get());

    uint32_t start = lv_tick_get();
    for (;;) {
        uint32_t idle = lv_timer_handler();
        if (run && M.ready && M.state == MS_IDLE) { machine_start(lv_tick_get()); run = 0; }
        if (shot && lv_tick_elaps(start) >= (uint32_t)after) {
            lv_refr_now(disp);
            int rc = ui_save_bmp(shot);
            if (rc) fprintf(stderr, "Снимката не може да се запише: %s\n", shot);
            else printf("Снимка: %s (%dx%d)\n", shot, w, h);
            return rc ? 1 : 0;
        }
        if (idle > 10) idle = 10;
        usleep(idle * 1000);
    }
}
