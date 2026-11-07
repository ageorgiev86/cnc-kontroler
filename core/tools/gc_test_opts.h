/* Общи помощни функции за gcsim и тестовете. */
#ifndef GC_TEST_OPTS_H
#define GC_TEST_OPTS_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* „XYZA“ → маска на осите; A, B, C са въртящи */
static inline unsigned gc_axes_mask(const char *s, unsigned *rotary)
{
    unsigned m = 0, r = 0;
    for (; *s; s++) {
        switch (toupper((unsigned char)*s)) {
        case 'X': m |= 1u; break;
        case 'Y': m |= 2u; break;
        case 'Z': m |= 4u; break;
        case 'A': m |= 8u; r |= 8u; break;
        case 'B': m |= 16u; r |= 16u; break;
        case 'C': m |= 32u; r |= 32u; break;
        default: break;
        }
    }
    if (rotary) *rotary = r;
    return m;
}

/* целия файл в памет (malloc, завършва с 0) или NULL */
static inline char *gc_read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *t = n >= 0 ? malloc((size_t)n + 1) : NULL;
    if (t && fread(t, 1, (size_t)n, f) != (size_t)n) { free(t); t = NULL; }
    if (t) t[n] = 0;
    fclose(f);
    return t;
}

#endif
