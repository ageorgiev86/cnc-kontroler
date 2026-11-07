/* gcsim: G-code → команди на интерпретатора (засега), по-късно и сегменти на планера.
 *   gcsim [--haas] [--axes XYZA] [--block-delete] файл.nc
 * Изходът е по един ред на команда; грешката е на stderr с ред и колона. */
#include "cnc/gcode.h"
#include "gc_test_opts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_cmd(void *ctx, const gc_cmd_t *c)
{
    char buf[256];
    gc_cmd_format(c, *(unsigned *)ctx, buf, sizeof buf);
    puts(buf);
}

int main(int argc, char **argv)
{
    gc_config_t cfg = {GC_DIALECT_MACH3, 0x7, 0, false, print_cmd, NULL};
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--haas")) cfg.dialect = GC_DIALECT_HAAS;
        else if (!strcmp(argv[i], "--block-delete")) cfg.block_delete = true;
        else if (!strcmp(argv[i], "--axes") && i + 1 < argc) cfg.axes = gc_axes_mask(argv[++i], &cfg.rotary);
        else if (argv[i][0] != '-') path = argv[i];
        else { fprintf(stderr, "непозната опция %s\n", argv[i]); return 2; }
    }
    if (!path) { fprintf(stderr, "употреба: gcsim [--haas] [--axes XYZA] [--block-delete] файл.nc\n"); return 2; }
    char *text = gc_read_file(path);
    if (!text) { perror(path); return 2; }
    cfg.ctx = &cfg.axes;
    gc_tables_t *tab = calloc(1, sizeof *tab);
    gc_interp_t in;
    gc_err_t e;
    gc_interp_init(&in, &cfg, tab, NULL);
    int r = gc_run_text(&in, text, &e);
    if (r && e.col >= 0) fprintf(stderr, "%s:%d:%d: %s\n", path, e.line + 1, e.col + 1, e.msg);
    else if (r) fprintf(stderr, "%s:%d: %s\n", path, e.line + 1, e.msg);
    free(text);
    free(tab);
    return r ? 1 : 0;
}
