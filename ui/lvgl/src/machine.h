/* Състояние на машината, което интерфейсът показва.
 * Засега е демо модел (както в ui/maket.html). По-късно данните идват
 * от интерпретатора/планера и от STM32 по SPI, а интерфейсът само ги чете. */
#ifndef CNC_MACHINE_H
#define CNC_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

enum { AX_X, AX_Y, AX_Z, AX_A, AX_B, AX_C, AX_N };

typedef enum { MS_IDLE, MS_RUN, MS_HOLD, MS_ALARM, MS_ESTOP } mstate_t;
typedef enum { HOMED_NO, HOMED_SENSOR, HOMED_MANUAL } homed_t;
typedef enum { MT_MILL, MT_LASER, MT_PLASMA, MT_LATHE } mtype_t;
typedef enum { MSG_NONE, MSG_INFO, MSG_ACT, MSG_BAD } msg_kind_t;

typedef struct {
    mstate_t state;
    bool enabled;          /* задвижванията са включени */
    bool ready;            /* включени и реферирани */
    bool homing;           /* реферирване след RESET */
    mtype_t type;

    double mach[AX_N];     /* машинна позиция, мм или ° */
    double off[AX_N];      /* активна работна нула (G54) */
    double togo[AX_N];     /* остатък до края на блока */
    double ferr[AX_N];     /* следяща грешка, µm (или ″ за въртящи оси) */

    bool active[AX_N];
    bool rotary[AX_N];
    bool home_sw[AX_N];    /* има датчик за Home */
    int home_seq[AX_N];    /* ред на реферирване */
    bool enc_on[AX_N];
    double ferr_lim[AX_N];
    homed_t homed[AX_N];

    int wcs;               /* 1 = G54 … 6 = G59 */
    int tool;
    char tool_desc[40];
    double tool_dia;

    bool mcu_link;
    int mcu_rate_hz;

    bool coord_mach;       /* позициите се показват в машинни координати */

    /* изпълнение на програмата (демо симулация върху P от program.h) */
    int op;                /* текущ ход в P.op */
    float op_s;            /* изминато в хода: мм (ход) или s (действие) */
    int line;              /* текущ ред на програмата, -1 = преди началото */
    bool prog_done;
    float run_time;        /* s от старта */
    float done_t;          /* изпълнено номинално време, s */
    int fov, sov, rov;     /* оверрайди: подаване, шпиндел, бърз ход, % */
    float feed_act;        /* мм/мин */
    float feed_prog;       /* F от програмата */
    int sp_cmd;            /* S от програмата */
    float sp_act;          /* действителни обороти */
    bool sp_on, coolant;
    bool single;           /* единичен блок */

    char msg[200];
    msg_kind_t msg_kind;
    uint32_t msg_until;    /* lv_tick, до кога се показва временното съобщение */
} machine_t;

extern machine_t M;
extern const char *const AX_NAME[AX_N];

void machine_init(void);
void machine_tick(uint32_t now_ms, uint32_t dt_ms);

/* Действия от интерфейса */
void machine_reset(uint32_t now_ms);
void machine_estop(void);

/* Програма: старт (и продължаване от пауза), пауза, стоп, връщане в началото, оверрайди */
void machine_start(uint32_t now_ms);
void machine_hold(void);
void machine_stop(uint32_t now_ms);
void machine_rewind(uint32_t now_ms);
void machine_fov(int delta, uint32_t now_ms);   /* delta 0 = 100 % */
void machine_sov(int delta, uint32_t now_ms);
void machine_rov(int pct);
const char *machine_tool_name(int t, double *dia);
float machine_remaining_s(void);

double machine_work(int ax);
bool machine_need_home(void);
void machine_toast(const char *txt, uint32_t now_ms, uint32_t dur_ms);

#endif
