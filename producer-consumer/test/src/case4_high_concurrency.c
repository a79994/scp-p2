#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define TOTAL_ITEMS 1000
#define BUFFER_CAPACITY 4
#define NUM_PRODUCERS 10
#define NUM_CONSUMERS 10

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 4] High-Concurrency Stress Test (10 Prods, 10 Cons, 1000 Items) ===" COLOR_RESET "\n");
    init_test_watchdog(15);

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

    /* Expand verify pipe buffer to 1MB */
    fcntl(verify_pipe[1], F_SETPIPE_SZ, 1024 * 1024);

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
                (void)write(verify_pipe[1], &item, sizeof(item));
            }
            close(verify_pipe[1]);
            buffer_destroy(&buffer);
            exit(0);
        }
        cons[i] = pid;
    }

    close(verify_pipe[1]);

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        waitpid(prods[i], NULL, 0);
    }

    buffer_shutdown(&buffer);

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        waitpid(cons[i], NULL, 0);
    }

    cancel_test_watchdog();

    int *g_consumed_counts = calloc(TOTAL_ITEMS, sizeof(int));
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
    printf("  - Total Items Expected: %d\n", TOTAL_ITEMS);
    printf("  - Total Items Consumed: %d\n", g_total_consumed);

    if (g_total_consumed != TOTAL_ITEMS) {
        fprintf(stderr, COLOR_RED "[FAIL] Total items consumed mismatch!\n" COLOR_RESET);
        free(g_consumed_counts);
        buffer_destroy(&buffer);
        return 1;
    }

    for (int i = 0; i < TOTAL_ITEMS; i++) {
        if (g_consumed_counts[i] != 1) {
            fprintf(stderr, COLOR_RED "[FAIL] Item %d consumed %d times!\n" COLOR_RESET, i, g_consumed_counts[i]);
            free(g_consumed_counts);
            buffer_destroy(&buffer);
            return 1;
        }
    }

    free(g_consumed_counts);
    buffer_destroy(&buffer);
    printf(COLOR_GREEN "[PASS] Case 4 passed: High-concurrency MPMC stress test passed with 100%% integrity.\n" COLOR_RESET);
    return 0;
}
