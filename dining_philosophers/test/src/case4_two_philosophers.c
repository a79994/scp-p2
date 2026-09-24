#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 4] Minimal Number of Philosophers (N = 2) ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    const int NUM_PHILOSOPHERS = 2;
    const int MEALS_EACH = 15;

    table_t table;
    if (table_init(&table, NUM_PHILOSOPHERS, MEALS_EACH) != 0) {
        fprintf(stderr, COLOR_RED "Failed to initialize table!\n" COLOR_RESET);
        return 1;
    }

    table.on_state_change = mutual_exclusion_validator;
    table.verbose = false;

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        table.philosophers[i].think_time_min_us = 500;
        table.philosophers[i].think_time_max_us = 1000;
        table.philosophers[i].eat_time_min_us = 500;
        table.philosophers[i].eat_time_max_us = 1000;
    }

    if (table_start(&table) != 0) {
        fprintf(stderr, COLOR_RED "Failed to start threads!\n" COLOR_RESET);
        table_destroy(&table);
        return 1;
    }

    table_wait(&table);
    cancel_test_watchdog();

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        printf("  - Philosopher %d completed %lu / %d meals\n",
               i, table.philosophers[i].meals_eaten, MEALS_EACH);
        if (table.philosophers[i].meals_eaten != (uint64_t)MEALS_EACH) {
            fprintf(stderr, COLOR_RED "[FAIL] Philosopher %d failed to complete meals!\n" COLOR_RESET, i);
            table_destroy(&table);
            return 1;
        }
    }

    table_destroy(&table);
    printf(COLOR_GREEN "[PASS] Case 4 passed: Minimal number of philosophers (N=2) verified.\n" COLOR_RESET);
    return 0;
}
