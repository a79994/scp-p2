#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_BARBERS 3
#define NUM_CHAIRS 5
#define NUM_CUSTOMERS 30

static barbershop_t g_shop;

static void *barber_func(void *arg) {
    int id = *(int *)arg;
    while (true) {
        int cid = barbershop_barber_wait_for_customer(&g_shop, id);
        if (cid < 0) break;
        usleep(1000); /* Simulate haircut */
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
    printf(COLOR_CYAN "=== [TEST CASE 2] Multi-Barber Parallel Service & Rendezvous ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    if (barbershop_init(&g_shop, NUM_BARBERS, NUM_CHAIRS) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize barbershop!\n" COLOR_RESET);
        return 1;
    }

    pthread_t b_threads[NUM_BARBERS];
    int b_ids[NUM_BARBERS];
    for (int i = 0; i < NUM_BARBERS; i++) {
        b_ids[i] = i;
        pthread_create(&b_threads[i], NULL, barber_func, &b_ids[i]);
    }

    pthread_t c_threads[NUM_CUSTOMERS];
    int c_ids[NUM_CUSTOMERS];

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        c_ids[i] = i;
        pthread_create(&c_threads[i], NULL, customer_func, &c_ids[i]);
        usleep(800);
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
    for (int i = 0; i < NUM_BARBERS; i++) {
        printf("  - Barber %d served: %d customers\n", i, g_shop.barber_served_count[i]);
        if (g_shop.barber_served_count[i] == 0) {
            fprintf(stderr, COLOR_RED "[FAIL] Barber %d was starved / never served any customer!\n" COLOR_RESET, i);
            barbershop_destroy(&g_shop);
            return 1;
        }
    }
    printf("  - Total served: %d | Total balked: %d\n", g_shop.total_served, g_shop.total_balked);

    if (!barbershop_verify_invariants(&g_shop, NUM_CUSTOMERS)) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariant verification failed!\n" COLOR_RESET);
        barbershop_destroy(&g_shop);
        return 1;
    }

    barbershop_destroy(&g_shop);
    printf(COLOR_GREEN "[PASS] Case 2 passed: All %d barbers actively served customers without conflict.\n" COLOR_RESET, NUM_BARBERS);
    return 0;
}
