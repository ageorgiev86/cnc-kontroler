/* Парсер и интерпретатор на G-code (docs/interpretator-planer.md).
 * Чист C11, без LVGL и без нишки. Интерпретаторът вади прости команди с машинни координати. */
#ifndef CNC_GCODE_H
#define CNC_GCODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { GC_X, GC_Y, GC_Z, GC_A, GC_B, GC_C, GC_NAX };

/* ---------- грешки ---------- */
typedef struct {
    int line;              /* ред в програмата (от 0), -1 = няма */
    int col;               /* колона (от 0), -1 = няма */
    char msg[200];
} gc_err_t;

/* ---------- парсер: ред → блок ---------- */
#define GC_MAX_G 12
#define GC_MAX_M 6

typedef struct {
    uint32_t has;          /* бит n = буква 'A' + n (без G и M) */
    uint32_t dot;          /* стойността е с десетична точка */
    double v[26];
    short col[26];
    int g[GC_MAX_G];       /* G × 10: G38.2 = 382, G90.1 = 901 */
    short gcol[GC_MAX_G];
    int ng;
    int m[GC_MAX_M];
    short mcol[GC_MAX_M];
    int nm;
    bool block_delete;     /* '/' в началото */
    bool percent;          /* ред '%' */
    bool empty;            /* само коментари / празен */
    char comment[96];      /* първият коментар */
} gc_block_t;

#define GC_HAS(b, ch) (((b)->has >> ((ch) - 'A')) & 1u)
#define GC_VAL(b, ch) ((b)->v[(ch) - 'A'])

int gc_parse_line(const char *s, gc_block_t *b, gc_err_t *e);   /* 0 = ok */

/* ---------- таблици: нули и инструменти ---------- */
#define GC_WCS_N 106       /* 0 не се ползва; 1–6 = G54–G59; 7–105 = G154 P1–P99 (Haas: G110–G129 = P1–P20) */
#define GC_TOOL_N 256      /* T0 … T255 */

typedef struct {
    double len, wear;      /* дължина и износване по дължина, мм */
    double dia, wear_dia;  /* диаметър и износване, мм */
} gc_tool_t;

typedef struct {
    double wcs[GC_WCS_N][GC_NAX];   /* машинни координати на нулата */
    gc_tool_t tool[GC_TOOL_N];
    double g28[GC_NAX], g30[GC_NAX];   /* машинни позиции за G28 / G30 */
} gc_tables_t;

/* ---------- команди към планера ---------- */
typedef enum {
    GC_RAPID,              /* tgt */
    GC_FEED,               /* tgt, feed */
    GC_ARC,                /* tgt, ctr (машинни), plane, cw, turns, feed */
    GC_DWELL,              /* val = s */
    GC_SPINDLE,            /* ival = посока (1 CW, -1 CCW, 0 стоп), val = обороти */
    GC_COOLANT,            /* ival = битове GC_COOL_* */
    GC_TOOL_PREP,          /* ival = T (подготовка в магазина) */
    GC_TOOL_CHANGE,        /* ival = T */
    GC_TLO,                /* val = корекция на дължината, ival = H */
    GC_ORIENT,             /* val = ъгъл, ° */
    GC_OVR_ENABLE,         /* ival = 1 / 0 (M48 / M49) */
    GC_PATH_MODE,          /* ival = 610, 611, 640; val = допуск, мм (0 = без) */
    GC_WCS_SELECT,         /* ival = активна нула 1 … 105 */
    GC_SET_WCS,            /* ival = номер на нулата (таблицата е сменена) */
    GC_SET_TOOL,           /* ival = T (таблицата е сменена) */
    GC_HOME,               /* ival = маска на осите (Mach3 G28.1) */
    GC_PAUSE,              /* ival = 0 (M0) / 1 (M1, по избор) */
    GC_PART,               /* начало на детайл: ival = поредният (от 0), val = нулата (1 = G54 …) */
    GC_END,                /* M2 / M30 / край на файла */
} gc_kind_t;

enum { GC_COOL_MIST = 1, GC_COOL_FLOOD = 2, GC_COOL_TSC = 4, GC_COOL_AIR = 8 };   /* TSC: през шпиндела (M88), AIR: въздух (M83) */
enum { GC_PLANE_XY = 17, GC_PLANE_ZX = 18, GC_PLANE_YZ = 19 };

typedef struct {
    gc_kind_t kind;
    int line;
    double tgt[GC_NAX];
    double ctr[3];         /* център на дъгата: X Y Z */
    double feed;           /* мм/мин */
    double val;
    int ival;
    int plane;
    int turns;             /* допълнителни пълни обороти (P − 1) */
    bool cw;
} gc_cmd_t;

typedef void (*gc_emit_fn)(void *ctx, const gc_cmd_t *c);

/* ---------- интерпретатор ---------- */
typedef enum { GC_DIALECT_MACH3, GC_DIALECT_HAAS } gc_dialect_t;
enum { GC_FLOW_NONE, GC_FLOW_CALL, GC_FLOW_CALL_N, GC_FLOW_RET };
#define GC_SUB_DEPTH 16    /* вложени подпрограми */
#define GC_COMP_Q 256      /* команди без XY движение, докато чака сегмент при G41/G42 */

typedef struct {
    gc_dialect_t dialect;
    unsigned axes;         /* маска на активните оси (бит GC_X …) */
    unsigned rotary;       /* маска на въртящите оси (стойностите им са в °, не се умножават по 25,4) */
    bool block_delete;     /* пропускай редовете с '/' */
    gc_emit_fn emit;
    void *ctx;
} gc_config_t;

typedef struct {
    gc_config_t cfg;
    gc_tables_t *tab;

    /* модално състояние */
    int motion;            /* 0, 10, 20, 30 (G0 … G3) */
    int cycle;             /* 0 = няма; 730, 810, 820, 830, 850, 860, 870, 880, 890 (G73, G81 …) */
    /* модалните стойности на цикъла, както са в програмата (мм; в G91: R от началното ниво, Z от R) */
    double cyc_z, cyc_r, cyc_q, cyc_p;     /* дълбочина, равнина R, кълване, пауза (s) */
    double cyc_i, cyc_j, cyc_k;            /* Haas G83/G73: първо кълване, намаление, минимум */
    bool cyc_has_z, cyc_has_r, cyc_has_q, cyc_has_ijk, cyc_incr;
    double cyc_init;                       /* началното ниво (машинно Z) при влизане в цикъла */
    int plane;             /* 17, 18, 19 */
    bool inch, incr, arc_abs, retract_r;   /* G20, G91, G90.1, G99 */
    int wcs;               /* 1 … 105: действащата нула */
    int wcs_prog;          /* нулата, избрана в програмата (G54 = 1) */
    int wcs_map;           /* няколко детайла: програмната G54 → тази нула (0 = без) */
    bool wcs_multi;        /* няколко детайла: програмата може да ползва само G54 */
    bool silent;           /* „Изпълни от ред“: интерпретира, без да изпраща (gc_job.c) */
    bool tlo_on;
    int tlo_h;
    double tlo;
    double g92[GC_NAX];
    double feed;           /* мм/мин, 0 = не е зададено */
    double s;
    int sp_dir;
    int cool;
    int tool, tool_next;   /* в шпиндела / избрания с T (-1 = няма) */
    int path_mode;
    double path_tol;
    bool ovr_en;

    double pos[GC_NAX];    /* машинна позиция (краят на последното движение) */
    bool ended;

    /* компенсация на радиуса G41/G42 (gc_comp.c): чакащият сегмент и командите след него */
    int comp_side;         /* 0, +1 = G41 (отляво), -1 = G42 (отдясно) */
    double comp_r;         /* радиус, мм */
    bool comp_entry;       /* следващото XY движение е влизането */
    bool comp_have;        /* има чакащ сегмент */
    gc_cmd_t comp_seg;     /* чакащият сегмент (програмирани координати) */
    double comp_s[GC_NAX]; /* програмираното му начало */
    double comp_out[GC_NAX];   /* последната изпратена позиция (център на фрезата) */
    gc_cmd_t comp_q[GC_COMP_Q];
    int comp_nq;
    int comp_err_line;
    char comp_err[200];

    /* поток за изпълнителя (gc_run_text): M98 / M97 / M99 в последния ред */
    int flow;              /* GC_FLOW_* */
    int flow_p, flow_l;    /* номер на подпрограмата (или N за M97) и брой повторения */
} gc_interp_t;

void gc_tables_init(gc_tables_t *t);
void gc_interp_init(gc_interp_t *in, const gc_config_t *cfg, gc_tables_t *tab, const double start_mach[GC_NAX]);
/* Изпълнява един ред. 0 = ok, -1 = грешка (e е попълнена). След M2/M30 in->ended е true. */
int gc_interp_line(gc_interp_t *in, const char *text, int line, gc_err_t *e);
/* Изпълнява цял текст (редовете от 0) с подпрограмите O / M98 P L / M97 P / M99 (до GC_SUB_DEPTH нива);
 * спира на M2/M30, на втори '%' или в края (тогава вади GC_END). */
int gc_run_text(gc_interp_t *in, const char *text, gc_err_t *e);

/* Няколко детайла и „Изпълни от ред“ (gc_job.c, docs/interpretator-planer.md):
 *  - parts: нулите на детайлите (1 = G54, 2 = G55 …); програмната G54 се пренасочва към нулата на
 *    текущия детайл. Програмата се дели на блокове при M6: смяната е веднъж, после блокът минава
 *    по всички детайли (без M6: цялата програма по всеки детайл). В началото на детайла: GC_PART.
 *  - start_line (от 0) и start_part: всичко преди тях се интерпретира без изход, после безопасен
 *    подход (Z на safe_z, инструмент, шпиндел, охлаждане, XY, бързо до approach мм над началото,
 *    с подаване до него) и оттам нататък нормално. Редът може да е и в подпрограма. */
typedef struct {
    const int *parts;
    int nparts;            /* 0 или 1: един детайл */
    int start_line;        /* < 0: от началото */
    int start_part;        /* индекс в parts */
    double safe_z;         /* машинно Z за подхода */
    double approach;       /* мм над началото, от които се слиза с подаване */
} gc_job_t;
int gc_run_job(gc_interp_t *in, const char *text, const gc_job_t *job, gc_err_t *e);

/* Програмна позиция по оста (в мм или °, без инчове) */
double gc_prog_pos(const gc_interp_t *in, int ax);

/* Текстов вид на командата (за тестовете и gcsim); показва само осите от маската */
int gc_cmd_format(const gc_cmd_t *c, unsigned axes, char *buf, size_t n);
const char *gc_axis_name(int ax);

#ifdef __cplusplus
}
#endif

#endif
