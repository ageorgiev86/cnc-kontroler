/*
 * Конзола по USB CDC: съобщения и прости команди за отстраняване на грешки.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Отваря се с терминал (PuTTY, Tera Term, VS Code Serial Monitor) на COM
 * порта на платката; скоростта няма значение. Командата „help“ дава списъка.
 * Само за разработката: машината не зависи от конзолата.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

/* Вика се често от главния цикъл: приема символи и изпълнява командите. */
void console_poll(void);

/* printf към конзолата (до 256 знака наведнъж); без терминал не прави нищо. */
void console_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* CONSOLE_H */
