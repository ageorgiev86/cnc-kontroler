#include "io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "machine.h"

const char *const IN_F[] = {"none", "Не се използва", "lubeP", "Налягане на мазането", "door", "Врата затворена", "air", "Налягане въздух",
    "oil", "Ниво масло", "start", "Бутон Старт", "hold", "Бутон Пауза", "clamp", "Детайл затегнат", "vac", "Вакуум ОК", "pallet", "Палет на място",
    "chip", "Контейнер за стружки пълен", "user", "Потребителски (M66)", NULL};
const char *const OUT_F[] = {"none", "Не се използва", "cool", "Охлаждане", "mist", "Мъгла", "air", "Обдухване", "lube", "Мазане", "dust", "Аспирация",
    "lgreen", "Сигнална колона: зелено", "lyellow", "Сигнална колона: жълто", "lred", "Сигнална колона: червено", "clamp", "Стяга",
    "vac1", "Вакуумна маса: зона 1", "vac2", "Вакуумна маса: зона 2", "conv", "Транспортьор стружки", "user", "Потребителски (M62–M65)", NULL};
const char *const AIN_F[] = {"none", "Не се използва", "airP", "Налягане на въздуха", "oilP", "Налягане на маслото", "temp", "Температура",
    "load", "Натоварване на шпиндела", "flow", "Дебит на охлаждането", "user", "Потребителски (M66 E)", NULL};
const char *const AOUT_F[] = {"none", "Не се използва", "spindle", "Обороти на шпиндела", "user", "Потребителски (M68)", NULL};
const char *const SIG_T[] = {"010", "0–10 V", "05", "0–5 V", "420", "4–20 mA", NULL};
const char *const AACT_T[] = {"none", "Нищо", "warn", "Предупреждение", "alarm", "Аларма", NULL};

int io_force[IO_N];
int io_force_arm = -1;
bool air_drop;
static double ai_v[AIN_N];
static int ai_al[AIN_N];

int opt_index(const char *const *o, const char *k)
{
    for (int i = 0; o[i]; i += 2)
        if (!strcmp(o[i], k)) return i / 2;
    return 0;
}
const char *opt_label(const char *const *o, const char *k) { return o[opt_index(o, k) * 2 + 1]; }
int opt_count(const char *const *o) { int n = 0; while (o[n * 2]) n++; return n; }

static void set(const char *k, const char *v) { cfg_add(k, v); }

void io_defaults(void)
{
    static const char *const I[][2] = {{"Врата затворена", "door"}, {"Налягане въздух", "air"}, {"Ниво масло", "oil"}, {"Бутон Старт (пулт)", "start"},
        {"Бутон Пауза (пулт)", "hold"}, {"Детайл затегнат", "clamp"}, {"Вакуум ОК", "vac"}, {"Палет на място", "pallet"}, {"Контейнер за стружки пълен", "chip"}};
    static const char *const O[][2] = {{"Охлаждане", "cool"}, {"Мъгла", "mist"}, {"Обдухване", "air"}, {"Мазане", "lube"}, {"Аспирация", "dust"},
        {"Сигнална колона: зелено", "lgreen"}, {"Сигнална колона: жълто", "lyellow"}, {"Сигнална колона: червено", "lred"}, {"Стяга", "clamp"},
        {"Вакуумна маса: зона 1", "vac1"}, {"Вакуумна маса: зона 2", "vac2"}, {"Транспортьор стружки", "conv"}};
    char k[32];
    for (int i = 0; i < IO_N; i++) {
        snprintf(k, sizeof k, "io.in%d.n", i + 1); set(k, i < 9 ? I[i][0] : "");
        snprintf(k, sizeof k, "io.in%d.f", i + 1); set(k, i < 9 ? I[i][1] : "none");
        snprintf(k, sizeof k, "io.in%d.inv", i + 1); set(k, "0");
        snprintf(k, sizeof k, "io.in%d.flt", i + 1); set(k, "3");
        snprintf(k, sizeof k, "io.out%d.n", i + 1); set(k, i < 12 ? O[i][0] : "");
        snprintf(k, sizeof k, "io.out%d.f", i + 1); set(k, i < 12 ? O[i][1] : "none");
        snprintf(k, sizeof k, "io.out%d.inv", i + 1); set(k, "0");
        snprintf(k, sizeof k, "io.out%d.safe", i + 1); set(k, "off");
        io_force[i] = -1;
    }
    static const char *const A[AIN_N][10] = {
        {"Налягане въздух", "airP", "420", "0", "10", "bar", "5.5", "9", "alarm", "200"},
        {"Температура на шпиндела", "temp", "010", "0", "100", "°C", "0", "60", "warn", "1000"},
        {"", "none", "010", "0", "10", "V", "0", "10", "none", "100"},
        {"", "none", "010", "0", "10", "V", "0", "10", "none", "100"}};
    static const char *const AK[10] = {"n", "f", "sig", "lo", "hi", "u", "amin", "amax", "act", "flt"};
    for (int i = 0; i < AIN_N; i++)
        for (int j = 0; j < 10; j++) { snprintf(k, sizeof k, "io.ain%d.%s", i + 1, AK[j]); set(k, A[i][j]); }
    for (int i = 0; i < AOUT_N; i++)
        for (int j = 0; j < 6; j++) {
            static const char *const D[6] = {"", "none", "010", "0", "100", "%"};
            snprintf(k, sizeof k, "io.aout%d.%s", i + 1, AK[j]);
            set(k, D[j]);
        }
}

static const char *cs(const char *fmt, int i, const char *f)
{
    static char k[32];
    snprintf(k, sizeof k, fmt, i + 1, f);
    return cfg_str(k);
}
static double cn(const char *fmt, int i, const char *f) { return atof(cs(fmt, i, f)); }

const char *io_name(bool out, int i, char *buf, int n)
{
    const char *nm = cs(out ? "io.out%d.%s" : "io.in%d.%s", i, "n"), *f = cs(out ? "io.out%d.%s" : "io.in%d.%s", i, "f");
    if (nm[0]) snprintf(buf, n, "%s", nm);
    else if (strcmp(f, "none")) snprintf(buf, n, "%s", opt_label(out ? OUT_F : IN_F, f));
    else buf[0] = 0;
    return buf;
}

static bool out_fn(const char *f)
{
    bool st_run = M.state == MS_RUN;
    if (!strcmp(f, "cool")) return M.coolant;
    if (!strcmp(f, "dust")) return M.sp_on;
    if (!strcmp(f, "lgreen")) return st_run;
    if (!strcmp(f, "lyellow")) return M.enabled && (M.state == MS_IDLE || M.state == MS_HOLD);
    if (!strcmp(f, "lred")) return M.state == MS_ALARM || M.state == MS_ESTOP || !M.enabled;
    if (!strcmp(f, "clamp")) return true;
    if (!strcmp(f, "conv")) return M.sp_on;
    return false;
}

bool io_out(int i)
{
    if (io_force[i] >= 0) return io_force[i];
    const char *f = cs("io.out%d.%s", i, "f");
    if (!M.enabled) return !strcmp(f, "lred");
    if (M.state == MS_ESTOP && !strcmp(cs("io.out%d.%s", i, "safe"), "off")) return false;
    return out_fn(f);
}

bool io_in(int i)
{
    const char *f = cs("io.in%d.%s", i, "f");
    if (!strcmp(f, "oil") || !strcmp(f, "door") || !strcmp(f, "pallet")) return true;
    if (!strcmp(f, "air")) return !air_drop;
    if (!strcmp(f, "start") || !strcmp(f, "hold") || !strcmp(f, "chip") || !strcmp(f, "lubeP")) return false;
    if (!strcmp(f, "clamp") || !strcmp(f, "vac")) {
        for (int k = 0; k < IO_N; k++) {
            const char *o = cs("io.out%d.%s", k, "f");
            if (((!strcmp(f, "clamp") && !strcmp(o, "clamp")) || (!strcmp(f, "vac") && (!strcmp(o, "vac1") || !strcmp(o, "vac2")))) && io_out(k)) return true;
        }
        return false;
    }
    return (i * 7) % 5 == 0;
}

static double ai_true(int i)
{
    const char *f = cs("io.ain%d.%s", i, "f");
    double n = (double)rand() / RAND_MAX - 0.5, spmax = cfg_num("sp.spMax");
    if (!strcmp(f, "airP")) return (air_drop ? 4.6 : 6.2) + n * 0.06;
    if (!strcmp(f, "temp")) return 24 + M.sp_act / (spmax > 1 ? spmax : 1) * 30 + n * 0.2;
    if (!strcmp(f, "load")) return M.sp_on ? 22 + M.feed_act / 200 + n * 2 : 0;
    if (!strcmp(f, "flow")) return M.coolant ? 3.2 + n * 0.1 : 0;
    if (!strcmp(f, "user")) return cn("io.ain%d.%s", i, "lo");
    return 0;
}

double aio_in(int i) { return ai_v[i]; }

double aio_out(int i)
{
    const char *f = cs("io.aout%d.%s", i, "f");
    double lo = cn("io.aout%d.%s", i, "lo"), hi = cn("io.aout%d.%s", i, "hi"), spmax = cfg_num("sp.spMax");
    if (!strcmp(f, "spindle")) return lo + (hi - lo) * fmin(1, M.sp_act / (spmax > 1 ? spmax : 1));
    return 0;
}

void aio_sig(bool out, int i, double v, char *b, int n)
{
    const char *fmt = out ? "io.aout%d.%s" : "io.ain%d.%s";
    double lo = cn(fmt, i, "lo"), hi = cn(fmt, i, "hi"), f = (v - lo) / ((hi - lo) != 0 ? hi - lo : 1);
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    const char *s = cs(fmt, i, "sig");
    if (!strcmp(s, "420")) snprintf(b, n, "%.2f mA", 4 + 16 * f);
    else if (!strcmp(s, "05")) snprintf(b, n, "%.2f V", 5 * f);
    else snprintf(b, n, "%.2f V", 10 * f);
}

/* филтър и граници на аналоговите входове: предупреждение или аларма */
void io_tick(float dt, uint32_t now)
{
    for (int i = 0; i < AIN_N; i++) {
        if (!strcmp(cs("io.ain%d.%s", i, "f"), "none")) { ai_v[i] = 0; continue; }
        static bool init[AIN_N];
        if (!init[i]) { ai_v[i] = ai_true(i); init[i] = true; }   /* без преходен процес при старта */
        double k = fmin(1, dt * 1000 / fmax(10, cn("io.ain%d.%s", i, "flt")));
        ai_v[i] += (ai_true(i) - ai_v[i]) * k;
        double v = ai_v[i], mn = cn("io.ain%d.%s", i, "amin"), mx = cn("io.ain%d.%s", i, "amax");
        int bad = v < mn ? -1 : v > mx ? 1 : 0;
        const char *act = cs("io.ain%d.%s", i, "act");
        if (bad && !ai_al[i] && strcmp(act, "none") && M.enabled) {
            ai_al[i] = 1;
            char nm[64], t[200];
            const char *n0 = cs("io.ain%d.%s", i, "n");
            snprintf(nm, sizeof nm, "%s", n0[0] ? n0 : opt_label(AIN_F, cs("io.ain%d.%s", i, "f")));
            const char *u = cs("io.ain%d.%s", i, "u");
            snprintf(t, sizeof t, "%s: %.2f %s %s %g %s", nm, v, u, bad < 0 ? "под" : "над", bad < 0 ? mn : mx, u);
            if (!strcmp(act, "alarm")) machine_alarm(t);
            else { machine_toast(t, now, 6000); M.msg_kind = MSG_BAD; }
        }
        if (!bad) ai_al[i] = 0;
    }
}
