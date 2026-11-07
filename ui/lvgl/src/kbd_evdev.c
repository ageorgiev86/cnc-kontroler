/* Клавиатура направо от /dev/input/event* (без libxkbcommon): американска подредба, Shift и Caps Lock,
 * F1–F12, стрелки, Page Up/Down, цифровия блок. Клавишите стигат до LVGL като keypad устройство,
 * а оттам до ui_key(). Повторението при задържане идва от ядрото. */
#include "kbd_evdev.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define KB_MAX 4
#define Q_N 64
static int fds[KB_MAX], nfd;
static uint32_t q[Q_N];
static int qh, qt;
static bool shift, caps, pending;
static uint32_t pend_key;

static const char NORM[] = {
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5', [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8',
    [KEY_9] = '9', [KEY_0] = '0', [KEY_MINUS] = '-', [KEY_EQUAL] = '=', [KEY_Q] = 'q', [KEY_W] = 'w', [KEY_E] = 'e',
    [KEY_R] = 'r', [KEY_T] = 't', [KEY_Y] = 'y', [KEY_U] = 'u', [KEY_I] = 'i', [KEY_O] = 'o', [KEY_P] = 'p',
    [KEY_LEFTBRACE] = '[', [KEY_RIGHTBRACE] = ']', [KEY_A] = 'a', [KEY_S] = 's', [KEY_D] = 'd', [KEY_F] = 'f',
    [KEY_G] = 'g', [KEY_H] = 'h', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l', [KEY_SEMICOLON] = ';',
    [KEY_APOSTROPHE] = '\'', [KEY_GRAVE] = '`', [KEY_BACKSLASH] = '\\', [KEY_Z] = 'z', [KEY_X] = 'x', [KEY_C] = 'c',
    [KEY_V] = 'v', [KEY_B] = 'b', [KEY_N] = 'n', [KEY_M] = 'm', [KEY_COMMA] = ',', [KEY_DOT] = '.', [KEY_SLASH] = '/',
    [KEY_SPACE] = ' ', [KEY_KP7] = '7', [KEY_KP8] = '8', [KEY_KP9] = '9', [KEY_KPMINUS] = '-', [KEY_KP4] = '4',
    [KEY_KP5] = '5', [KEY_KP6] = '6', [KEY_KPPLUS] = '+', [KEY_KP1] = '1', [KEY_KP2] = '2', [KEY_KP3] = '3',
    [KEY_KP0] = '0', [KEY_KPDOT] = '.', [KEY_KPSLASH] = '/', [KEY_KPASTERISK] = '*',
};
static const char SHIFT_NUM[] = ")!@#$%^&*(";

static void push(uint32_t k)
{
    int n = (qt + 1) % Q_N;
    if (n == qh) return;
    q[qt] = k;
    qt = n;
}

static uint32_t map(int code)
{
    switch (code) {
    case KEY_ENTER: case KEY_KPENTER: return LV_KEY_ENTER;
    case KEY_BACKSPACE: return LV_KEY_BACKSPACE;
    case KEY_DELETE: return LV_KEY_DEL;
    case KEY_ESC: return LV_KEY_ESC;
    case KEY_UP: return LV_KEY_UP;
    case KEY_DOWN: return LV_KEY_DOWN;
    case KEY_LEFT: return LV_KEY_LEFT;
    case KEY_RIGHT: return LV_KEY_RIGHT;
    case KEY_HOME: return LV_KEY_HOME;
    case KEY_END: return LV_KEY_END;
    case KEY_PAGEUP: return 0xF000u + 20;
    case KEY_PAGEDOWN: return 0xF000u + 21;
    case KEY_TAB: return LV_KEY_NEXT;
    default: break;
    }
    if (code >= KEY_F1 && code <= KEY_F10) return 0xF000u + 1 + (uint32_t)(code - KEY_F1);
    if (code == KEY_F11 || code == KEY_F12) return 0xF000u + 11 + (uint32_t)(code - KEY_F11);
    if (code < 0 || code >= (int)sizeof NORM || !NORM[code]) return 0;
    char c = NORM[code];
    if (c >= 'a' && c <= 'z') return (uint32_t)((shift != caps) ? c - 32 : c);
    if (shift) {
        if (c >= '0' && c <= '9' && code < KEY_KP7) return (uint32_t)SHIFT_NUM[c - '0'];
        const char *a = "-=[];'`\\,./", *b = "_+{}:\"~|<>?";
        const char *p = strchr(a, c);
        if (p && code < KEY_KP7) return (uint32_t)b[p - a];
    }
    return (uint32_t)c;
}

static void poll_devs(void)
{
    struct input_event ev[32];
    for (int i = 0; i < nfd; i++) {
        ssize_t r;
        while ((r = read(fds[i], ev, sizeof ev)) > 0)
            for (int k = 0; k < (int)(r / (ssize_t)sizeof ev[0]); k++) {
                if (ev[k].type != EV_KEY) continue;
                int c = ev[k].code, v = ev[k].value;   /* 1 натиснат, 2 повторение, 0 пуснат */
                if (c == KEY_LEFTSHIFT || c == KEY_RIGHTSHIFT) { shift = v != 0; continue; }
                if (c == KEY_CAPSLOCK) { if (v == 1) caps = !caps; continue; }
                if (v == 0) continue;
                uint32_t key = map(c);
                if (key) push(key);
            }
    }
}

static void read_cb(lv_indev_t *in, lv_indev_data_t *d)
{
    (void)in;
    if (pending) {   /* всеки клавиш: натиснат, после пуснат */
        d->key = pend_key;
        d->state = LV_INDEV_STATE_RELEASED;
        pending = false;
    } else {
        poll_devs();
        if (qh == qt) { d->key = pend_key; d->state = LV_INDEV_STATE_RELEASED; return; }
        pend_key = q[qh];
        qh = (qh + 1) % Q_N;
        d->key = pend_key;
        d->state = LV_INDEV_STATE_PRESSED;
        pending = true;
    }
    d->continue_reading = pending || qh != qt;
}

lv_indev_t *kbd_evdev_create(char **paths, int n)
{
    for (int i = 0; i < n && nfd < KB_MAX; i++) {
        int fd = open(paths[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) { fprintf(stderr, "Клавиатура %s: не може да се отвори\n", paths[i]); continue; }
        fds[nfd++] = fd;
        printf("Клавиатура: %s\n", paths[i]);
    }
    if (!nfd) return NULL;
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(in, read_cb);
    return in;
}
