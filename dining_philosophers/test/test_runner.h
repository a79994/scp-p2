#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "philosopher.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_BOLD    "\033[1m"

static volatile bool g_mutual_exclusion_violated = false;
static volatile int g_violator_1 = -1;
static volatile int g_violator_2 = -1;

static inline void mutual_exclusion_validator(table_t *table, int phil_id, philosopher_state_t old_state, philosopher_state_t new_state) {
    (void)old_state;
    if (new_state == STATE_EATING) {
        int n = table->num_philosophers;
        int left = (phil_id - 1 + n) % n;
        int right = (phil_id + 1) % n;

        if (table->philosophers[left].state == STATE_EATING) {
            g_mutual_exclusion_violated = true;
            g_violator_1 = phil_id;
            g_violator_2 = left;
            fprintf(stderr, COLOR_RED "\n[INVARIANT VIOLATION] Mutual exclusion failed! Adjacent philosophers %d and %d are EATING simultaneously!\n" COLOR_RESET, phil_id, left);
            exit(2);
        }
        if (table->philosophers[right].state == STATE_EATING) {
            g_mutual_exclusion_violated = true;
            g_violator_1 = phil_id;
            g_violator_2 = right;
            fprintf(stderr, COLOR_RED "\n[INVARIANT VIOLATION] Mutual exclusion failed! Adjacent philosophers %d and %d are EATING simultaneously!\n" COLOR_RESET, phil_id, right);
            exit(2);
        }
    }
}

static inline void timeout_watchdog_handler(int sig) {
    (void)sig;
    fprintf(stderr, COLOR_RED "\n[WATCHDOG TIMEOUT] Test exceeded deadline! A deadlock has likely occurred!\n" COLOR_RESET);
    exit(3);
}

static inline void init_test_watchdog(int seconds) {
    signal(SIGALRM, timeout_watchdog_handler);
    alarm(seconds);
}

static inline void cancel_test_watchdog(void) {
    alarm(0);
}

#endif
