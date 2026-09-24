#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_BARBERS 1
#define NUM_CHAIRS 3
#define NUM_CUSTOMERS 10

static barbershop_t g_shop;

static void *barber_func(void *arg) {
    int id = *(int *)arg;
    while (true) {
        int cid = barbershop_barber_wait_for_customer(&g_shop, id);
        if (cid < 0) break;
        usleep(500);
        barbershop_barber_finish_haircut(&g_shop, id);
    }
    return NULL;
}

static void *customer_func(void *arg) {
    int id = *(int *)arg;
    int assigned = -1;
    barbershop_customer_arrive(&g_shop, id, &assigned);
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 1] Basic Single-Barber Lifecycle ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    if (barbershop_init(&g_shop, NUM_BARBERS, NUM_CHAIRS) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize barbershop!\n" COLOR_RESET);
        return 1;
    }

    pthread_t b_thread;
    int b_id = 0;
    pthread_create(&b_thread, NULL, barber_func, &b_id);

    pthread_t c_threads[NUM_CUSTOMERS];
    int c_ids[NUM_CUSTOMERS];

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        c_ids[i] = i;
        pthread_create(&c_threads[i], NULL, customer_func, &c_ids[i]);
        usleep(1500); /* Spaced out so all should be served */
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(c_threads[i], NULL);
    }

    barbershop_shutdown(&g_shop);
    pthread_join(b_thread, NULL);

    cancel_test_watchdog();

    printf(" Checking results:\n");
    printf("  - Total served: %d / %d\n", g_shop.total_served, NUM_CUSTOMERS);
    printf("  - Total balked: %d\n", g_shop.total_balked);
    printf("  - Invariant violations: %d\n", g_shop.invariant_violations);

    if (!barbershop_verify_invariants(&g_shop, NUM_CUSTOMERS)) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariants verification failed!\n" COLOR_RESET);
        barbershop_destroy(&g_shop);
        return 1;
    }

    if (g_shop.total_served != NUM_CUSTOMERS) {
        fprintf(stderr, COLOR_RED "[FAIL] Expected all %d customers to be served!\n" COLOR_RESET, NUM_CUSTOMERS);
        barbershop_destroy(&g_shop);
        return 1;
    }

    barbershop_destroy(&g_shop);
    printf(COLOR_GREEN "[PASS] Case 1 passed: Basic single-barber lifecycle verified.\n" COLOR_RESET);
    return 0;
}
