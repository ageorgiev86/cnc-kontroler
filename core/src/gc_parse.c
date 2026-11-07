/* Парсер: един ред G-code → блок от думи. */
#include "cnc/gcode.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int fail(gc_err_t *e, int col, const char *fmt, ...)
{
    if (e) {
        e->col = col;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(e->msg, sizeof e->msg, fmt, ap);
        va_end(ap);
    }
    return -1;
}

static void skip_ws(const char *s, int *i)
{
    while (s[*i] == ' ' || s[*i] == '\t') (*i)++;
}

/* Число: знак, цифри, точка, цифри; позволени са интервали между тях (RS274). */
static int read_num(const char *s, int *i, double *v, bool *dot)
{
    skip_ws(s, i);
    double sign = 1;
    if (s[*i] == '+' || s[*i] == '-') {
        if (s[*i] == '-') sign = -1;
        (*i)++;
        skip_ws(s, i);
    }
    double ip = 0, fp = 0, sc = 1;
    int digits = 0;
    *dot = false;
    for (;;) {
        char c = s[*i];
        if (c >= '0' && c <= '9') {
            if (*dot) { sc *= 0.1; fp += (c - '0') * sc; }
            else ip = ip * 10 + (c - '0');
            digits++;
        } else if (c == '.' && !*dot) {
            *dot = true;
        } else if (c == ' ' || c == '\t') {
            /* интервал вътре в числото се допуска само ако следва цифра или точка */
            int j = *i;
            while (s[j] == ' ' || s[j] == '\t') j++;
            if (!((s[j] >= '0' && s[j] <= '9') || (s[j] == '.' && !*dot))) break;
        } else {
            break;
        }
        (*i)++;
    }
    if (!digits) return -1;
    *v = sign * (ip + fp);
    return 0;
}

int gc_parse_line(const char *s, gc_block_t *b, gc_err_t *e)
{
    memset(b, 0, sizeof *b);
    b->empty = true;
    int i = 0;
    skip_ws(s, &i);
    if (s[i] == '/') { b->block_delete = true; i++; }
    skip_ws(s, &i);
    if (s[i] == '%') {
        int j = i + 1;
        skip_ws(s, &j);
        if (!s[j] || s[j] == '\r' || s[j] == '\n') { b->percent = true; return 0; }
    }
    while (s[i] && s[i] != '\n' && s[i] != '\r') {
        char c = s[i];
        if (c == ' ' || c == '\t') { i++; continue; }
        if (c == '(') {
            int st = i++;
            int k = 0;
            while (s[i] && s[i] != ')' && s[i] != '\n' && s[i] != '\r') {
                if (s[i] == '(') return fail(e, i, "Вложени скоби в коментар");
                if (!b->comment[0] || k) {
                    if (k < (int)sizeof b->comment - 1) b->comment[k++] = s[i];
                }
                i++;
            }
            if (s[i] != ')') return fail(e, st, "Незатворен коментар: липсва „)“");
            if (k) b->comment[k] = 0;
            i++;
            continue;
        }
        if (c == ';') break;
        if (c == '#' || c == '[') return fail(e, i, "Параметрите # и изразите [ ] идват с макросите");
        if (!isalpha((unsigned char)c)) return fail(e, i, "Неочакван знак „%c“", c);
        char L = (char)toupper((unsigned char)c);
        int col = i++;
        double v;
        bool dot;
        if (read_num(s, &i, &v, &dot)) {
            if (L == 'O') {   /* O<име> или O без номер: пропуска се до края */
                b->empty = false;
                while (s[i] && s[i] != '\n' && s[i] != '\r' && s[i] != '(' && s[i] != ';') i++;
                continue;
            }
            return fail(e, col, "Липсва число след %c", L);
        }
        b->empty = false;
        if (L == 'G') {
            double g10 = v * 10;
            int iv = (int)lround(g10);
            if (fabs(g10 - iv) > 1e-6 || iv < 0) return fail(e, col, "Невалиден G-код G%g", v);
            if (b->ng >= GC_MAX_G) return fail(e, col, "Твърде много G-кодове в един ред");
            b->gcol[b->ng] = (short)col;
            b->g[b->ng++] = iv;
        } else if (L == 'M') {
            int iv = (int)lround(v);
            if (fabs(v - iv) > 1e-9 || iv < 0) return fail(e, col, "Невалиден M-код M%g", v);
            if (b->nm >= GC_MAX_M) return fail(e, col, "Твърде много M-кодове в един ред");
            b->mcol[b->nm] = (short)col;
            b->m[b->nm++] = iv;
        } else if (L == 'U' || L == 'V' || L == 'W') {
            return fail(e, col, "Оста %c не се поддържа (осите са X Y Z A B C)", L);
        } else {
            int n = L - 'A';
            if (b->has >> n & 1u) return fail(e, col, "Думата %c се повтаря в реда", L);
            b->has |= 1u << n;
            if (dot) b->dot |= 1u << n;
            b->v[n] = v;
            b->col[n] = (short)col;
        }
    }
    return 0;
}
