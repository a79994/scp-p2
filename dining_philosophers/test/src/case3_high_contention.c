#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 3] High-Contention Scalability (20 Philosophers) ===" COLOR_RESET "\n");
    init_test_watchdog(15);

    const int NUM_PHILOSOPHERS = 20;
    const int MEALS_EACH = 10;

    table_t table;
    if (table_init(&table, NUM_PHILOSOPHERS, MEALS_EACH) != 0) {
        fprintf(stderr, COLOR_RED "Failed to initialize table!\n" COLOR_RESET);
        return 1;
    }

    table.on_state_change = mutual_exclusion_validator;
    table.verbose = false;
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        table.philosophers[i].think_time_min_us = 200;
        table.philosophers[i].think_time_max_us = 600;
        table.philosophers[i].eat_time_min_us = 200;
        table.philosophers[i].eat_time_max_us = 600;
    }

    if (table_start(&table) != 0) {
        fprintf(stderr, COLOR_RED "Failed to start threads!\n" COLOR_RESET);
        table_destroy(&table);
        return 1;
    }

    table_wait(&table);
    cancel_test_watchdog();

    uint64_t total_meals = 0;
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        total_meals += table.philosophers[i].meals_eaten;
        if (table.philosophers[i].meals_eaten != (uint64_t)MEALS_EACH) {
            fprintf(stderr, COLOR_RED "[FAIL] Philosopher %d only ate %lu meals!\n" COLOR_RESET,
                    i, table.philosophers[i].meals_eaten);
            table_destroy(&table);
            return 1;
        }
    }

    printf(" All %d philosophers completed %d meals (Total meals served: %lu)\n",
           NUM_PHILOSOPHERS, MEALS_EACH, total_meals);

    table_destroy(&table);
    printf(COLOR_GREEN "[PASS] Case 3 passed: High-contention scalability tested.\n" COLOR_RESET);
    return 0;
}
