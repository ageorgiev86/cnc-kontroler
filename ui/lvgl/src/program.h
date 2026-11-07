/* Заредената програма: редовете (за G-code списъка) и траекторията (за 3D изгледа и демо изпълнението).
 * Това е четене само за интерфейса. Истинският интерпретатор и планер са отделни модули в Linux
 * (docs/arhitektura.md); интерфейсът по-късно ще получава същите данни от тях. */
#ifndef CNC_PROGRAM_H
#define CNC_PROGRAM_H

#include <stdbool.h>

typedef struct { float x, y, z; } p3_t;

typedef enum { OP_MOVE, OP_TOOL, OP_SPINDLE, OP_COOL, OP_END } op_kind_t;

typedef struct {
    op_kind_t kind;
    int line;          /* ред в програмата (от 0) */
    bool rapid;        /* G0 */
    float feed;        /* мм/мин */
    int p0, np;        /* точките на хода в P.pt (np >= 2, включително началната) */
    float len;         /* дължина, мм */
    int tool;          /* OP_MOVE: инструментът по време на хода; OP_TOOL: новият */
    int s;             /* OP_SPINDLE: обороти (0 = спрян) */
    bool on;           /* OP_COOL */
    float t0, t1;      /* номинално време от началото на програмата, s */
} op_t;

#define PROG_MAX_TOOLS 8

typedef struct {
    char name[64];
    char **line;
    int nlines;
    p3_t *pt;          /* точки в работни координати */
    float *cum;        /* натрупана дължина в рамките на хода */
    int npt;
    op_t *op;
    int nop;
    float x0, x1, y0, y1, z0, z1;   /* граници на рязането (без бързите ходове до машинни координати) */
    int tools[PROG_MAX_TOOLS];
    int ntools;
    float t_total;     /* номинално време, s */
} program_t;

extern program_t P;

int program_load_text(const char *name, const char *text);
int program_load_file(const char *path);
void program_load_demo(void);

#endif
