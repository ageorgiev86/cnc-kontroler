/* Заредената програма: редовете (за G-code списъка) и траекторията (за 3D изгледа и демо изпълнението).
 * Това е четене само за интерфейса. Истинският интерпретатор и планер са отделни модули в Linux
 * (docs/arhitektura.md); интерфейсът по-късно ще получава същите данни от тях. */
#ifndef CNC_PROGRAM_H
#define CNC_PROGRAM_H

#include <stdbool.h>
#include <stddef.h>

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
    char **line;       /* сочат в text */
    char *text;        /* целият текст в един буфер (без отделна памет за всеки ред) */
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
    int err_line;      /* първата грешка на интерпретатора (ред от 0) или -1 */
    char err[200];
    int cap_pt, cap_op, cap_line;
} program_t;

extern program_t P;

int program_load_text(const char *name, const char *text);
int program_parse_into(program_t *dst, const char *name, const char *text);   /* за прегледа на файлове */
void program_free_into(program_t *p);
char *program_read_file(const char *path, long *size);   /* целия файл (malloc) или NULL */
const char *program_demo_text(int i, const char **name);   /* вградените примери; NULL след последния */
int program_load_file(const char *path);
void program_reparse(void);   /* наново през ядрото (сменен диалект, оси, нули) */
void program_load_demo(void);

/* редакция на заредената програма (екранът „Работа“) */
enum { PE_REPLACE, PE_DELETE, PE_INSERT };
int program_edit(int line, int mode, const char *txt);   /* PE_INSERT: новият ред става ред line */
char *program_join(const program_t *p, size_t *len);      /* редовете с \n (malloc) */

#endif
