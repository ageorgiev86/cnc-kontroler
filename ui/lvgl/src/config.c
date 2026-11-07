#include "config.h"
#include "io.h"

#include <stdio.h>
#include <stdlib.h>

#define VAL_LEN 72
#define HASH_N 1024

typedef struct {
    char key[28];
    char cfg[VAL_LEN], drf[VAL_LEN];
} entry_t;

static entry_t *E;
static int n_e, cap_e;
static short H[HASH_N];   /* индекс + 1 */
static char file_path[256];

static unsigned hash(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) h = (h ^ (unsigned char)*s++) * 16777619u;
    return h;
}

static entry_t *find(const char *key)
{
    for (unsigned h = hash(key) % HASH_N, i = 0; i < HASH_N; i++, h = (h + 1) % HASH_N) {
        if (!H[h]) return NULL;
        entry_t *e = &E[H[h] - 1];
        if (!strcmp(e->key, key)) return e;
    }
    return NULL;
}

static void add(const char *key, const char *val)
{
    if (find(key)) return;
    if (n_e >= cap_e) {
        cap_e = cap_e ? cap_e * 2 : 256;
        E = realloc(E, (size_t)cap_e * sizeof *E);
        if (!E) abort();
    }
    entry_t *e = &E[n_e++];
    snprintf(e->key, sizeof e->key, "%s", key);
    snprintf(e->cfg, VAL_LEN, "%s", val ? val : "");
    snprintf(e->drf, VAL_LEN, "%s", val ? val : "");
    unsigned h = hash(key) % HASH_N;
    while (H[h]) h = (h + 1) % HASH_N;
    H[h] = (short)n_e;
}

void cfg_add(const char *key, const char *val) { add(key, val); }

static void add_sec(const char *sec, const pdef_t *p, int n)
{
    char k[40];
    for (int i = 0; i < n; i++) {
        if (!p[i].k) continue;
        snprintf(k, sizeof k, "%s.%s", sec, p[i].k);
        add(k, p[i].def ? p[i].def : "");
    }
}

static void add_ax(const pdef_t *p, int n)
{
    static const char *const AXN[6] = {"X", "Y", "Z", "A", "B", "C"};
    char k[40];
    for (int i = 0; i < n; i++)
        for (int a = 0; a < 6; a++) {
            snprintf(k, sizeof k, "%s.%s", AXN[a], p[i].k);
            add(k, p[i].adef[a] ? p[i].adef[a] : "0");
        }
}

static void load_file(void);

void cfg_init(void)
{
    if (n_e) return;
    add_ax(P_AX, P_AX_N);
    add_ax(P_HOME, P_HOME_N);
    add_ax(P_ENC, P_ENC_N);
    add_sec("mag", P_MAG, P_MAG_N);
    add_sec("ts", P_TS, P_TS_N);
    add_sec("net", P_NET, P_NET_N);
    add_sec("mach", P_MACH, P_MACH_N);
    add_sec("sp", P_SP, P_SP_N);
    add_sec("prb", P_PRB, P_PRB_N);
    add_sec("disp", P_DISP, P_DISP_N);
    add_sec("lube", P_LUBE, P_LUBE_N);
    /* достъп: PIN-овете на нивата и автоматичен изход (макетът: 1111 и 2222) */
    add("acc.pin1", "1111");
    add("acc.pin2", "2222");
    add("acc.tmo", "10");
    io_defaults();   /* входовете и изходите: преди четенето на файла */
}

static const char *val(const char *key, bool draft)
{
    entry_t *e = find(key);
    if (!e) return "";
    return draft ? e->drf : e->cfg;
}

double cfg_num(const char *key) { return atof(val(key, false)); }
const char *cfg_str(const char *key) { return val(key, false); }
double drf_num(const char *key) { return atof(val(key, true)); }
const char *drf_str(const char *key) { return val(key, true); }

void drf_set(const char *key, const char *v)
{
    entry_t *e = find(key);
    if (!e) { add(key, v); e = find(key); if (e) e->cfg[0] = 0; return; }
    snprintf(e->drf, VAL_LEN, "%s", v ? v : "");
}

void drf_set_num(const char *key, double v, int dec)
{
    char b[48];
    snprintf(b, sizeof b, "%.*f", dec, v);
    /* без излишни нули, за да съвпада с макета (0.15, а не 0.150) */
    if (strchr(b, '.')) {
        char *p = b + strlen(b) - 1;
        while (*p == '0') *p-- = 0;
        if (*p == '.') *p = 0;
    }
    if (!strcmp(b, "-0")) strcpy(b, "0");
    drf_set(key, b);
}

static bool same(const entry_t *e)
{
    if (!strcmp(e->cfg, e->drf)) return true;
    char *a, *b;
    double x = strtod(e->cfg, &a), y = strtod(e->drf, &b);
    return a != e->cfg && b != e->drf && !*a && !*b && x == y;
}

bool cfg_changed(const char *key)
{
    entry_t *e = find(key);
    return e && !same(e);
}

bool cfg_changed_prefix(const char *prefix)
{
    size_t n = strlen(prefix);
    for (int i = 0; i < n_e; i++)
        if (!strncmp(E[i].key, prefix, n) && !same(&E[i])) return true;
    return false;
}

bool cfg_dirty(void)
{
    for (int i = 0; i < n_e; i++)
        if (!same(&E[i])) return true;
    return false;
}

void cfg_set_file(const char *path)
{
    snprintf(file_path, sizeof file_path, "%s", path ? path : "");
    if (file_path[0]) load_file();
}

static void load_file(void)
{
    FILE *f = fopen(file_path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        char *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        entry_t *e = find(line);
        if (!e) continue;   /* непознат ключ (стара версия) */
        snprintf(e->cfg, VAL_LEN, "%s", v);
        snprintf(e->drf, VAL_LEN, "%s", v);
    }
    fclose(f);
}

static void write_file(void);

void cfg_commit(void)
{
    for (int i = 0; i < n_e; i++) memcpy(E[i].cfg, E[i].drf, VAL_LEN);
    write_file();
}

static void write_file(void)
{
    if (!file_path[0]) return;
    char tmp[300];
    snprintf(tmp, sizeof tmp, "%s.tmp", file_path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "# Настройки на CNC интерфейса (записва ги екранът „Настройки“)\n");
    for (int i = 0; i < n_e; i++) fprintf(f, "%s=%s\n", E[i].key, E[i].cfg);
    if (fclose(f) == 0) rename(tmp, file_path);
}

void cfg_commit_key(const char *key)
{
    entry_t *e = find(key);
    if (!e) return;
    memcpy(e->cfg, e->drf, VAL_LEN);
    write_file();
}

void cfg_revert(void)
{
    for (int i = 0; i < n_e; i++) memcpy(E[i].drf, E[i].cfg, VAL_LEN);
}

double cfg_max(const pdef_t *p)
{
    if (p->max >= 0 || !p->k) return p->max;
    if (!strcmp(p->k, "mn")) return DN("mag.mOn") && DS_IS("mag.mtype", "linear") ? 60 : 255;
    return 1e9;
}
