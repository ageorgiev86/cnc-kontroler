/* Управлението на машината (Linux): интерпретатор + планер + връзката с STM32 в един цикъл от 1 ms.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Интерфейсът подава команди (mc_home, mc_jog, mc_run …) и чете моментна снимка (mc_snapshot).
 * Всички функции са безопасни от друга нишка. Цикълът е в собствена нишка (mc_start) или се вика
 * ръчно с mc_cycle (тестовете). Сегментите са по 1 ms, абсолютни позиции в 1/65536 стъпка.
 * Машинна координата = base + стъпки / spm; base се задава при реферирането (HOME_DONE). */
#ifndef CNC_MC_H
#define CNC_MC_H

#include "cnc/gcode.h"
#include "cnc/link.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MC_AXES 6

typedef struct {
    bool active, rotary;
    double spm;                    /* стъпки на мм (°) */
    double vmax, amax, jmax;       /* мм/s, мм/s², мм/s³ */
    double smin, smax;             /* меки лимити, машинни координати (след реферирането) */
    bool home_sw;                  /* има датчик HOME */
    int home_dir;                  /* +1 / -1 */
    int home_seq;                  /* ред на реферирането (по-малкото първо) */
    double home_search, home_latch;   /* мм/s */
    double home_pull;              /* мм */
    double home_sw_pos;            /* машинната координата на датчика */
    double home_after;             /* къде отива оста след реферирането */
    bool step_inv, dir_inv;
} mc_axis_t;

typedef struct {
    mc_axis_t ax[MC_AXES];
    double tol, arc_tol;           /* G64 по подразбиране и хордите на дъгите, мм */
    double sp_delay;               /* s: изчакване след пускане на шпиндела */
    int out_flood, out_mist, out_air;   /* бит на изхода (-1 = няма) */
    bool opt_stop;                 /* M1 спира */
} mc_cfg_t;

typedef enum {
    MC_NOLINK,      /* няма връзка със STM32 */
    MC_FAULT,       /* грешка (в STM32 или тук): RESET */
    MC_ESTOP,       /* авариен стоп */
    MC_IDLE,
    MC_HOMING,
    MC_RUN,         /* програма или MDI */
    MC_HOLD,        /* пауза на програмата */
    MC_MOVE,        /* ръчно движение или ход от интерфейса */
    MC_WAIT,        /* чака Старт: M0, M1, ръчна смяна на инструмент */
    MC_STOPPING     /* спира и изравнява позицията */
} mc_state_t;

enum { MC_WAIT_NONE, MC_WAIT_PAUSE, MC_WAIT_TOOL };

typedef struct {
    bool link;
    char port[64];
    mc_state_t state;
    int mcu_state;
    int fault;                     /* код от STM32 (0 = няма) */
    char alarm[200];               /* причината за MC_FAULT */
    double mach[MC_AXES];
    bool homed[MC_AXES];
    double v;                      /* скорост по пътя, мм/s */
    int line;                      /* редът, който се изпълнява (от 0), -1 = няма */
    int part;                      /* детайлът (GC_PART) */
    int tool;                      /* инструментът след M6 */
    int wcs;                       /* активната нула (1 = G54) */
    int wait_kind, wait_tool;
    int sp_dir;                    /* 0, 1 по часовника, -1 обратно */
    double sp_rpm;
    int cool;                      /* GC_COOL_* */
    bool prog_done;                /* програмата е стигнала края (до нов старт) */
    int done_seq;                  /* расте при всеки завършен ход, MDI или програма */
    uint32_t local_in, outputs, pendant;
    uint64_t inputs;
    int32_t mpg;
    uint16_t ain_mv[4];
    int queue;                     /* сегменти в STM32 и на път */
    double pkt_rate;               /* пакета/s */
    double max_gap_ms;             /* най-дългият цикъл за последните 5 s (Linux без реално време: закъснения) */
    uint32_t rx_errors, bad_rx;    /* отхвърлени в STM32 / тук */
    char msg[200];                 /* последното съобщение (напр. грешка в програмата) */
    int msg_seq;
} mc_snap_t;

typedef struct mc mc_t;

mc_t *mc_new(lk_port_t *port, const mc_cfg_t *cfg);
void mc_free(mc_t *m);                          /* спира нишката и затваря преноса */
int mc_start(mc_t *m);                           /* нишката (SCHED_FIFO, ако може) */
void mc_cycle(mc_t *m);                          /* един цикъл без нишка (тестовете) */
void mc_set_cfg(mc_t *m, const mc_cfg_t *cfg);   /* при спряна машина */
void mc_snapshot(mc_t *m, mc_snap_t *s);

void mc_reset(mc_t *m);                          /* изчиства грешката, пуска връзката */
void mc_estop(mc_t *m);                          /* програмен авариен стоп */
int mc_home(mc_t *m, unsigned mask);             /* 0 = прието */
int mc_jog(mc_t *m, int ax, int dir, double dist, double feed);   /* мм/мин; dist 0 = докато mc_jog_stop */
void mc_jog_stop(mc_t *m);
int mc_move(mc_t *m, const double tgt[MC_AXES], double feed);    /* NAN = без промяна; мм/мин, 0 = бърз ход */
/* програма или MDI: текстът се копира; tab може да е NULL (празни таблици); job може да е NULL */
int mc_run(mc_t *m, const char *text, const gc_config_t *gcfg, const gc_tables_t *tab, const gc_job_t *job);
void mc_hold(mc_t *m, bool on);
void mc_stop(mc_t *m);                           /* спиране по рампа и край на програмата */
void mc_continue(mc_t *m);                       /* след M0 / смяна на инструмент */
void mc_override(mc_t *m, double feed, double rapid);
void mc_single(mc_t *m, bool on);
void mc_outputs(mc_t *m, uint32_t value, uint32_t mask);
void mc_spindle(mc_t *m, int dir, double rpm);   /* ръчно: 0, 1, -1 */
void mc_set_pos(mc_t *m, int ax, double mach);   /* ръчно задаване на машинната координата (без датчик) */
void mc_set_tool(mc_t *m, int tool);             /* инструментът в шпиндела (M6 със същия T не чака) */

#ifdef __cplusplus
}
#endif

#endif
