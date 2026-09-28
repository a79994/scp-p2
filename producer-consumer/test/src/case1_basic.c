#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

#define TOTAL_ITEMS 40
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define BUFFER_CAPACITY 5

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 1] Basic Multi-Producer Multi-Consumer Lifecycle ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    buffer_t buffer;
    if (buffer_init(&buffer, BUFFER_CAPACITY) != 0) {
        fprintf(stderr, COLOR_RED "Failed to initialize buffer!\n" COLOR_RESET);
        return 1;
    }

    int verify_pipe[2];
    if (pipe(verify_pipe) != 0) {
        fprintf(stderr, COLOR_RED "Failed to create verification pipe!\n" COLOR_RESET);
        buffer_destroy(&buffer);
        return 1;
    }

    pid_t prods[NUM_PRODUCERS];
    pid_t cons[NUM_CONSUMERS];

    int items_per_prod = TOTAL_ITEMS / NUM_PRODUCERS;
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork producer failed");
            exit(1);
        }
        if (pid == 0) {
            close(verify_pipe[0]);
            close(verify_pipe[1]);
            buffer_close_producer_unused(&buffer);

            int start = i * items_per_prod;
            for (int k = 0; k < items_per_prod; k++) {
                item_t item;
                item.value = start + k;
                item.producer_id = i;
                usleep(500);
                if (!buffer_push(&buffer, item)) {
                    break;
                }
            }
            buffer_destroy(&buffer);
            exit(0);
        }
        prods[i] = pid;
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork consumer failed");
            exit(1);
        }
        if (pid == 0) {
            close(verify_pipe[0]);
            buffer_close_consumer_unused(&buffer);

            item_t item;
            while (buffer_pop(&buffer, &item)) {
                usleep(500);
                (void)write(verify_pipe[1], &item, sizeof(item));
            }
            close(verify_pipe[1]);
            buffer_destroy(&buffer);
            exit(0);
        }
        cons[i] = pid;
    }

    /* Parent closes write end of verify_pipe */
    close(verify_pipe[1]);

    /* Wait for all producers to finish producing */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        waitpid(prods[i], NULL, 0);
    }

    /* Signal shutdown so consumers exit after draining */
    buffer_shutdown(&buffer);

    /* Wait for all consumers to exit */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        waitpid(cons[i], NULL, 0);
    }

    cancel_test_watchdog();

    int g_consumed_counts[TOTAL_ITEMS] = {0};
    int g_total_consumed = 0;
    item_t item;
    while (read(verify_pipe[0], &item, sizeof(item)) == sizeof(item)) {
        if (item.value >= 0 && item.value < TOTAL_ITEMS) {
            g_consumed_counts[item.value]++;
        }
        g_total_consumed++;
    }
    close(verify_pipe[0]);

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
