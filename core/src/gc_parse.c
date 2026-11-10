/* Парсер: един ред G-code → блок от думи. */
#include "cnc/gcode.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
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

/* ---------- изрази (Macro B) ---------- */
typedef struct {
    const char *s;
    int i;
    gc_var_fn rd;
    void *ctx;
    gc_err_t *e;
    int err;
} ex_t;

static int exfail(ex_t *x, const char *fmt, ...)
{
    if (x->err) return -1;
    x->err = 1;
    if (x->e) {
        x->e->col = x->i;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(x->e->msg, sizeof x->e->msg, fmt, ap);
        va_end(ap);
    }
    return -1;
}

/* дума от букви (ключова дума или функция), главни; дължината или 0 */
static int peek_word(const ex_t *x, char *w, int n)
{
    int j = x->i, k = 0;
    while (isalpha((unsigned char)x->s[j]) && k < n - 1) w[k++] = (char)toupper((unsigned char)x->s[j++]);
    w[k] = 0;
    return k;
}

static bool take_word(ex_t *x, const char *kw)
{
    skip_ws(x->s, &x->i);
    char w[16];
    int k = peek_word(x, w, sizeof w);
    if (k && !strcmp(w, kw)) { x->i += k; return true; }
    return false;
}

static double ex_expr(ex_t *x);

static double ex_bracket(ex_t *x)
{
    skip_ws(x->s, &x->i);
    if (x->s[x->i] != '[') { exfail(x, "Очаква се „[“"); return 0; }
    x->i++;
    double v = ex_expr(x);
    skip_ws(x->s, &x->i);
    if (x->s[x->i] != ']') { exfail(x, "Липсва „]“"); return 0; }
    x->i++;
    return v;
}

/* номерът след # (число, #… или [ ]) */
static int ex_varnum(ex_t *x)
{
    double n;
    skip_ws(x->s, &x->i);
    char c = x->s[x->i];
    if (c == '#') { x->i++; int k = ex_varnum(x); if (x->err) return -1; n = x->rd(x->ctx, k); }
    else if (c == '[') n = ex_bracket(x);
    else {
        bool dot;
        if (read_num(x->s, &x->i, &n, &dot)) { exfail(x, "Липсва номер след #"); return -1; }
    }
    if (x->err) return -1;
    if (isnan(n) || n < 0 || fabs(n - lround(n)) > 1e-6 || n > 99999) { exfail(x, "Невалиден номер на променлива #%g", n); return -1; }
    return (int)lround(n);
}

static double ex_primary(ex_t *x)
{
    skip_ws(x->s, &x->i);
    char c = x->s[x->i];
    if (c == '[') return ex_bracket(x);
    if (c == '#') {
        x->i++;
        int n = ex_varnum(x);
        return x->err ? 0 : x->rd(x->ctx, n);
    }
    if ((c >= '0' && c <= '9') || c == '.') {
        double v;
        bool dot;
        if (read_num(x->s, &x->i, &v, &dot)) { exfail(x, "Невалидно число"); return 0; }
        return v;
    }
    char w[16];
    int k = peek_word(x, w, sizeof w);
    if (k) {
        static const char *const FN[] = {"SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN", "SQRT", "ABS", "ROUND", "FIX", "FUP", "LN", "EXP"};
        for (int f = 0; f < (int)(sizeof FN / sizeof *FN); f++) {
            if (strcmp(w, FN[f])) continue;
            x->i += k;
            double a = ex_bracket(x), d2r = M_PI / 180;
            if (x->err) return 0;
            if (isnan(a)) a = 0;
            switch (f) {
            case 0: return sin(a * d2r);
            case 1: return cos(a * d2r);
            case 2: return tan(a * d2r);
            case 3: return asin(a) / d2r;
            case 4: return acos(a) / d2r;
            case 5: {   /* ATAN[y]/[x] (ъгъл 0 … 360) или ATAN[a] */
                int save = x->i;
                skip_ws(x->s, &x->i);
                if (x->s[x->i] == '/') {
                    x->i++;
                    skip_ws(x->s, &x->i);
                    if (x->s[x->i] == '[') {
                        double b = ex_bracket(x), r = atan2(a, b) / d2r;
                        return r < 0 ? r + 360 : r;
                    }
                }
                x->i = save;
                return atan(a) / d2r;
            }
            case 6: if (a < 0) { exfail(x, "SQRT от отрицателно число"); return 0; } return sqrt(a);
            case 7: return fabs(a);
            case 8: return round(a);
            case 9: return trunc(a);
            case 10: return a < 0 ? floor(a) : ceil(a);
            case 11: if (a <= 0) { exfail(x, "LN от число ≤ 0"); return 0; } return log(a);
            default: return exp(a);
            }
        }
        exfail(x, "Непозната функция %s", w);
        return 0;
    }
    exfail(x, "Очаква се число, # или [ ]");
    return 0;
}

static double nz(double v) { return isnan(v) ? 0 : v; }

static double ex_unary(ex_t *x)
{
    skip_ws(x->s, &x->i);
    if (x->s[x->i] == '-') { x->i++; return -nz(ex_unary(x)); }
    if (x->s[x->i] == '+') { x->i++; return ex_unary(x); }
    return ex_primary(x);
}

static double ex_mul(ex_t *x)
{
    double v = ex_unary(x);
    for (;;) {
        skip_ws(x->s, &x->i);
        char c = x->s[x->i];
        if (c == '*') { x->i++; v = nz(v) * nz(ex_unary(x)); }
        else if (c == '/') {
            x->i++;
            double d = nz(ex_unary(x));
            if (d == 0) { exfail(x, "Деление на нула"); return 0; }
            v = nz(v) / d;
        }
        else if (take_word(x, "AND")) v = (double)((long)nz(v) & (long)nz(ex_unary(x)));
        else if (take_word(x, "MOD")) {
            double d = nz(ex_unary(x));
            if (d == 0) { exfail(x, "MOD на нула"); return 0; }
            v = fmod(nz(v), d);
        }
        else return v;
        if (x->err) return 0;
    }
}

static double ex_add(ex_t *x)
{
    double v = ex_mul(x);
    for (;;) {
        skip_ws(x->s, &x->i);
        char c = x->s[x->i];
        if (c == '+') { x->i++; v = nz(v) + nz(ex_mul(x)); }
        else if (c == '-') { x->i++; v = nz(v) - nz(ex_mul(x)); }
        else if (take_word(x, "OR")) v = (double)((long)nz(v) | (long)nz(ex_mul(x)));
        else if (take_word(x, "XOR")) v = (double)((long)nz(v) ^ (long)nz(ex_mul(x)));
        else return v;
        if (x->err) return 0;
    }
}

/* сравнение: 1 / 0; празната (#0) е равна само на празна при EQ/NE, иначе е 0 */
static double ex_expr(ex_t *x)
{
    double a = ex_add(x);
    static const char *const OP[] = {"EQ", "NE", "GT", "GE", "LT", "LE"};
    for (int o = 0; o < 6; o++) {
        if (!take_word(x, OP[o])) continue;
        double b = ex_add(x);
        if (x->err) return 0;
        if (o < 2 && (isnan(a) || isnan(b))) {
            bool eq = isnan(a) && isnan(b);
            return (o == 0) == eq;
        }
        a = nz(a);
        b = nz(b);
        switch (o) {
        case 0: return fabs(a - b) < 1e-9;
        case 1: return fabs(a - b) >= 1e-9;
        case 2: return a > b;
        case 3: return a >= b - 1e-9;
        case 4: return a < b;
        default: return a <= b + 1e-9;
        }
    }
    return a;
}

/* стойността на дума след буквата: число, #…, [ ] или -#… / -[ ] */
static int word_value(ex_t *x, double *v, bool *dot)
{
    skip_ws(x->s, &x->i);
    int j = x->i;
    if (x->s[j] == '-' || x->s[j] == '+') { j++; while (x->s[j] == ' ' || x->s[j] == '\t') j++; }
    if (x->s[j] == '#' || x->s[j] == '[') {
        if (!x->rd) return exfail(x, "Параметрите # и изразите [ ] тук не се поддържат");
        *v = ex_unary(x);
        *dot = true;
        return x->err ? -1 : 0;
    }
    return read_num(x->s, &x->i, v, dot);
}

/* номер след ключова дума (DO1, END 2, GOTO 100) */
static int kw_num(ex_t *x, const char *kw, int *n)
{
    double v;
    bool dot;
    skip_ws(x->s, &x->i);
    if (word_value(x, &v, &dot) || isnan(v) || v < 0 || fabs(v - lround(v)) > 1e-6) return exfail(x, "Липсва номер след %s", kw);
    *n = (int)lround(v);
    return 0;
}

/* #n = израз (от '#') */
static int assign(ex_t *x, gc_macro_t *mac)
{
    x->i++;
    int n = ex_varnum(x);
    if (x->err) return -1;
    skip_ws(x->s, &x->i);
    if (x->s[x->i] != '=') return exfail(x, "Очаква се „=“ след #%d", n);
    x->i++;
    double v = ex_expr(x);
    if (x->err) return -1;
    mac->kind = GC_MAC_ASSIGN;
    mac->var = n;
    mac->val = v;
    return 0;
}

/* макро команда от сегашната позиция (WHILE, END, IF, GOTO); 1 = не е такава */
static int macro_stmt(ex_t *x, gc_macro_t *mac)
{
    if (take_word(x, "WHILE")) {
        double c = ex_bracket(x);
        if (x->err) return -1;
        if (!take_word(x, "DO")) return exfail(x, "Очаква се DO след WHILE […]");
        if (kw_num(x, "DO", &mac->id)) return -1;
        mac->kind = GC_MAC_WHILE;
        mac->cond = nz(c) != 0;
        return 0;
    }
    if (take_word(x, "END")) {
        if (kw_num(x, "END", &mac->id)) return -1;
        mac->kind = GC_MAC_END;
        return 0;
    }
    if (take_word(x, "GOTO")) {
        if (kw_num(x, "GOTO", &mac->id)) return -1;
        mac->kind = GC_MAC_GOTO;
        mac->cond = true;
        return 0;
    }
    if (take_word(x, "IF")) {
        double c = ex_bracket(x);
        if (x->err) return -1;
        bool cond = nz(c) != 0;
        if (take_word(x, "GOTO")) {
            if (kw_num(x, "GOTO", &mac->id)) return -1;
            mac->kind = GC_MAC_GOTO;
        } else if (take_word(x, "THEN")) {
            skip_ws(x->s, &x->i);
            if (x->s[x->i] != '#') return exfail(x, "След THEN се очаква #n = …");
            if (assign(x, mac)) return -1;
        } else {
            return exfail(x, "Очаква се GOTO или THEN след IF […]");
        }
        mac->cond = cond;
        return 0;
    }
    return 1;
}

static bool line_end(const char *s, int i)
{
    while (s[i] == ' ' || s[i] == '\t') i++;
    return !s[i] || s[i] == '\n' || s[i] == '\r' || s[i] == ';' || s[i] == '(';
}

gc_mac_kind_t gc_macro_scan(const char *s, int *id)
{
    /* N…, после ключовата дума; номерът на DO е след последния ']' */
    int i = 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '/') i++;
    if ((s[i] & ~0x20) == 'N' && s[i + 1] >= '0' && s[i + 1] <= '9') { i++; while ((s[i] >= '0' && s[i] <= '9') || s[i] == ' ') i++; }
    char w[8];
    int k = 0;
    while (isalpha((unsigned char)s[i + k]) && k < 7) { w[k] = (char)toupper((unsigned char)s[i + k]); k++; }
    w[k] = 0;
    if (!strcmp(w, "END")) {
        int j = i + 3;
        while (s[j] == ' ') j++;
        *id = atoi(s + j);
        return GC_MAC_END;
    }
    if (!strcmp(w, "WHILE")) {
        const char *q = NULL;
        for (const char *t = s + i; *t && *t != '\n'; t++)
            if ((t[0] & ~0x20) == 'D' && (t[1] & ~0x20) == 'O') q = t + 2;
        if (!q) return GC_MAC_NONE;
        while (*q == ' ') q++;
        *id = atoi(q);
        return GC_MAC_WHILE;
    }
    return GC_MAC_NONE;
}

int gc_parse_line(const char *s, gc_block_t *b, gc_err_t *e) { return gc_parse_line_v(s, b, e, NULL, NULL, NULL); }

int gc_parse_line_v(const char *s, gc_block_t *b, gc_err_t *e, gc_var_fn rd, void *ctx, gc_macro_t *mac)
{
    memset(b, 0, sizeof *b);
    if (mac) memset(mac, 0, sizeof *mac);
    ex_t x = {s, 0, rd, ctx, e, 0};
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
        if (c == '#') {
            if (!mac || !rd) return fail(e, i, "Параметрите # и изразите [ ] тук не се поддържат");
            if (b->has & ~(1u << ('N' - 'A')) || b->ng || b->nm) return fail(e, i, "#n = … трябва да е сам в реда");
            x.i = i;
            if (assign(&x, mac)) return -1;
            mac->cond = true;
            if (!line_end(s, x.i)) return fail(e, x.i, "След #n = … не може да има други думи");
            b->empty = false;
            return 0;
        }
        if (c == '[') return fail(e, i, "Изразът [ ] трябва да е след буква (X[#1 + 2])");
        if (!isalpha((unsigned char)c)) return fail(e, i, "Неочакван знак „%c“", c);
        if (isalpha((unsigned char)s[i + 1]) && toupper((unsigned char)c) != 'O') {   /* ключова дума: WHILE, END, IF, GOTO */
            if (!mac || !rd) return fail(e, i, "WHILE, IF и GOTO тук не се поддържат");
            x.i = i;
            int r = macro_stmt(&x, mac);
            if (r < 0) return -1;
            if (r == 0) {
                if (b->has & ~(1u << ('N' - 'A')) || b->ng || b->nm) return fail(e, i, "WHILE, END, IF и GOTO трябва да са сами в реда");
                if (!line_end(s, x.i)) return fail(e, x.i, "Неочаквани думи след макро командата");
                b->empty = false;
                return 0;
            }
            char w[16];
            int k = peek_word(&x, w, sizeof w);
            return fail(e, i, "Непозната дума „%.*s“", k, w);
        }
        char L = (char)toupper((unsigned char)c);
        int col = i++;
        double v;
        bool dot;
        x.i = i;
        int wr = word_value(&x, &v, &dot);
        if (x.err) return -1;
        i = x.i;
        if (wr == 0 && isnan(v)) { b->empty = false; continue; }   /* празна променлива: думата се пропуска */
        if (wr) {
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
