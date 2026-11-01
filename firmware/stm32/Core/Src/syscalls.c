/*
 * Минимални системни функции за newlib (без операционна система).
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * По-късно _write ще се пренасочи към USB CDC или UART за съобщения
 * при отстраняване на грешки (printf).
 */
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

int _close(int fd)                    { (void)fd; return -1; }
int _lseek(int fd, int p, int dir)    { (void)fd; (void)p; (void)dir; return 0; }
int _read(int fd, char *b, int n)     { (void)fd; (void)b; (void)n; return 0; }
int _write(int fd, char *b, int n)    { (void)fd; (void)b; return n; }
int _isatty(int fd)                   { (void)fd; return 1; }
int _fstat(int fd, struct stat *st)   { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _getpid(void)                     { return 1; }
int _kill(int pid, int sig)           { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status)                { (void)status; for (;;) {} }
