/* Планер: look-ahead и интерполатор 1 ms с ограничен jerk (S-крива), docs/interpretator-planer.md, стъпка 3.
 * Вход: командите на интерпретатора (машинни координати, компенсацията е вече приложена).
 * Изход: абсолютна позиция на всеки такт (както MOTION_SEGMENT към STM32) и събитията в точния момент. */
#ifndef CNC_PLANNER_H
#define CNC_PLANNER_H

#include "cnc/gcode.h"
#include "cnc/hmap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double vmax[GC_NAX];   /* мм/s (°/s за въртящите оси) */
    double amax[GC_NAX];   /* мм/s² */
    double jmax[GC_NAX];   /* мм/s³ */
    double tol;            /* допуск на ъглите при G64 без P, мм */
    double arc_tol;        /* най-голямо отклонение на хордата от дъгата, мм */
    double dt;             /* такт, s (0,001) */
} pl_config_t;

typedef struct pl pl_t;

enum {
    PL_SAMPLE,             /* нова позиция (и може би събитие) */
    PL_STARVED,            /* буферът е празен: трябват още команди */
    PL_WAIT,               /* спряно на събитие (смяна, M0, шпиндел …): чака pl_continue() */
    PL_DONE                /* краят на програмата (GC_END) */
};

typedef struct {
    double pos[GC_NAX];    /* машинна позиция в края на такта */
    double v, a;           /* скорост и ускорение по пътя, мм/s, мм/s² */
    int line;              /* редът на програмата */
    bool has_ev;
    gc_cmd_t ev;           /* събитието, стигнато в този такт */
} pl_sample_t;

pl_t *pl_create(const pl_config_t *cfg, const double start[GC_NAX]);
void pl_destroy(pl_t *p);
int pl_space(const pl_t *p);                    /* свободни места в буфера */
int pl_push(pl_t *p, const gc_cmd_t *c);        /* 0 = приета, -1 = няма място (опитай по-късно) */
void pl_input_end(pl_t *p);                     /* няма да има още команди: краят на буфера е краят */
int pl_tick(pl_t *p, pl_sample_t *s);           /* един такт; PL_* */
void pl_continue(pl_t *p);                      /* след PL_WAIT */
void pl_hold(pl_t *p, bool on);                 /* пауза на подаването: плавно спиране и чакане */
void pl_override(pl_t *p, double feed, double rapid);   /* 1,0 = 100 %; подаване 0 … 2, бърз ход 0 … 1; M49 изключва подаването */
void pl_single(pl_t *p, bool on);               /* единичен блок: спира в края на всеки ред (PL_WAIT) */
/* картата на височините за следващите приети команди (NULL = без); картата трябва да живее, докато се ползва.
 * G1 и дъгите се делят до резолюция / 4 и Z им се коригира; бързите ходове не се коригират */
void pl_set_hmap(pl_t *p, const hm_map_t *m);
double pl_ahead(const pl_t *p);                 /* мм път в буфера пред сегашната точка */

#ifdef __cplusplus
}
#endif

#endif
