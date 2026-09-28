#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

#define TOTAL_ITEMS 30
#define BUFFER_CAPACITY 3
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define PRODUCER_SLEEP_TIME_MS 3

typedef struct {
    int max_observed_count;
    bool overflow_detected;
} prod_stat_t;

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 2] Fast Producers, Slow Consumers (Buffer Saturation) ===" COLOR_RESET "\n");
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

    int prod_report_pipe[2];
    if (pipe(prod_report_pipe) != 0) {
        fprintf(stderr, COLOR_RED "Failed to create producer report pipe!\n" COLOR_RESET);
        close(verify_pipe[0]);
        close(verify_pipe[1]);
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
            close(prod_report_pipe[0]);
            buffer_close_producer_unused(&buffer);

            prod_stat_t stat = { .max_observed_count = 0, .overflow_detected = false };
            int start = i * items_per_prod;

            for (int k = 0; k < items_per_prod; k++) {
                item_t item;
                item.value = start + k;
                item.producer_id = i;

                if (!buffer_push(&buffer, item)) {
                    break;
                }

                int cnt = buffer_get_count(&buffer);
                if (cnt > stat.max_observed_count) {
                    stat.max_observed_count = cnt;
                }
                if (cnt > BUFFER_CAPACITY) {
                    stat.overflow_detected = true;
                }
            }

            (void)write(prod_report_pipe[1], &stat, sizeof(stat));
            close(prod_report_pipe[1]);
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
            close(prod_report_pipe[0]);
            close(prod_report_pipe[1]);
            buffer_close_consumer_unused(&buffer);

            item_t item;
            while (buffer_pop(&buffer, &item)) {
                usleep(PRODUCER_SLEEP_TIME_MS * 1000);
                (void)write(verify_pipe[1], &item, sizeof(item));
            }
            close(verify_pipe[1]);
            buffer_destroy(&buffer);
            exit(0);
        }
        cons[i] = pid;
    }

    close(verify_pipe[1]);
    close(prod_report_pipe[1]);

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        waitpid(prods[i], NULL, 0);
    }

    buffer_shutdown(&buffer);

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        waitpid(cons[i], NULL, 0);
    }

    cancel_test_watchdog();

    int g_max_observed_count = 0;
    bool g_overflow_detected = false;
    prod_stat_t st;
    while (read(prod_report_pipe[0], &st, sizeof(st)) == sizeof(st)) {
        if (st.max_observed_count > g_max_observed_count) {
            g_max_observed_count = st.max_observed_count;
        }
        if (st.overflow_detected) {
            g_overflow_detected = true;
        }
    }
    close(prod_report_pipe[0]);

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
    printf("  - Buffer Capacity: %d\n", BUFFER_CAPACITY);
    printf("  - Max Observed Buffer Count: %d\n", g_max_observed_count);
    printf("  - Total Consumed: %d / %d\n", g_total_consumed, TOTAL_ITEMS);

    if (g_overflow_detected || g_max_observed_count > BUFFER_CAPACITY) {
        fprintf(stderr, COLOR_RED "[FAIL] Buffer overflow detected! Count exceeded capacity!\n" COLOR_RESET);
        buffer_destroy(&buffer);
        return 1;
    }

    if (g_total_consumed != TOTAL_ITEMS) {
        fprintf(stderr, COLOR_RED "[FAIL] Not all items were consumed!\n" COLOR_RESET);
        buffer_destroy(&buffer);
        return 1;
    }

    for (int i = 0; i < TOTAL_ITEMS; i++) {
        if (g_consumed_counts[i] != 1) {
            fprintf(stderr, COLOR_RED "[FAIL] Item %d consumed %d times!\n" COLOR_RESET, i, g_consumed_counts[i]);
            buffer_destroy(&buffer);
            return 1;
        }
    }

    buffer_destroy(&buffer);
    printf(COLOR_GREEN "[PASS] Case 2 passed: Buffer saturation handled safely without overflow.\n" COLOR_RESET);
    return 0;
}
