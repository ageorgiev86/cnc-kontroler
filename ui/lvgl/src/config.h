/* Настройките на машината (екранът „Настройки“ в макета).
 * Параметрите са описани в таблици (config_defs.c, генерирани от ui/maket.html). Всяка стойност има
 * записано състояние (cfg) и чернова (draft): екранът променя черновата, „Запиши“ я прехвърля в cfg,
 * „Отмени промените“ я връща. Ключовете са „раздел.параметър“ (mag.mn), а за осите „ос.параметър“ (X.spm). */
#ifndef CNC_CONFIG_H
#define CNC_CONFIG_H

#include <stdbool.h>
#include <string.h>

typedef enum { PT_HDR, PT_BOOL, PT_DIR, PT_NUM, PT_ENUM, PT_TEXT, PT_IP, PT_PASS } ptype_t;

typedef struct {
    const char *k, *n, *u;        /* ключ, име, мерна единица */
    ptype_t t;
    int d;                        /* знаци след точката */
    double min, max;              /* max < 0: изчислява се (виж cfg_max) */
    const char *on, *off;         /* надписи за bool */
    int rev;                      /* „обратна“ посока се оцветява */
    const char *const *opts;      /* за enum: ключ, надпис, …, NULL */
    const char *h, *ref;          /* обяснение и съответствие в LinuxCNC/grblHAL/Mach3 */
    bool (*show)(void);           /* NULL = винаги */
    const char *def;              /* стойност по подразбиране */
    const char *adef[6];          /* за параметрите на осите: по ос */
} pdef_t;

extern const pdef_t P_AX[], P_HOME[], P_ENC[], P_MAG[], P_TS[], P_NET[], P_MACH[], P_SP[], P_PRB[], P_DISP[], P_LUBE[], P_UPD[];
extern const int P_AX_N, P_HOME_N, P_ENC_N, P_MAG_N, P_TS_N, P_NET_N, P_MACH_N, P_SP_N, P_PRB_N, P_DISP_N, P_LUBE_N, P_UPD_N;

void cfg_init(void);
void cfg_add(const char *key, const char *val);   /* нов параметър (ако го няма) */
void cfg_set_file(const char *path);     /* къде се пазят настройките; NULL = само в паметта */

double cfg_num(const char *key);
const char *cfg_str(const char *key);
double drf_num(const char *key);
const char *drf_str(const char *key);
void drf_set(const char *key, const char *val);
void drf_set_num(const char *key, double v, int dec);
bool cfg_changed(const char *key);       /* черновата се различава от записаното */
bool cfg_dirty(void);
bool cfg_changed_prefix(const char *prefix);
void cfg_commit(void);                   /* draft → cfg и запис във файла */
void cfg_revert(void);                   /* cfg → draft */
void cfg_commit_key(const char *key);    /* един параметър направо (превключвателите извън настройките) */
double cfg_max(const pdef_t *p);

#define DN(k) drf_num(k)
#define DS_IS(k, v) (strcmp(drf_str(k), (v)) == 0)

#endif
