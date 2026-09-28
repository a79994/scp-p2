#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_READERS 50
#define NUM_WRITERS 10
#define READS_PER_THREAD 20
#define WRITES_PER_THREAD 10

typedef struct {
    int id;
    rwlock_t *rw;
    shared_resource_t *res;
} thread_ctx_t;

static void *reader_thread(void *arg) {
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    for (int i = 0; i < READS_PER_THREAD; i++) {
        usleep(100);
        int version = 0;
        shared_resource_read(ctx->res, ctx->rw, &version);
    }
    return NULL;
}

static void *writer_thread(void *arg) {
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    for (int i = 0; i < WRITES_PER_THREAD; i++) {
        usleep(300);
        shared_resource_write(ctx->res, ctx->rw, (ctx->id + 1) * 1000 + i);
    }
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 4] High Concurrency Stress Test ===" COLOR_RESET "\n");
    init_test_watchdog(15);

    rwlock_t rw;
    if (rwlock_init(&rw) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize rwlock!\n" COLOR_RESET);
        return 1;
    }

    shared_resource_t res;
    shared_resource_init(&res, 0);

    pthread_t readers[NUM_READERS];
    thread_ctx_t r_ctx[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    thread_ctx_t w_ctx[NUM_WRITERS];

    for (int i = 0; i < NUM_READERS; i++) {
        r_ctx[i].id = i;
        r_ctx[i].rw = &rw;
        r_ctx[i].res = &res;
        pthread_create(&readers[i], NULL, reader_thread, &r_ctx[i]);
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        w_ctx[i].id = i;
        w_ctx[i].rw = &rw;
        w_ctx[i].res = &res;
        pthread_create(&writers[i], NULL, writer_thread, &w_ctx[i]);
    }

    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }

    cancel_test_watchdog();

    int expected_reads = NUM_READERS * READS_PER_THREAD;
    int expected_writes = NUM_WRITERS * WRITES_PER_THREAD;

    printf(" Checking results:\n");
    printf("  - Active threads: %d readers, %d writers\n", NUM_READERS, NUM_WRITERS);
    printf("  - Total reads performed: %d (expected: %d)\n", res.total_reads, expected_reads);
    printf("  - Total writes performed: %d (expected: %d)\n", res.total_writes, expected_writes);
    printf("  - Peak concurrent readers observed: %d\n", res.max_concurrent_readers);
    printf("  - Invariant violations: %d\n", res.invariant_violations);

    if (res.total_reads != expected_reads || res.total_writes != expected_writes) {
        fprintf(stderr, COLOR_RED "[FAIL] Operation count mismatch!\n" COLOR_RESET);
        rwlock_destroy(&rw);
        shared_resource_destroy(&res);
        return 1;
    }

    if (!shared_resource_verify_invariants(&res)) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariant violations occurred during stress test!\n" COLOR_RESET);
        rwlock_destroy(&rw);
        shared_resource_destroy(&res);
        return 1;
    }

    if (res.max_concurrent_readers <= 1) {
        fprintf(stderr, COLOR_RED "[WARN] Readers did not achieve parallel concurrency!\n" COLOR_RESET);
    }

    rwlock_destroy(&rw);
    shared_resource_destroy(&res);

    printf(COLOR_GREEN "[PASS] Case 4 passed: High-concurrency stress test succeeded with zero violations.\n" COLOR_RESET);
    return 0;
}
