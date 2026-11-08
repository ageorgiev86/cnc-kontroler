#include "net_apply.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CNC_NET "/usr/local/sbin/cnc-net"

#if !CNC_UI_SIM
/* пуска sudo -n cnc-net ... и взима последния ред от изхода */
static bool run(const char *const argv[], char *msg, size_t n)
{
    int pfd[2];
    if (pipe(pfd)) { snprintf(msg, n, "Мрежа: няма pipe"); return false; }
    pid_t pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); snprintf(msg, n, "Мрежа: няма fork"); return false; }
    if (pid == 0) {
        dup2(pfd[1], 1);
        dup2(pfd[1], 2);
        close(pfd[0]);
        close(pfd[1]);
        execv("/usr/bin/sudo", (char *const *)argv);
        printf("ГРЕШКА: няма sudo\n");
        _exit(127);
    }
    close(pfd[1]);
    char buf[1024], last[300] = "";
    size_t len = 0;
    ssize_t r;
    while ((r = read(pfd[0], buf + len, sizeof buf - 1 - len)) > 0) {
        len += (size_t)r;
        if (len >= sizeof buf - 1) len = 0;   /* много дълъг изход: пазим края */
    }
    close(pfd[0]);
    buf[len] = 0;
    for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n"))
        if (*l) snprintf(last, sizeof last, "%s", l);
    int st = 0;
    waitpid(pid, &st, 0);
    bool ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
    if (!ok && (strstr(last, "password") || strstr(last, "парола") || strstr(last, "sudo:") || access(CNC_NET, X_OK)))
        snprintf(msg, n, "Мрежата не се сменя от интерфейса. Еднократно по SSH: sudo bash tools/cnc-net.sh --install");
    else
        snprintf(msg, n, "%s", last[0] ? last : (ok ? "ОК" : "ГРЕШКА при смяната на мрежата"));
    return ok;
}
#endif

bool net_apply(bool addr, bool host, char *msg, size_t n)
{
#if CNC_UI_SIM
    snprintf(msg, n, "Симулатор: мрежата на компютъра не се сменя (%s)", !strcmp(cfg_str("net.mode"), "static") ? cfg_str("net.ip") : "DHCP");
    return true;
#else
    char m1[300] = "", m2[300] = "";
    bool ok = true;
    if (host && cfg_str("net.host")[0]) {
        const char *const a[] = {"sudo", "-n", CNC_NET, "hostname", cfg_str("net.host"), NULL};
        ok = run(a, m1, sizeof m1);
    }
    if (ok && addr) {
        if (!strcmp(cfg_str("net.mode"), "static")) {
            const char *const a[] = {"sudo", "-n", CNC_NET, "static", cfg_str("net.ip"), cfg_str("net.mask"), cfg_str("net.gw"), cfg_str("net.dns"), NULL};
            ok = run(a, m2, sizeof m2);
        } else {
            const char *const a[] = {"sudo", "-n", CNC_NET, "dhcp", NULL};
            ok = run(a, m2, sizeof m2);
        }
    }
    snprintf(msg, n, "%s", m2[0] ? m2 : m1);
    if (!ok && m1[0] && !m2[0]) snprintf(msg, n, "%s", m1);
    return ok;
#endif
}
