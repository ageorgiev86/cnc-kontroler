/*
 * Конзола по USB CDC.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#include "console.h"
#include "main.h"
#include "usb_cdc.h"
#include "stepgen.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 80
#define ARGS_MAX 10

static char line[LINE_MAX];
static unsigned line_len;
static int was_connected;
static char last_c;

void console_printf(const char *fmt, ...)
{
  char buf[256];
  va_list ap;
  int n;

  if (!usb_cdc_connected()) {
    return;
  }
  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n > 0) {
    usb_cdc_write(buf, ((size_t)n < sizeof(buf)) ? (size_t)n : sizeof(buf) - 1u);
  }
}

/* ---- команди ---- */

static void cmd_help(int argc, char **argv);

static void cmd_info(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  console_printf("SYSCLK %lu MHz, HCLK %lu MHz, време %lu ms\r\n",
                 (unsigned long)(HAL_RCC_GetSysClockFreq() / 1000000u),
                 (unsigned long)(HAL_RCC_GetHCLKFreq() / 1000000u),
                 (unsigned long)HAL_GetTick());
  console_printf("DEVID 0x%03lX REV 0x%04lX, flash %lu KB, UID %08lX%08lX%08lX\r\n",
                 (unsigned long)HAL_GetDEVID(), (unsigned long)HAL_GetREVID(),
                 (unsigned long)(*(const volatile uint16_t *)FLASHSIZE_BASE),
                 (unsigned long)HAL_GetUIDw0(), (unsigned long)HAL_GetUIDw1(),
                 (unsigned long)HAL_GetUIDw2());
  console_printf("USB: изхвърлени %lu байта\r\n", (unsigned long)usb_cdc_dropped());
}

static const char axis_names[STEPGEN_AXES] = { 'X', 'Y', 'Z', 'A', 'B', 'C' };

static void cmd_status(int argc, char **argv)
{
  struct stepgen_status st;

  (void)argc;
  (void)argv;
  stepgen_get_status(&st);
  for (unsigned i = 0; i < STEPGEN_AXES; i++) {
    console_printf("%c %ld  ", axis_names[i], (long)st.position[i]);
  }
  console_printf("\r\nопашка %u/%u, грешка %u, %s\r\n", st.queue_used, st.queue_size, st.fault,
                 st.running ? "движи се" : "стои");
  console_printf("DMA: буфери %lu, най-дълго попълване %lu такта (%lu ns от 250000)\r\n",
                 (unsigned long)st.blocks, (unsigned long)st.fill_cycles_max,
                 (unsigned long)(st.fill_cycles_max * 1000u / (HAL_RCC_GetSysClockFreq() / 1000000u)));
}

/* move x=<стъпки> y=... c=... f=<стъпки/s> [acc=<стъпки/s2>]: права линия по осите */
static void cmd_move(int argc, char **argv)
{
  int32_t steps[STEPGEN_AXES] = {0};
  long rate  = 0;
  long accel = 0;
  int r;

  for (int k = 1; k < argc; k++) {
    char *eq = strchr(argv[k], '=');
    if (eq == NULL) {
      rate = -1;
      break;
    }
    *eq = '\0';
    long v = strtol(eq + 1, NULL, 0);
    if (strcmp(argv[k], "f") == 0) {
      rate = v;
    } else if (strcmp(argv[k], "acc") == 0) {
      accel = v;
    } else if (argv[k][1] == '\0') {
      const char *pos = strchr("xyzabc", argv[k][0]);
      if (pos == NULL || *pos == '\0') {
        rate = -1;
        break;
      }
      steps[pos - "xyzabc"] = (int32_t)v;
    } else {
      rate = -1;
      break;
    }
  }
  if (rate <= 0) {
    console_printf("употреба: move x=<стъпки> [y=.. z=.. a=.. b=.. c=..] f=<стъпки/s> [acc=<стъпки/s2>]\r\n"
                   "  f и acc са за оста с най-голямо преместване; другите следват по права линия\r\n");
    return;
  }
  r = stepgen_test_move(steps, (int32_t)rate, (int32_t)accel);
  switch (r) {
  case 0:  console_printf("ok\r\n"); break;
  case 1:  console_printf("отказ: осите се движат\r\n"); break;
  case 2:  console_printf("отказ: има грешка (reset)\r\n"); break;
  case 4:  console_printf("отказ: скоростта е над възможното за генератора\r\n"); break;
  default: console_printf("отказ: невалидни стойности\r\n"); break;
  }
}

static void cmd_stop(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  stepgen_stop();
  console_printf("спиране\r\n");
}

static void cmd_reset(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  stepgen_clear_fault();
  console_printf("грешките са изчистени\r\n");
}

struct command {
  const char *name;
  void (*fn)(int argc, char **argv);
  const char *help;
};

static const struct command commands[] = {
  { "help",   cmd_help,   "този списък" },
  { "info",   cmd_info,   "такт, чип, USB" },
  { "status", cmd_status, "състояние на генератора на стъпки" },
  { "move",   cmd_move,   "move x=3200 y=-1600 f=8000 [acc=50000]: права линия по осите" },
  { "stop",   cmd_stop,   "спиране по рампа" },
  { "reset",  cmd_reset,  "изчистване на грешка" },
};

static void cmd_help(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    console_printf("  %-7s %s\r\n", commands[i].name, commands[i].help);
  }
}

static void execute(char *s)
{
  char *argv[ARGS_MAX];
  int argc = 0;
  char *save = NULL;

  for (char *t = strtok_r(s, " \t", &save); t != NULL && argc < ARGS_MAX; t = strtok_r(NULL, " \t", &save)) {
    argv[argc++] = t;
  }
  if (argc == 0) {
    return;
  }
  for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (strcmp(argv[0], commands[i].name) == 0) {
      commands[i].fn(argc, argv);
      return;
    }
  }
  console_printf("непозната команда „%s“ (виж help)\r\n", argv[0]);
}

void console_poll(void)
{
  char buf[32];
  size_t n;
  int connected = usb_cdc_connected();

  if (connected && !was_connected) {
    console_printf("\r\nCNC MCU (STM32H723), сборка " __DATE__ " " __TIME__ "\r\nhelp за командите\r\n> ");
    line_len = 0;
  }
  was_connected = connected;

  n = usb_cdc_read(buf, sizeof(buf));
  for (size_t i = 0; i < n; i++) {
    char c = buf[i];
    char prev = last_c;
    last_c = c;
    if (c == '\n' && prev == '\r') {
      continue;                                 /* CR LF се брои за един край на реда */
    }
    if (c == '\r' || c == '\n') {
      console_printf("\r\n");
      line[line_len] = '\0';
      execute(line);
      line_len = 0;
      console_printf("> ");
    } else if (c == '\b' || c == 0x7F) {
      if (line_len > 0) {
        line_len--;
        console_printf("\b \b");
      }
    } else if (c >= ' ' && line_len < LINE_MAX - 1u) {
      line[line_len++] = c;
      usb_cdc_write(&c, 1);                     /* ехо */
    }
  }
}
