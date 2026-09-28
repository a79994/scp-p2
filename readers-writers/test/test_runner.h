#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "rwlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_CYAN    "\033[1;36m"

static inline void timeout_watchdog_handler(int sig) {
    (void)sig;
    fprintf(stderr, COLOR_RED "\n[WATCHDOG TIMEOUT] Test exceeded deadline! A deadlock has occurred!\n" COLOR_RESET);
    exit(3);
}

static inline void init_test_watchdog(int seconds) {
    signal(SIGALRM, timeout_watchdog_handler);
    alarm(seconds);
}

static inline void cancel_test_watchdog(void) {
    alarm(0);
}

#endif /* TEST_RUNNER_H */
