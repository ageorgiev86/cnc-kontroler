/*
 * Минимални системни функции за newlib (без операционна система).
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * _write праща stdout и stderr (printf) към USB CDC конзолата.
 * За собствения код се предпочита console_printf (без буфер на stdio).
 */
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "usb_cdc.h"

/* newlib ги вика без заглавен файл; декларациите са за -Wmissing-prototypes */
int _close(int fd);
int _lseek(int fd, int p, int dir);
int _read(int fd, char *b, int n);
int _write(int fd, char *b, int n);
int _isatty(int fd);
int _fstat(int fd, struct stat *st);
int _getpid(void);
int _kill(int pid, int sig);
void _exit(int status);

int _close(int fd)                    { (void)fd; return -1; }
int _lseek(int fd, int p, int dir)    { (void)fd; (void)p; (void)dir; return 0; }
int _read(int fd, char *b, int n)     { (void)fd; (void)b; (void)n; return 0; }
int _write(int fd, char *b, int n)
{
  if (fd == 1 || fd == 2) {
    (void)usb_cdc_write(b, (size_t)n);
  }
  return n;                             /* без терминал излишното се изхвърля */
}
int _isatty(int fd)                   { (void)fd; return 1; }
int _fstat(int fd, struct stat *st)   { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _getpid(void)                     { return 1; }
int _kill(int pid, int sig)           { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status)                { (void)status; for (;;) {} }
