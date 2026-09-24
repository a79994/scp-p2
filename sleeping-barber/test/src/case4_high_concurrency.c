#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_BARBERS 8
#define NUM_CHAIRS 10
#define NUM_CUSTOMERS 100

static barbershop_t g_shop;

static void *stress_barber(void *arg) {
    int id = *(int *)arg;
    while (true) {
        int cid = barbershop_barber_wait_for_customer(&g_shop, id);
        if (cid < 0) break;
        usleep(300);
        barbershop_barber_finish_haircut(&g_shop, id);
    }
    return NULL;
}

static void *stress_customer(void *arg) {
    int id = *(int *)arg;
    int assigned = -1;
    barbershop_customer_arrive(&g_shop, id, &assigned);
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 4] High Concurrency Stress Test ===" COLOR_RESET "\n");
    init_test_watchdog(15);

    if (barbershop_init(&g_shop, NUM_BARBERS, NUM_CHAIRS) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize barbershop!\n" COLOR_RESET);
        return 1;
    }

    pthread_t b_threads[NUM_BARBERS];
    int b_ids[NUM_BARBERS];
    for (int i = 0; i < NUM_BARBERS; i++) {
        b_ids[i] = i;
        pthread_create(&b_threads[i], NULL, stress_barber, &b_ids[i]);
    }

    pthread_t c_threads[NUM_CUSTOMERS];
    int c_ids[NUM_CUSTOMERS];

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        c_ids[i] = i;
        pthread_create(&c_threads[i], NULL, stress_customer, &c_ids[i]);
        if (i % 5 == 0) {
            usleep(200);
        }
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(c_threads[i], NULL);
    }

    barbershop_shutdown(&g_shop);
    for (int i = 0; i < NUM_BARBERS; i++) {
        pthread_join(b_threads[i], NULL);
    }

    cancel_test_watchdog();

    printf(" Checking results:\n");
    printf("  - Total customers: %d\n", NUM_CUSTOMERS);
    printf("  - Customers served: %d\n", g_shop.total_served);
    printf("  - Customers balked: %d\n", g_shop.total_balked);
    printf("  - Peak waiting room occupancy: %d / %d chairs\n", g_shop.max_observed_waiting, NUM_CHAIRS);
    printf("  - Invariant violations: %d\n", g_shop.invariant_violations);

    if (!barbershop_verify_invariants(&g_shop, NUM_CUSTOMERS)) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariant verification failed during high concurrency!\n" COLOR_RESET);
        barbershop_destroy(&g_shop);
        return 1;
    }

    barbershop_destroy(&g_shop);
    printf(COLOR_GREEN "[PASS] Case 4 passed: High-concurrency stress test succeeded with zero violations.\n" COLOR_RESET);
    return 0;
}
