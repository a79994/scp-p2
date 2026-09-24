#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define TOTAL_ITEMS 40
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define BUFFER_CAPACITY 5

static int g_consumed_counts[TOTAL_ITEMS] = {0};
static pthread_mutex_t g_check_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_total_consumed = 0;

typedef struct {
    int id;
    buffer_t *buffer;
    int start_item;
    int num_items;
} prod_info_t;

typedef struct {
    int id;
    buffer_t *buffer;
} cons_info_t;

static void *producer_func(void *arg) {
    prod_info_t *info = (prod_info_t *)arg;
    for (int i = 0; i < info->num_items; i++) {
        item_t item;
        item.value = info->start_item + i;
        item.producer_id = info->id;
        usleep(500);
        if (!buffer_push(info->buffer, item)) {
            break;
        }
    }
    return NULL;
}

static void *consumer_func(void *arg) {
    cons_info_t *info = (cons_info_t *)arg;
    item_t item;
    while (buffer_pop(info->buffer, &item)) {
        usleep(500);
        pthread_mutex_lock(&g_check_lock);
        if (item.value >= 0 && item.value < TOTAL_ITEMS) {
            g_consumed_counts[item.value]++;
        }
        g_total_consumed++;
        pthread_mutex_unlock(&g_check_lock);
    }
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 1] Basic Multi-Producer Multi-Consumer Lifecycle ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    buffer_t buffer;
    if (buffer_init(&buffer, BUFFER_CAPACITY) != 0) {
        fprintf(stderr, COLOR_RED "Failed to initialize buffer!\n" COLOR_RESET);
        return 1;
    }

    pthread_t prods[NUM_PRODUCERS];
    prod_info_t pinfo[NUM_PRODUCERS];
    pthread_t cons[NUM_CONSUMERS];
    cons_info_t cinfo[NUM_CONSUMERS];

    int items_per_prod = TOTAL_ITEMS / NUM_PRODUCERS;
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pinfo[i].id = i;
        pinfo[i].buffer = &buffer;
        pinfo[i].start_item = i * items_per_prod;
        pinfo[i].num_items = items_per_prod;
        pthread_create(&prods[i], NULL, producer_func, &pinfo[i]);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        cinfo[i].id = i;
        cinfo[i].buffer = &buffer;
        pthread_create(&cons[i], NULL, consumer_func, &cinfo[i]);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(prods[i], NULL);
    }

    buffer_shutdown(&buffer);

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(cons[i], NULL);
    }

    cancel_test_watchdog();

    printf(" Checking results:\n");
    printf("  - Total items expected: %d\n", TOTAL_ITEMS);
    printf("  - Total items consumed: %d\n", g_total_consumed);

    if (g_total_consumed != TOTAL_ITEMS) {
        fprintf(stderr, COLOR_RED "[FAIL] Item count mismatch!\n" COLOR_RESET);
        buffer_destroy(&buffer);
        return 1;
    }

    for (int i = 0; i < TOTAL_ITEMS; i++) {
        if (g_consumed_counts[i] != 1) {
            fprintf(stderr, COLOR_RED "[FAIL] Item %d consumed %d times (expected 1)!\n" COLOR_RESET,
                    i, g_consumed_counts[i]);
            buffer_destroy(&buffer);
            return 1;
        }
    }

    buffer_destroy(&buffer);
    printf(COLOR_GREEN "[PASS] Case 1 passed: Basic MPMC lifecycle and data integrity verified.\n" COLOR_RESET);
    return 0;
}
