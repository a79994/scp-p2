#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 2] Starvation Resistance Under Asymmetric Contention ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    const int NUM_PHILOSOPHERS = 5;
    const int TARGET_MEALS_PHIL_1 = 15;

    table_t table;
    if (table_init(&table, NUM_PHILOSOPHERS, 0) != 0) {
        fprintf(stderr, COLOR_RED "Failed to initialize table!\n" COLOR_RESET);
        return 1;
    }

    table.on_state_change = mutual_exclusion_validator;
    table.verbose = false;

    table.philosophers[0].think_time_min_us = 0;
    table.philosophers[0].think_time_max_us = 10;
    table.philosophers[0].eat_time_min_us = 500;
    table.philosophers[0].eat_time_max_us = 1000;

    table.philosophers[2].think_time_min_us = 0;
    table.philosophers[2].think_time_max_us = 10;
    table.philosophers[2].eat_time_min_us = 500;
    table.philosophers[2].eat_time_max_us = 1000;

    table.philosophers[1].think_time_min_us = 500;
    table.philosophers[1].think_time_max_us = 1000;
    table.philosophers[1].eat_time_min_us = 500;
    table.philosophers[1].eat_time_max_us = 1000;

    for (int i = 3; i < NUM_PHILOSOPHERS; i++) {
        table.philosophers[i].think_time_min_us = 500;
        table.philosophers[i].think_time_max_us = 1000;
        table.philosophers[i].eat_time_min_us = 500;
        table.philosophers[i].eat_time_max_us = 1000;
    }

    printf(" Starting simulation: Phil 0 & 2 are hyper-aggressive, Phil 1 is sandwiched...\n");
    if (table_start(&table) != 0) {
        fprintf(stderr, COLOR_RED "Failed to start threads!\n" COLOR_RESET);
        table_destroy(&table);
        return 1;
    }

    while (1) {
        usleep(5000);
        pthread_mutex_lock(&table.state_lock);
        uint64_t phil1_meals = table.philosophers[1].meals_eaten;
        pthread_mutex_unlock(&table.state_lock);

        if (phil1_meals >= TARGET_MEALS_PHIL_1) {
            break;
        }
    }

    table_stop(&table);
    table_wait(&table);
    cancel_test_watchdog();

    printf(" Meal distribution under aggressive contention:\n");
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        printf("  - Philosopher %d ate %lu meals\n", i, table.philosophers[i].meals_eaten);
    }

    if (table.philosophers[1].meals_eaten < TARGET_MEALS_PHIL_1) {
        fprintf(stderr, COLOR_RED "[FAIL] Philosopher 1 was starved! Only got %lu meals!\n" COLOR_RESET,
                table.philosophers[1].meals_eaten);
        table_destroy(&table);
        return 1;
    }

    table_destroy(&table);
    printf(COLOR_GREEN "[PASS] Case 2 passed: Starvation resistance under asymmetric contention verified.\n" COLOR_RESET);
    return 0;
}
