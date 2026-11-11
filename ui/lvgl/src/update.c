/* Обновяване на интерфейса (прозорецът „Обновяване“, знакът в горната лента, „Софтуер“ в Диагностика).
 * Работата е в tools/cnc-update.sh (на клиентските машини: cnc-update до програмата). Той се пуска като
 * отделен процес; изходът му се чете ред по ред без блокиране от таймера на интерфейса. */
#include "update.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "machine.h"
#include "ui.h"

upd_t UPD;

#define CHECK_EVERY_MS (6u * 3600u * 1000u)
#define FIRST_CHECK_MS 15000u

typedef enum { J_NONE, J_STATUS, J_CHECK, J_CHECK_BG, J_DL, J_INST, J_ROLL } job_t;

static char tool[PATH_MAX], bin[PATH_MAX];
static pid_t pid;
static int fd = -1;
static char lbuf[256];
static size_t ln;
static job_t job;
static bool job_ok, cancelled, toast_pend, have_ready, first_tick = true;
static char ready_lbl[40];
static uint32_t next_check, restart_at;

bool update_machine_busy(void) { return M.state != MS_IDLE; }

static bool usable(const char *p) { return p && *p && access(p, R_OK) == 0; }

static void find_tool(void)
{
    ssize_t n = readlink("/proc/self/exe", bin, sizeof bin - 1);
    bin[n > 0 ? n : 0] = 0;
    const char *e = getenv("CNC_UPDATE");
    if (usable(e)) { snprintf(tool, sizeof tool, "%s", e); return; }
    char dir[PATH_MAX];
    snprintf(dir, sizeof dir, "%s", bin);
    char *sl = strrchr(dir, '/');
    if (sl) *sl = 0;
    /* до програмата (клиентските машини), после в хранилището (ui/lvgl/build → tools) */
    static const char *const C[] = {"%s/cnc-update", "%s/../../../tools/cnc-update.sh", "/usr/local/bin/cnc-update"};
    for (unsigned i = 0; i < sizeof C / sizeof C[0]; i++) {
        char p[PATH_MAX + 64];
        snprintf(p, sizeof p, C[i], dir);
        if (usable(p)) { snprintf(tool, sizeof tool, "%.4000s", p); return; }
    }
    tool[0] = 0;
}

static bool start(job_t j, const char *cmd)
{
    if (pid || !tool[0] || !bin[0]) return false;
    int p[2];
    if (pipe(p) != 0) return false;
    pid_t c = fork();
    if (c < 0) { close(p[0]); close(p[1]); return false; }
    if (c == 0) {
        setpgid(0, 0);   /* „Отказ“ спира и curl/cmake, които скриптът е пуснал */
        dup2(p[1], 1);
        int nul = open("/dev/null", O_RDWR);
        if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); }
        close(p[0]);
        close(p[1]);
        const char *ch = cfg_str("upd.upCh");
        execl("/bin/bash", "bash", tool, "--bin", bin, "--ch", ch && *ch ? ch : "stable", cmd, (char *)NULL);
        _exit(127);
    }
    setpgid(c, c);
    close(p[1]);
    fd = p[0];
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    pid = c;
    job = j;
    job_ok = false;
    cancelled = false;
    ln = 0;
    UPD.err[0] = 0;
    return true;
}

static void stamp(void)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(UPD.checked, sizeof UPD.checked, "%d.%m.%Y %H:%M", &tm);
}

static void line(char *l)
{
    if (!strncmp(l, "CUR ", 4)) snprintf(UPD.cur, sizeof UPD.cur, "%.39s", l + 4);
    else if (!strncmp(l, "PREV", 4)) snprintf(UPD.prev, sizeof UPD.prev, "%.39s", l[4] ? l + 5 : "");
    else if (!strncmp(l, "READY ", 6)) { have_ready = true; snprintf(ready_lbl, sizeof ready_lbl, "%.39s", l + 6); }
    else if (!strncmp(l, "NEW ", 4)) {
        char v[40] = "";
        int c = 0;
        sscanf(l + 4, "%39s %d", v, &c);
        bool fresh = strcmp(v, UPD.avail) != 0;
        snprintf(UPD.avail, sizeof UPD.avail, "%s", v);
        UPD.crit = c != 0;
        if (fresh) toast_pend = true;
        job_ok = true;
    } else if (!strcmp(l, "NONE")) { UPD.avail[0] = 0; UPD.crit = false; job_ok = true; }
    else if (!strncmp(l, "P ", 2)) { int v = atoi(l + 2); UPD.pc = v < 0 ? 0 : v > 100 ? 100 : v; }
    else if (!strcmp(l, "OK")) job_ok = true;
    else if (!strncmp(l, "ERR ", 4)) snprintf(UPD.err, sizeof UPD.err, "%.159s", l + 4);
}

static void finish(uint32_t now)
{
    job_t j = job;
    job = J_NONE;
    if (j == J_STATUS) {
        /* изтеглено от преди: „Обновяване“ показва направо „Инсталирай“ */
        if (have_ready && UPD.avail[0] == 0) snprintf(UPD.avail, sizeof UPD.avail, "%s", ready_lbl);
        return;
    }
    if (j == J_CHECK || j == J_CHECK_BG) {
        if (job_ok) stamp();
        if (j == J_CHECK_BG) return;   /* фоновата проверка мълчи при грешка */
        if (!job_ok) { UPD.ph = UP_ERR; return; }
        if (UPD.avail[0]) { UPD.ph = UP_DL; UPD.pc = 0; toast_pend = false; start(J_DL, "download"); }
        else UPD.ph = UP_NONE;
        return;
    }
    if (j == J_DL) {
        if (cancelled) { UPD.ph = UP_IDLE; return; }
        if (job_ok && !UPD.err[0]) { UPD.ph = UP_READY; UPD.pc = 100; have_ready = true; snprintf(ready_lbl, sizeof ready_lbl, "%s", UPD.avail); }
        else UPD.ph = UP_ERR;
        return;
    }
    if (j == J_INST || j == J_ROLL) {
        if (job_ok && !UPD.err[0]) {
            UPD.ph = j == J_INST ? UP_RESTART : UP_IDLE;
            UPD.pc = 100;
            restart_at = now + 1500;
            have_ready = false;
            if (j == J_ROLL) machine_toast("Върната е предишната версия. Интерфейсът се рестартира.", now, 3000);
        } else if (j == J_INST) UPD.ph = UP_ERR;
        else {
            char t[200];
            snprintf(t, sizeof t, "Предишната версия не може да се върне: %.100s", UPD.err[0] ? UPD.err : "грешка");
            machine_toast(t, now, 5000);
        }
    }
}

static void poll_job(uint32_t now)
{
    if (!pid) return;
    for (;;) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 1) {
            if (c == '\n') { lbuf[ln] = 0; line(lbuf); ln = 0; }
            else if (ln + 1 < sizeof lbuf) lbuf[ln++] = c;
            continue;
        }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        if (r < 0 && errno == EINTR) continue;
        break;   /* EOF: процесът е свършил */
    }
    close(fd);
    fd = -1;
    int st;
    waitpid(pid, &st, 0);
    pid = 0;
    finish(now);
}

void update_init(void)
{
    memset(&UPD, 0, sizeof UPD);
    find_tool();
    UPD.no_tool = !tool[0];
#ifdef CNC_VERSION
    snprintf(UPD.cur, sizeof UPD.cur, "%s", CNC_VERSION);
#endif
    start(J_STATUS, "status");
}

void update_tick(uint32_t now)
{
    if (first_tick) { first_tick = false; next_check = now + FIRST_CHECK_MS; }
    poll_job(now);
    if (restart_at && (int32_t)(now - restart_at) >= 0) {
        restart_at = 0;
        if (getenv("INVOCATION_ID")) { raise(SIGTERM); return; }   /* под systemd: излиза, systemd пуска новата */
        /* ръчно пуснат (или симулаторът): само обновява надписите */
        if (UPD.ph == UP_RESTART) {
            char t[160];
            snprintf(t, sizeof t, "Обновено до версия %s. Пуснете програмата отново.", UPD.avail);
            machine_toast(t, now, 6000);
        }
        UPD.ph = UP_IDLE;
        UPD.avail[0] = 0;
        UPD.crit = false;
        start(J_STATUS, "status");
    }
    if (!pid && cfg_num("upd.upAuto") && (int32_t)(now - next_check) >= 0 && (UPD.ph == UP_IDLE || UPD.ph == UP_NONE)) {
        next_check = now + CHECK_EVERY_MS;
        start(J_CHECK_BG, "check");
    }
    if (toast_pend && UPD.avail[0] && !update_machine_busy() && UPD.ph == UP_IDLE) {
        toast_pend = false;
        char t[160];
        snprintf(t, sizeof t, "%s %s. Натиснете „Обновяване“ горе.", UPD.crit ? "Важно обновление: версия" : "Има нова версия", UPD.avail);
        machine_toast(t, now, 6000);
    }
}

void update_open(void)
{
    if (UPD.ph == UP_DL || UPD.ph == UP_INST || UPD.ph == UP_RESTART || UPD.ph == UP_CHECK) { dlg_update(); return; }
    if (have_ready && UPD.avail[0]) { UPD.ph = UP_READY; UPD.pc = 100; dlg_update(); return; }
    if (UPD.no_tool) { snprintf(UPD.err, sizeof UPD.err, "обновяването не е настроено"); UPD.ph = UP_ERR; dlg_update(); return; }
    if (pid) {   /* тече фонова проверка: тя се прекъсва и започва наново */
        kill(-pid, SIGTERM);
        while (pid) poll_job(0);
    }
    UPD.pc = 0;
    if (UPD.avail[0]) { UPD.ph = UP_DL; start(J_DL, "download"); }
    else { UPD.ph = UP_CHECK; start(J_CHECK, "check"); }
    dlg_update();
}

void update_cancel(void)
{
    if ((job == J_DL || job == J_CHECK) && pid) { cancelled = true; kill(-pid, SIGTERM); }
    if (UPD.ph != UP_INST && UPD.ph != UP_RESTART) UPD.ph = UP_IDLE;
}

void update_install(void)
{
    if (UPD.ph == UP_ERR) {   /* „Опитай отново“ */
        UPD.ph = UP_IDLE;
        update_open();
        return;
    }
    if (UPD.ph != UP_READY || update_machine_busy() || pid) return;
    UPD.ph = UP_INST;
    UPD.pc = 0;
    if (!start(J_INST, "install")) UPD.ph = UP_ERR;
}

void update_rollback(void)
{
    uint32_t now = lv_tick_get();
    if (update_machine_busy()) { machine_toast("Спрете машината", now, 3000); return; }
    if (!UPD.prev[0]) { machine_toast("Няма предишна версия", now, 3000); return; }
    if (pid) { machine_toast("Изчакайте: тече обновяване", now, 3000); return; }
    start(J_ROLL, "rollback");
}
