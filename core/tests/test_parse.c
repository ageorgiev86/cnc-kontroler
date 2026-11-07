/* Тестове на парсера: един ред → думи. */
#include "cnc/gcode.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails, total;
#define CHECK(c) do { total++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void)
{
    gc_block_t b;
    gc_err_t e;

    CHECK(gc_parse_line("G01X10.5y-2 Z.5F300", &b, &e) == 0);
    CHECK(b.ng == 1 && b.g[0] == 10);
    CHECK(GC_HAS(&b, 'X') && GC_VAL(&b, 'X') == 10.5);
    CHECK(GC_VAL(&b, 'Y') == -2 && GC_VAL(&b, 'Z') == 0.5 && GC_VAL(&b, 'F') == 300);
    CHECK((b.dot >> ('X' - 'A') & 1u) && !(b.dot >> ('Y' - 'A') & 1u));

    CHECK(gc_parse_line("G 1 X 1 0 (коментар) Y2 ; край", &b, &e) == 0);
    CHECK(b.g[0] == 10 && GC_VAL(&b, 'X') == 10 && GC_VAL(&b, 'Y') == 2);
    CHECK(!strcmp(b.comment, "коментар"));

    CHECK(gc_parse_line("G38.2 G90.1 G154 P5", &b, &e) == 0);
    CHECK(b.ng == 3 && b.g[0] == 382 && b.g[1] == 901 && b.g[2] == 1540);

    CHECK(gc_parse_line("T1M06", &b, &e) == 0);
    CHECK(b.nm == 1 && b.m[0] == 6 && GC_VAL(&b, 'T') == 1);

    CHECK(gc_parse_line("/G0 X1", &b, &e) == 0 && b.block_delete);
    CHECK(gc_parse_line("%", &b, &e) == 0 && b.percent);
    CHECK(gc_parse_line("  ", &b, &e) == 0 && b.empty);
    CHECK(gc_parse_line("(само коментар)", &b, &e) == 0 && b.empty);
    CHECK(gc_parse_line("O01234 (ИМЕ)", &b, &e) == 0 && GC_HAS(&b, 'O') && GC_VAL(&b, 'O') == 1234);
    CHECK(gc_parse_line("N10 G0 X1\r", &b, &e) == 0 && GC_VAL(&b, 'N') == 10);

    CHECK(gc_parse_line("G1 X1 X2", &b, &e) < 0 && e.col == 6);
    CHECK(gc_parse_line("G1 X", &b, &e) < 0);
    CHECK(gc_parse_line("G1 (незатворен", &b, &e) < 0 && e.col == 3);
    CHECK(gc_parse_line("G1 X#1", &b, &e) < 0);
    CHECK(gc_parse_line("G1.55", &b, &e) < 0);
    CHECK(gc_parse_line("G1 U5", &b, &e) < 0);
    CHECK(gc_parse_line("M3.5", &b, &e) < 0);

    printf("%d проверки, %d неуспешни\n", total, fails);
    return fails ? 1 : 0;
}
