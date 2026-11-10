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

/* „1,2,3“ → нулите на детайлите (1 = G54); връща броя */
static inline int gc_parse_parts(const char *s, int *parts, int max)
{
    int n = 0;
    while (*s && n < max) {
        char *e;
        long v = strtol(s, &e, 10);
        if (e == s) break;
        parts[n++] = (int)v;
        s = *e == ',' ? e + 1 : e;
    }
    return n;
}

/* „ред[:детайл]“ (и двете от 1) → start_line, start_part (от 0) */
static inline void gc_parse_from(const char *s, int *line, int *part)
{
    *line = atoi(s) - 1;
    const char *c = s;
    while (*c && *c != ':') c++;
    *part = *c == ':' ? atoi(c + 1) - 1 : 0;
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
