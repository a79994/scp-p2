#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_BARBERS 1
#define NUM_CHAIRS 2
#define NUM_CUSTOMERS 15

static barbershop_t g_shop;

static void *slow_barber_func(void *arg) {
    int id = *(int *)arg;
    while (true) {
        int cid = barbershop_barber_wait_for_customer(&g_shop, id);
        if (cid < 0) break;
        /* Slow haircut to guarantee saturation of waiting room */
        usleep(30000);
        barbershop_barber_finish_haircut(&g_shop, id);
    }
    return NULL;
}

static void *fast_arriving_customer(void *arg) {
    int id = *(int *)arg;
    int assigned = -1;
    barbershop_customer_arrive(&g_shop, id, &assigned);
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 3] Customer Balking Under Saturated Capacity ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    if (barbershop_init(&g_shop, NUM_BARBERS, NUM_CHAIRS) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize barbershop!\n" COLOR_RESET);
        return 1;
    }

    pthread_t b_thread;
    int b_id = 0;
    pthread_create(&b_thread, NULL, slow_barber_func, &b_id);

    pthread_t c_threads[NUM_CUSTOMERS];
    int c_ids[NUM_CUSTOMERS];

    /* Flood the shop rapidly */
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        c_ids[i] = i;
        pthread_create(&c_threads[i], NULL, fast_arriving_customer, &c_ids[i]);
        usleep(500); /* Arrive much faster than haircut time */
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(c_threads[i], NULL);
    }

    barbershop_shutdown(&g_shop);
    pthread_join(b_thread, NULL);

    cancel_test_watchdog();

    printf(" Checking results:\n");
    printf("  - Total customers: %d\n", NUM_CUSTOMERS);
    printf("  - Customers served: %d\n", g_shop.total_served);
    printf("  - Customers balked: %d\n", g_shop.total_balked);
    printf("  - Peak waiting room occupancy: %d / %d chairs\n", g_shop.max_observed_waiting, NUM_CHAIRS);

    if (g_shop.total_balked == 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Expected customers to balk under saturation, but balked count is 0!\n" COLOR_RESET);
        barbershop_destroy(&g_shop);
        return 1;
    }

    if (!barbershop_verify_invariants(&g_shop, NUM_CUSTOMERS)) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariant verification failed!\n" COLOR_RESET);
        barbershop_destroy(&g_shop);
        return 1;
    }

    barbershop_destroy(&g_shop);
    printf(COLOR_GREEN "[PASS] Case 3 passed: Balking policy functioned correctly under shop saturation.\n" COLOR_RESET);
    return 0;
}
