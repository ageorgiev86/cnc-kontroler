/* Тестове с примерни програми: всеки файл .nc в tests/gcode се сравнява със своя .out ред по ред.
 * Първият ред може да е „; test: haas axes=XYZA block-delete“.
 * CNC_UPDATE=1 презаписва .out (след това изходът се проверява на ръка!). */
#include "cnc/gcode.h"
#include "gc_test_opts.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *buf; size_t n, cap; unsigned axes; } sbuf_t;

static void put(sbuf_t *s, const char *t)
{
    size_t l = strlen(t);
    if (s->n + l + 2 > s->cap) {
        s->cap = (s->n + l + 2) * 2;
        s->buf = realloc(s->buf, s->cap);
    }
    memcpy(s->buf + s->n, t, l);
    s->n += l;
    s->buf[s->n++] = '\n';
    s->buf[s->n] = 0;
}

static void on_cmd(void *ctx, const gc_cmd_t *c)
{
    sbuf_t *s = ctx;
    char b[256];
    gc_cmd_format(c, s->axes, b, sizeof b);
    put(s, b);
}

static void opts(const char *text, gc_config_t *cfg)
{
    const char *p = strstr(text, "; test:");
    if (!p || p != text) return;
    const char *end = strchr(p, '\n');
    char line[256];
    size_t l = end ? (size_t)(end - p) : strlen(p);
    if (l >= sizeof line) l = sizeof line - 1;
    memcpy(line, p, l);
    line[l] = 0;
    for (char *t = strtok(line + 7, " \t\r"); t; t = strtok(NULL, " \t\r")) {
        if (!strcmp(t, "haas")) cfg->dialect = GC_DIALECT_HAAS;
        else if (!strcmp(t, "mach3")) cfg->dialect = GC_DIALECT_MACH3;
        else if (!strcmp(t, "block-delete")) cfg->block_delete = true;
        else if (!strncmp(t, "axes=", 5)) cfg->axes = gc_axes_mask(t + 5, &cfg->rotary);
    }
}

static int run_one(const char *dir, const char *name)
{
    char path[1024], opath[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    snprintf(opath, sizeof opath, "%.*s.out", (int)strlen(path) - 3, path);
    char *text = gc_read_file(path);
    if (!text) { printf("FAIL %s: не се чете\n", name); return 1; }
    sbuf_t s = {0};
    gc_config_t cfg = {GC_DIALECT_MACH3, 0x7, 0, false, on_cmd, &s};
    opts(text, &cfg);
    s.axes = cfg.axes;
    put(&s, "");
    s.n = 0;
    s.buf[0] = 0;
    gc_tables_t *tab = calloc(1, sizeof *tab);
    gc_interp_t in;
    gc_err_t e;
    gc_interp_init(&in, &cfg, tab, NULL);
    if (gc_run_text(&in, text, &e)) {
        char b[300];
        if (e.col >= 0) snprintf(b, sizeof b, "ERR %d:%d %s", e.line + 1, e.col + 1, e.msg);
        else snprintf(b, sizeof b, "ERR %d %s", e.line + 1, e.msg);
        put(&s, b);
    }
    free(tab);
    free(text);
    int fail = 0;
    if (getenv("CNC_UPDATE")) {
        FILE *f = fopen(opath, "wb");
        if (f) { fwrite(s.buf, 1, s.n, f); fclose(f); }
        printf("UPDATE %s\n", name);
    } else {
        char *exp = gc_read_file(opath);
        if (!exp) { printf("FAIL %s: няма %s\n", name, opath); fail = 1; }
        else if (strcmp(exp, s.buf)) {
            /* първият различен ред */
            const char *a = exp, *b = s.buf;
            int ln = 1;
            while (*a && *a == *b) { if (*a == '\n') ln++; a++; b++; }
            while (a > exp && a[-1] != '\n') a--, b--;
            const char *ae = strchr(a, '\n'), *be = strchr(b, '\n');
            printf("FAIL %s, ред %d от изхода:\n  очаква се: %.*s\n  получено:  %.*s\n", name, ln,
                   (int)(ae ? ae - a : (long)strlen(a)), a, (int)(be ? be - b : (long)strlen(b)), b);
            fail = 1;
        } else {
            printf("ok   %s\n", name);
        }
        free(exp);
    }
    free(s.buf);
    return fail;
}

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "gcode";
    DIR *d = opendir(dir);
    if (!d) { perror(dir); return 2; }
    char *names[512];
    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) && n < 512) {
        size_t l = strlen(de->d_name);
        if (l > 3 && !strcmp(de->d_name + l - 3, ".nc")) names[n++] = strdup(de->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, sizeof names[0], cmp);
    int fails = 0;
    for (int i = 0; i < n; i++) { fails += run_one(dir, names[i]); free(names[i]); }
    printf("%d теста, %d неуспешни\n", n, fails);
    return fails ? 1 : 0;
}
