/* Симулатор на интерфейса със SDL2.
 *   cnc-ui-sim [--res 800x1280] [--zoom 0.5]
 *   cnc-ui-sim --res 1920x1200 --shot екран.bmp [--after 500] [--reset] [--run] [--prog файл.nc]
 * --shot записва снимка на екрана след --after ms и излиза (ползва се и без дисплей:
 * SDL_VIDEODRIVER=dummy). --reset натиска RESET в началото, --run след реферирането пуска
 * програмата (демо или --prog). --screen отваря екран (run jog mdi tools probe diag set files),
 * --act изпълнява тестово действие от ui_test() (за снимки на диалози и състояния). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "lvgl.h"
#include "program.h"
#include "ui.h"

/* колелцето на мишката: както на платката, превърта областта под курсора (ui_wheel) */
static int wheel_acc;
static int on_sdl_event(void *ud, SDL_Event *e)
{
    (void)ud;
    if (e->type == SDL_MOUSEWHEEL) wheel_acc += e->wheel.y;
    return 0;
}

static void usage(void)
{
    fprintf(stderr, "Употреба: cnc-ui-sim [--res ШxВ] [--zoom к] [--shot файл.bmp] [--after ms] [--reset] [--run] [--prog файл.nc] [--screen име] [--act действие] [--conf файл]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    int w = 800, h = 1280, after = 300;
    float zoom = 1.0f;
    const char *shot = NULL;
    int press_reset = 0, run = 0;
    const char *prog = NULL, *screen = NULL, *conf = NULL, *act[16];
    int nact = 0;
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
        } else if (!strcmp(argv[i], "--conf") && i + 1 < argc) {
            conf = argv[++i];
        } else if (!strcmp(argv[i], "--screen") && i + 1 < argc) {
            screen = argv[++i];
        } else if (!strcmp(argv[i], "--act") && i + 1 < argc && nact < 16) {
            act[nact++] = argv[++i];
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
    lv_indev_t *mouse = lv_sdl_mouse_create();
    SDL_AddEventWatch(on_sdl_event, NULL);

    ui_create(disp);
    ui_keys_attach(lv_sdl_keyboard_create());
    if (conf) ui_load_config(conf);   /* в симулатора само при --conf, за да са снимките повторими */
    if (press_reset) machine_reset(lv_tick_get());
    if (screen && ui_go_name(screen)) { fprintf(stderr, "Няма екран „%s“\n", screen); return 2; }

    uint32_t start = lv_tick_get();
    for (;;) {
        uint32_t idle = lv_timer_handler();
        if (wheel_acc) {
            lv_point_t p;
            lv_indev_get_point(mouse, &p);
            ui_wheel(p.x, p.y, wheel_acc);
            wheel_acc = 0;
        }
        if (run && M.ready && M.state == MS_IDLE) { machine_start(lv_tick_get()); run = 0; }
        /* тестовите действия: по едно, когато машината е свободна (след реферирането, ако е пуснато с --reset) */
        static int ai;
        static uint32_t at;
        if (ai < nact && (!press_reset || M.ready) && M.state == MS_IDLE && lv_tick_elaps(at) > 300) {
            ui_test(act[ai++]);
            at = lv_tick_get();
        }
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
