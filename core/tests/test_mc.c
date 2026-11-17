/* Тест на управлението (mc.c) с виртуалния STM32 (vmcu.c): цикъл по цикъл, време 1 ms на цикъл. */
#include "cnc/mc.h"
#include "cnc/vmcu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails, total;
#define CHECK(c, ...) do { total++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static vmcu_t *V;
static mc_t *M;
static mc_snap_t S;
static int max_queue;

static void cyc(int n)
{
    for (int i = 0; i < n; i++) {
        vmcu_advance(V, 1000);
        mc_cycle(M);
        mc_snapshot(M, &S);
        if (S.state == MC_RUN && S.queue > max_queue) max_queue = S.queue;
    }
}

/* до състояние st (или до лимит от цикли) */
static bool until(mc_state_t st, int max)
{
    for (int i = 0; i < max; i++) {
        cyc(1);
        if (S.state == st) return true;
    }
    return false;
}

static bool settle(int max) { cyc(5); return until(MC_IDLE, max); }

static double vpos(int a) { return (double)vmcu_pos_q(V, a) / 65536.0 / 400.0; }   /* стъпки → мм отн. на включването */

static int run_all(bool lag);

int main(void)
{
    int r = run_all(false);
    printf("--- същото със закъснение на отговора с един пакет (като по SPI) ---\n");
    r |= run_all(true);
    printf("%d проверки, %d неуспешни\n", total, fails);
    return r || fails ? 1 : 0;
}

static int run_all(bool lag)
{
    mc_cfg_t c;
    memset(&c, 0, sizeof c);
    for (int a = 0; a < 3; a++) {
        mc_axis_t *x = &c.ax[a];
        x->active = true;
        x->spm = 400;
        x->vmax = 100; x->amax = 1000; x->jmax = 50000;
        x->smin = -300; x->smax = 0;
        x->home_sw = true; x->home_dir = 1; x->home_seq = a == 2 ? 0 : 1;
        x->home_search = 20; x->home_latch = 2; x->home_pull = 2;
        x->home_sw_pos = 0; x->home_after = -5;
    }
    for (int a = 3; a < 6; a++) { c.ax[a].spm = 100; c.ax[a].vmax = 100; c.ax[a].amax = 1000; }
    c.tol = 0.02; c.arc_tol = 0.002; c.sp_delay = 0.05;
    c.out_flood = 1; c.out_mist = -1; c.out_air = 0;

    V = vmcu_new();
    vmcu_manual_time(V, true);
    vmcu_spi_lag(V, lag);
    M = mc_new(vmcu_port(V), &c);

    /* 1. връзка → чака RESET → готова */
    cyc(20);
    CHECK(S.link && S.state == MC_FAULT, "след свързване: link %d, състояние %d", S.link, S.state);
    mc_reset(M);
    CHECK(settle(200), "RESET: състояние %d (%s)", S.state, S.alarm);

    /* 2. реферирането: Z първо, после X и Y; датчиците на 30 мм от включването */
    for (int a = 0; a < 3; a++) vmcu_set_home_switch(V, a, (int32_t)(vmcu_pos_q(V, a) / 65536 + 30 * 400));
    mc_home(M, 7);
    CHECK(settle(60000), "реферирането не свърши: състояние %d (%s)", S.state, S.alarm);
    CHECK(S.homed[0] && S.homed[1] && S.homed[2], "не са реферирани: %d %d %d", S.homed[0], S.homed[1], S.homed[2]);
    for (int a = 0; a < 3; a++) CHECK(fabs(S.mach[a] + 5) < 0.003, "след Home ос %d е на %.4f вместо -5", a, S.mach[a]);

    /* 3. ръчно с отместване */
    mc_jog(M, 0, -1, 10, 3000);
    CHECK(settle(5000), "jog: състояние %d", S.state);
    CHECK(fabs(S.mach[0] + 15) < 0.003, "jog: X %.4f вместо -15", S.mach[0]);

    /* 4. програма: линии, дъга, шпиндел, охлаждане */
    gc_config_t g = {GC_DIALECT_MACH3, 7, 0, false, NULL, NULL};
    const char *prog = "G21 G90 G53 G0 Z-10\nM3 S1000\nM8\nG53 G1 X-50 Y-20 F3000\nG53 G1 X-60\nG2 X-80 Y-20 R10\nG53 G1 Z-12\nM9\nM5\nM30\n";
    max_queue = 0;
    mc_run(M, prog, &g, NULL, NULL);
    cyc(10);
    CHECK(S.state == MC_RUN, "програмата не тръгна: %d (%s %s)", S.state, S.alarm, S.msg);
    bool saw_sp = false, saw_cool = false;
    for (int i = 0; i < 20000 && S.state != MC_IDLE; i++) {
        cyc(1);
        if (vmcu_outputs(V) >> 30 & 1) saw_sp = true;
        if (vmcu_outputs(V) >> 1 & 1) saw_cool = true;
    }
    CHECK(S.state == MC_IDLE && S.prog_done, "програмата не свърши: %d (%s)", S.state, S.alarm);
    CHECK(saw_sp && saw_cool, "шпиндел %d, охлаждане %d", saw_sp, saw_cool);
    CHECK(!(vmcu_outputs(V) >> 30 & 1) && !(vmcu_outputs(V) >> 1 & 1), "след M9 M5 изходите са %08x", vmcu_outputs(V));
    CHECK(fabs(S.mach[0] + 80) < 0.003 && fabs(S.mach[1] + 20) < 0.003 && fabs(S.mach[2] + 12) < 0.003,
          "краят: %.4f %.4f %.4f", S.mach[0], S.mach[1], S.mach[2]);
    CHECK(vmcu_fault(V) == 0, "грешка в STM32: %d", vmcu_fault(V));
    CHECK(max_queue >= 40 && max_queue <= 64, "буферът напред: %d сегмента", max_queue);
    printf("програма: буфер до %d ms напред\n", max_queue);

    /* 5. непрекъснато ръчно, пуснато след 300 ms */
    double x0 = S.mach[0];
    mc_jog(M, 0, 1, 0, 1200);
    cyc(300);
    CHECK(S.state == MC_MOVE, "непрекъснато: %d", S.state);
    mc_jog_stop(M);
    CHECK(settle(3000), "непрекъснато не спря: %d", S.state);
    CHECK(S.mach[0] > x0 + 3 && S.mach[0] < x0 + 8, "непрекъснато: изминато %.3f мм", S.mach[0] - x0);

    /* 6. авариен стоп от входа */
    vmcu_set_inputs(V, LK_IN_ESTOP, 0);
    cyc(5);
    CHECK(S.state == MC_ESTOP, "авариен стоп: %d", S.state);
    vmcu_set_inputs(V, 0, LK_IN_ESTOP);
    mc_reset(M);
    CHECK(settle(200), "след аварийния стоп: %d (%s)", S.state, S.alarm);
    CHECK(S.homed[0], "след аварийния стоп позицията е валидна");

    /* 7. мек лимит */
    mc_run(M, "G53 G1 X10 F3000\nM30\n", &g, NULL, NULL);
    CHECK(until(MC_FAULT, 5000), "мекият лимит не спря: %d", S.state);
    CHECK(strstr(S.alarm, "Мек лимит") != NULL, "текст: %s", S.alarm);
    CHECK(S.mach[0] <= 0.001, "мекият лимит: X %.4f", S.mach[0]);
    mc_reset(M);
    CHECK(settle(500), "след мекия лимит: %d", S.state);

    /* 8. Стоп насред програмата */
    mc_run(M, "G53 G1 X-250 F3000\nM30\n", &g, NULL, NULL);
    cyc(400);
    double xs = S.mach[0];
    mc_stop(M);
    CHECK(settle(2000), "Стоп: %d", S.state);
    CHECK(fabs(S.mach[0] - xs) < 2, "Стоп: спря на %.3f (беше %.3f)", S.mach[0], xs);
    double xh = S.mach[0];
    mc_jog(M, 0, 1, 1, 600);
    CHECK(settle(3000), "след Стоп ръчно: %d", S.state);
    CHECK(fabs(S.mach[0] - xh - 1) < 0.003, "след Стоп ръчно: %.4f вместо %.4f", S.mach[0], xh + 1);

    /* 9. пауза и продължаване */
    mc_run(M, "G53 G1 X-100 F3000\nM30\n", &g, NULL, NULL);
    cyc(200);
    mc_hold(M, true);
    cyc(300);
    double xp = S.mach[0];
    cyc(200);
    CHECK(S.state == MC_HOLD && fabs(S.mach[0] - xp) < 1e-6, "пауза: %d, %.4f → %.4f", S.state, xp, S.mach[0]);
    mc_continue(M);
    CHECK(settle(5000) && fabs(S.mach[0] + 100) < 0.003, "след паузата: %d, X %.4f", S.state, S.mach[0]);

    /* 10. M0 */
    mc_run(M, "G53 G1 X-90 F3000\nM0\nG53 G1 X-80\nM30\n", &g, NULL, NULL);
    CHECK(until(MC_WAIT, 5000) && S.wait_kind == MC_WAIT_PAUSE, "M0: %d", S.state);
    CHECK(fabs(S.mach[0] + 90) < 0.003, "M0 спря на %.4f", S.mach[0]);
    mc_continue(M);
    CHECK(settle(5000) && fabs(S.mach[0] + 80) < 0.003, "след M0: X %.4f", S.mach[0]);

    /* 11. грешка в програмата */
    mc_run(M, "G53 G1 X-85 F3000\nG1 X-70 Y\nM30\n", &g, NULL, NULL);
    CHECK(until(MC_FAULT, 5000), "грешка в програмата: %d", S.state);
    printf("грешка: %s\n", S.alarm);
    CHECK(strstr(S.alarm, "Ред 2") != NULL, "грешката: %s", S.alarm);

    CHECK(vmcu_fault(V) == 0 || vmcu_fault(V) == LK_FAULT_ESTOP, "STM32: грешка %d", vmcu_fault(V));

    /* 12. прекъсната връзка: в покой без грешка, в движение грешка */
    mc_reset(M);
    CHECK(settle(500), "преди проверката на връзката: %d", S.state);
    vmcu_advance(V, 100000);   /* 100 ms без пакети, машината стои */
    CHECK(vmcu_fault(V) == 0, "в покой прекъснатата връзка дава грешка %d", vmcu_fault(V));
    cyc(20);
    CHECK(S.state == MC_IDLE, "след тишина в покой: %d (%s)", S.state, S.alarm);
    mc_jog(M, 0, 1, 20, 1200);
    cyc(100);
    vmcu_advance(V, 100000);   /* 100 ms без пакети в движение */
    CHECK(vmcu_fault(V) == LK_FAULT_LINK, "в движение: грешка %d вместо LINK", vmcu_fault(V));
    cyc(5);
    CHECK(S.state == MC_FAULT, "в движение: състояние %d", S.state);
    (void)vpos;
    mc_free(M);
    vmcu_free(V);
    return 0;
}
