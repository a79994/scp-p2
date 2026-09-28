#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_READERS 6
#define NUM_WRITERS 3
#define OPS_PER_THREAD 30
#define ARRAY_SIZE 8

typedef struct {
    int data[ARRAY_SIZE];
    int write_seq;
} consistency_data_t;

static rwlock_t g_rw;
static consistency_data_t g_shared;
static int g_inconsistency_detected = 0;
static pthread_mutex_t g_check_lock = PTHREAD_MUTEX_INITIALIZER;

static void *writer_thread(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int token = (id + 1) * 1000 + i;

        rwlock_write_lock(&g_rw);

        /* Write incrementally with small delay to test mutual exclusion window */
        for (int k = 0; k < ARRAY_SIZE; k++) {
            g_shared.data[k] = token;
            if (k == ARRAY_SIZE / 2) {
                usleep(200); /* Deliberate window for any ill-behaved thread */
            }
        }
        g_shared.write_seq++;

        rwlock_write_unlock(&g_rw);
        usleep(300);
    }
    return NULL;
}

static void *reader_thread(void *arg) {
    (void)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        rwlock_read_lock(&g_rw);

        /* Verify all elements are identical (atomicity of writes) */
        int expected = g_shared.data[0];
        bool consistent = true;
        for (int k = 1; k < ARRAY_SIZE; k++) {
            if (g_shared.data[k] != expected) {
                consistent = false;
                break;
            }
        }

        rwlock_read_unlock(&g_rw);

        if (!consistent) {
            pthread_mutex_lock(&g_check_lock);
            g_inconsistency_detected++;
            pthread_mutex_unlock(&g_check_lock);
        }

        usleep(200);
    }
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 2] Strict Mutual Exclusion & No Dirty Reads ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    if (rwlock_init(&g_rw) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize rwlock!\n" COLOR_RESET);
        return 1;
    }

    for (int k = 0; k < ARRAY_SIZE; k++) {
        g_shared.data[k] = 0;
    }
    g_shared.write_seq = 0;

    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    int r_ids[NUM_READERS];
    int w_ids[NUM_WRITERS];

    for (int i = 0; i < NUM_WRITERS; i++) {
        w_ids[i] = i;
        pthread_create(&writers[i], NULL, writer_thread, &w_ids[i]);
    }

    for (int i = 0; i < NUM_READERS; i++) {
        r_ids[i] = i;
        pthread_create(&readers[i], NULL, reader_thread, &r_ids[i]);
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    cancel_test_watchdog();

    printf(" Checking results:\n");
    printf("  - Total completed writes: %d\n", g_shared.write_seq);
    printf("  - Inconsistency / Dirty read occurrences: %d\n", g_inconsistency_detected);

    if (g_inconsistency_detected > 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Inconsistency detected! Reader observed partial write.\n" COLOR_RESET);
        rwlock_destroy(&g_rw);
        return 1;
    }

    rwlock_destroy(&g_rw);
    printf(COLOR_GREEN "[PASS] Case 2 passed: Strict mutual exclusion and atomicity verified.\n" COLOR_RESET);
    return 0;
}
