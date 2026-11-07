/* Входове и изходи: 32 + 32 цифрови, 4 + 2 аналогови (Настройки → Входове/изходи, Диагностика).
 * Настройките им са в config (ключове io.in1.n, io.out3.f, io.ain1.lo …). Стойностите тук са демо. */
#ifndef CNC_IO_H
#define CNC_IO_H

#include <stdbool.h>
#include <stdint.h>

#define IO_N 32
#define AIN_N 4
#define AOUT_N 2

/* функции: ключ, надпис, NULL */
extern const char *const IN_F[], *const OUT_F[], *const AIN_F[], *const AOUT_F[], *const SIG_T[], *const AACT_T[];
const char *opt_label(const char *const *opts, const char *key);
int opt_index(const char *const *opts, const char *key);
int opt_count(const char *const *opts);

void io_defaults(void);                 /* стойностите по подразбиране в config */
const char *io_name(bool out, int i, char *buf, int n);   /* име или функция; "" = резерв */
bool io_in(int i);
bool io_out(int i);
extern int io_force[IO_N];              /* -1 автоматично, 0/1 ръчно */
extern int io_force_arm;
extern bool air_drop;
double aio_in(int i);                   /* филтрирана стойност в мерни единици */
double aio_out(int i);
void aio_sig(bool out, int i, double v, char *buf, int n);   /* стойността като ток/напрежение */
void io_tick(float dt, uint32_t now);

#endif
