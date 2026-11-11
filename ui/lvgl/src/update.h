/* Обновяване на интерфейса: пуска tools/cnc-update.sh във фонов процес и чете напредъка му.
 * Изтеглянето и инсталирането не спират интерфейса; инсталира се само при спряна машина. */
#ifndef CNC_UPDATE_H
#define CNC_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { UP_IDLE, UP_CHECK, UP_NONE, UP_DL, UP_READY, UP_INST, UP_RESTART, UP_ERR } up_phase_t;

typedef struct {
    up_phase_t ph;         /* какво показва прозорецът */
    int pc;                /* напредък 0..100 */
    char cur[40], prev[40], avail[40];   /* инсталирана, предишна, нова версия ("" = няма) */
    bool crit;             /* новата версия е важна */
    char checked[24];      /* кога е проверено последно */
    char err[160];
    bool no_tool;          /* няма скрипт за обновяване */
} upd_t;

extern upd_t UPD;

void update_init(void);
void update_tick(uint32_t now);   /* фоновата проверка и четенето на процеса */
void update_open(void);           /* бутонът „Обновяване“: проверка и изтегляне */
void update_cancel(void);         /* „Отказ“: спира изтеглянето */
void update_install(void);        /* „Инсталирай“ (или „Опитай отново“ след грешка) */
void update_rollback(void);       /* „Върни предишната версия“ */
bool update_machine_busy(void);

#endif
