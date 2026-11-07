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
/* какво изпълнява машината в момента: програма или опашка от движения (ръчно, MDI, реферирване …) */
typedef enum { RK_NONE, RK_PROG, RK_JOG, RK_MDI, RK_HOME, RK_PROBE, RK_TOOL } runkind_t;

#define TOOL_MAX 255
typedef struct {
    char name[40];         /* празно = празно гнездо */
    double len, dia, wear;
} tool_t;
extern tool_t TT[TOOL_MAX + 1];

/* резултат от измерване (картата „Резултат“ в Офсети) */
typedef struct {
    int seq;               /* расте при всеки нов резултат */
    char title[80];
    char rows[6][2][48];
    int nrows;
    char note[200];
    bool warn;
} prb_result_t;

typedef struct {
    mstate_t state;
    bool enabled;          /* задвижванията са включени */
    bool ready;            /* включени и реферирани */
    bool homing;           /* реферирване след RESET */
    mtype_t type;

    double mach[AX_N];     /* машинна позиция, мм или ° */
    double *off;           /* активната работна нула: сочи в wcs_off */
    double wcs_off[7][AX_N];   /* G54 … G59 = 1 … 6 (машинни координати на нулата) */
    int parts;             /* детайли за програмата: бит n = G53+n; G54 е винаги */
    int wcs_run;           /* нулата на текущия детайл по време на работа (0 = M.wcs) */
    int part_i, part_n;    /* текущ детайл / брой детайли */
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
    float t_total;         /* номинално време за всички детайли, s */
    int fov, sov, rov;     /* оверрайди: подаване, шпиндел, бърз ход, % */
    float feed_act;        /* мм/мин */
    float feed_prog;       /* F от програмата */
    int sp_cmd;            /* S от програмата */
    float sp_act;          /* действителни обороти */
    bool sp_on, coolant;
    bool single;           /* единичен блок */
    runkind_t run_kind;

    /* ръчно движение */
    double jog_inc;        /* стъпка, мм; 0 = непрекъснато (докато бутонът е натиснат) */
    int jog_f;             /* мм/мин */
    bool ref_manual;       /* реферирване „Ръчно, тук“ вместо с датчик */
    int jog_ax, jog_dir;   /* задържан бутон при непрекъснато движение; jog_dir 0 = няма */

    /* инструменти и магазин */
    int sel_tool;          /* избраният в таблицата (за „Смени“) */
    int mag_pos;           /* гнездото на позицията за смяна */
    float mag_ph;          /* плавна позиция за анимацията */
    int wpos[6];           /* карусел с колела: гнездото на всяко колело при смяната */
    float wph[6];
    bool clamped, orient, mag_moving;
    int mtc;               /* ръчна смяна: чака се поставяне на T (0 = не) */
    char act_label[96];    /* текущото действие от опашката */
    prb_result_t res;
    int prb_cnt, ts_cnt;   /* брой сработвания на входовете */
    uint32_t prb_flash, ts_flash;

    char alarm[200];       /* причината за алармата */
    int acc_level;         /* достъп: 0 оператор, 1 настройчик, 2 производител */

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
void machine_alarm(const char *why);   /* спира движението и изключва задвижванията */

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

/* Ръчно управление (екраните Ръчно и MDI) */
bool machine_can_move(uint32_t now_ms);          /* иначе показва причината */
void machine_jog(int ax, int dir, uint32_t now_ms);   /* една стъпка или начало на непрекъснато */
void machine_jog_release(void);
void machine_home(int ax, uint32_t now_ms);           /* ax -1 = всички активни */
void machine_goto_zero(uint32_t now_ms);
void machine_zero(int ax, uint32_t now_ms);           /* ax -1 = X, Y и Z */
int machine_set_pos(int ax, bool mach, double v, uint32_t now_ms);
int machine_mdi(const char *text, uint32_t now_ms);   /* 0 = прието за изпълнение */
void machine_coolant(uint32_t now_ms);
void machine_select_tool(int t);                      /* T в шпиндела (след смяна) */
int mag_n(void);                                      /* брой гнезда / инструменти */
int mag_wheels(void);
bool machine_auto_tc(void);                           /* има магазин и автоматичната смяна е включена */
void machine_tool_change(int t, uint32_t now_ms);
void machine_measure_tool(uint32_t now_ms);
void machine_mag_tick(float dt);
void machine_result(const char *title, const char *note);   /* нов резултат; редовете се добавят с machine_result_row */
void machine_result_row(const char *k, const char *v);
void machine_apply_cfg(void);                         /* след „Запиши“ в настройките */

/* Опашка от движения и действия: координатите са машинни, NAN = без промяна.
 * feed 0 = бърз ход. fn се вика в края на действието. */
typedef void (*mq_fn_t)(int arg, uint32_t now_ms);
void mq_begin(runkind_t kind);
void mq_move(const double tgt[AX_N], float feed, bool ov, const char *label);
void mq_act(float dur_s, mq_fn_t fn, int arg, const char *label);
void mq_go(void);
bool mq_busy(void);
void mq_set_refill(bool (*fn)(void));   /* при празна опашка fn добавя следващата порция (дълги сканирания) */

/* Карта на височините (по модела на Autoleveler в UGS): мрежа от точки, билинейна корекция на Z */
#define HM_MAX 64
typedef struct {
    double x0, x1, y0, y1, z0, z1, res, zs, ret, f, scan;   /* работни координати, мм, %, мм/мин */
    int nx, ny, wcs;
    float z[HM_MAX][HM_MAX];
    int got;               /* измерени точки */
    bool done, apply;
} hmap_t;
extern hmap_t HM;
void hmap_grid(void);                   /* nx, ny от областта и стъпката; изчиства картата */
void hmap_scan(uint32_t now_ms);
double hmap_corr(double x, double y);   /* корекция на Z (0 извън областта или без карта) */

/* Нулеви точки и детайли (екранът „Офсети“) */
void machine_wcs_select(int n, uint32_t now_ms);      /* 1 … 6 */
void machine_wcs_toggle(int n, uint32_t now_ms);      /* отметка за G55 … G59 */
bool machine_wcs_isset(int n);
int machine_parts(int *list);                         /* списък на детайлите (G54 + отметнатите) */
void machine_probe_cycle(const char *k, int corner, uint32_t now_ms);   /* corner 0 fl, 1 fr, 2 bl, 3 br */
extern bool prb_warn;
void machine_skip_part(uint32_t now_ms);              /* пропуска текущия детайл (при пауза) */                                 /* цикълът иска T10 в шпиндела */

double machine_work(int ax);
bool machine_need_home(void);
void machine_toast(const char *txt, uint32_t now_ms, uint32_t dur_ms);

#endif
