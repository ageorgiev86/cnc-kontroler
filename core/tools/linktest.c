/* cnc-linktest: проверка на връзката Linux ↔ STM32 по SPI, без движение.
 *
 *   cnc-linktest [--dev /dev/spidev0.0] [--hz 1000000] [--rate 100] [--sec 0] [--dump] [--vmcu]
 *
 * Праща само празни пакети (heartbeat: без записи и без флагове), --rate на секунда, при такт --hz.
 * Всяка секунда показва колко отговора са валидни, грешките, състоянието на STM32 и входовете му.
 * --dump показва байтовете на всеки невалиден отговор (до 5 пъти). --sec спира след толкова секунди.
 * --vmcu: срещу виртуалния STM32 (проба на самата програма без платка).
 * Спрете интерфейса преди това: sudo systemctl stop cnc-ui (иначе двете програми пращат по SPI). */
#define _GNU_SOURCE
#include "cnc/link.h"
#include "cnc/vmcu.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t quit;
static void on_sig(int s) { (void)s; quit = 1; }

static const char *st_name(int s)
{
    static const char *const N[] = {"BOOT", "IDLE", "READY", "RUN", "HOLD", "STOPPING", "FAULT", "ESTOP"};
    return s >= 0 && s <= 7 ? N[s] : "?";
}

static void dump(const char *what, const uint8_t *p)
{
    printf("  %s:", what);
    for (int i = 0; i < 32; i++) printf(" %02X", p[i]);
    printf(" …\n");
}

int main(int argc, char **argv)
{
    const char *dev = "/dev/spidev0.0";
    unsigned hz = 1000000, rate = 100, sec = 0;
    bool dumpf = false, use_vmcu = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dev") && i + 1 < argc) dev = argv[++i];
        else if (!strcmp(argv[i], "--hz") && i + 1 < argc) hz = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rate") && i + 1 < argc) rate = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sec") && i + 1 < argc) sec = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump")) dumpf = true;
        else if (!strcmp(argv[i], "--vmcu")) use_vmcu = true;
        else {
            fprintf(stderr, "употреба: cnc-linktest [--dev /dev/spidev0.0] [--hz 1000000] [--rate 100] [--sec 0] [--dump] [--vmcu]\n");
            return 2;
        }
    }
    if (rate < 1) rate = 1;
    if (rate > 1000) rate = 1000;
    char err[200];
    vmcu_t *vm = NULL;
    lk_port_t *port;
    if (use_vmcu) {
        vm = vmcu_new();
        vmcu_spi_lag(vm, true);
        vmcu_manual_time(vm, true);
        port = vmcu_port(vm);
    } else {
        port = lk_spi_open(dev, hz, err, sizeof err);
        if (!port) {
            fprintf(stderr, "%s\n", err);
            fprintf(stderr, "Проверете: ls -l /dev/spidev0.0 (след tools/orangepi-setup.sh и рестарт; потребителят в група spi)\n");
            return 1;
        }
    }
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    printf("%s, %u пакета/s, само празни пакети (без движение). Ctrl+C спира.\n", port->name, rate);

    uint16_t seq = 0, ack = 0;
    unsigned long n = 0, good = 0, bad = 0, zeros = 0, ffs = 0, dumps = 0;
    unsigned long s_n = 0, s_good = 0, s_bad = 0;
    uint32_t last_rx_err = 0;
    int lag = -1;
    lk_rx_t last;
    memset(&last, 0, sizeof last);
    bool have = false;
    struct timespec next, t0, tsec;
    clock_gettime(CLOCK_MONOTONIC, &next);
    t0 = tsec = next;
    long period_ns = 1000000000L / (long)rate;
    while (!quit) {
        lk_tx_t tx;
        lk_begin(&tx);
        lk_finish(&tx, 0, ++seq, ack);
        uint8_t rb[LK_PKT];
        lk_rx_t r;
        if (vm) vmcu_advance(vm, (uint32_t)(period_ns / 1000));
        int x = port->xfer(port, tx.b, rb);
        n++; s_n++;
        if (x == 0 && lk_parse(rb, &r) == 0) {
            good++; s_good++;
            ack = r.seq;
            lag = (int)(uint16_t)(seq - r.ack);   /* по SPI отговорът е на предишния пакет: 1 */
            last = r;
            have = true;
        } else if (n == 1) {
            n--; s_n--;   /* първият отговор е отпреди първия пакет (по SPI закъснява с един): не се брои */
        } else {
            bad++; s_bad++;
            bool z = true, f = true;
            for (int i = 0; i < LK_PKT; i++) { z &= rb[i] == 0; f &= rb[i] == 0xFF; }
            zeros += z;
            ffs += f;
            if (dumpf && dumps < 5) { dumps++; dump(x ? "грешка при обмена" : "невалиден отговор", rb); }
        }
        clock_gettime(CLOCK_MONOTONIC, &tsec);
        double el = (tsec.tv_sec - t0.tv_sec) + (tsec.tv_nsec - t0.tv_nsec) / 1e9;
        if (el >= 1.0) {
            printf("%6lu/s  валидни %lu, грешни %lu", s_n, s_good, s_bad);
            if (have) {
                printf("  │ STM32 %s", st_name(last.state));
                if (last.fault) printf(" (%s)", lk_fault_text(last.fault));
                printf(", отговор на пакет −%d, отхвърлени в STM32 %u (+%u), време %.1f s",
                       lag, (unsigned)last.rx_errors, (unsigned)(last.rx_errors - last_rx_err), last.time_us / 1e6);
                printf(", входове 0x%06X", (unsigned)(last.local_in & 0xFFFFFF));
                last_rx_err = last.rx_errors;
            }
            printf("\n");
            if (s_good == 0 && s_bad) {
                if (zeros == s_bad) printf("  само нули: MISO не е свързан, STM32 не е захранен или няма общ GND\n");
                else if (ffs == s_bad) printf("  само FF: MISO виси (не е свързан) или STM32 не е избран (CE0 → PE11)\n");
                else printf("  разбъркани байтове: проверете SCLK/MOSI, общия GND, по-нисък такт (--hz 500000) и --dump\n");
            }
            s_n = s_good = s_bad = 0;
            zeros = ffs = 0;
            t0 = tsec;
            if (sec && --sec == 0) break;
        }
        next.tv_nsec += period_ns;
        while (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    printf("общо %lu, валидни %lu, грешни %lu\n", n, good, bad);
    port->close(port);
    if (vm) vmcu_free(vm);
    return good && !bad ? 0 : 1;
}
