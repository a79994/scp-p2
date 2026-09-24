#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_READERS 4
#define NUM_WRITERS 2
#define OPS_PER_THREAD 25

typedef struct {
    int id;
    rwlock_t *rw;
    shared_resource_t *res;
    int ops_done;
} worker_arg_t;

static void *reader_thread(void *arg) {
    worker_arg_t *w = (worker_arg_t *)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        usleep(500);
        int version = 0;
        shared_resource_read(w->res, w->rw, &version);
        w->ops_done++;
    }
    return NULL;
}

static void *writer_thread(void *arg) {
    worker_arg_t *w = (worker_arg_t *)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        usleep(1000);
        shared_resource_write(w->res, w->rw, (w->id + 1) * 100 + i);
        w->ops_done++;
    }
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 1] Basic Readers-Writers Lifecycle ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    rwlock_t rw;
    if (rwlock_init(&rw) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize rwlock!\n" COLOR_RESET);
        return 1;
    }

    shared_resource_t res;
    shared_resource_init(&res, 0);

    pthread_t readers[NUM_READERS];
    worker_arg_t rargs[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    worker_arg_t wargs[NUM_WRITERS];

    for (int i = 0; i < NUM_READERS; i++) {
        rargs[i].id = i;
        rargs[i].rw = &rw;
        rargs[i].res = &res;
        rargs[i].ops_done = 0;
        pthread_create(&readers[i], NULL, reader_thread, &rargs[i]);
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        wargs[i].id = i;
        wargs[i].rw = &rw;
        wargs[i].res = &res;
        wargs[i].ops_done = 0;
        pthread_create(&writers[i], NULL, writer_thread, &wargs[i]);
    }

    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }

    cancel_test_watchdog();

    int expected_reads = NUM_READERS * OPS_PER_THREAD;
    int expected_writes = NUM_WRITERS * OPS_PER_THREAD;

    printf(" Checking results:\n");
    printf("  - Total reads completed: %d (expected: %d)\n", res.total_reads, expected_reads);
    printf("  - Total writes completed: %d (expected: %d)\n", res.total_writes, expected_writes);
    printf("  - Invariant violations: %d\n", res.invariant_violations);

    if (res.total_reads != expected_reads || res.total_writes != expected_writes) {
        fprintf(stderr, COLOR_RED "[FAIL] Operation count mismatch!\n" COLOR_RESET);
        rwlock_destroy(&rw);
        shared_resource_destroy(&res);
        return 1;
    }

    if (!shared_resource_verify_invariants(&res)) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariant verification failed!\n" COLOR_RESET);
        rwlock_destroy(&rw);
        shared_resource_destroy(&res);
        return 1;
    }

    rwlock_destroy(&rw);
    shared_resource_destroy(&res);

    printf(COLOR_GREEN "[PASS] Case 1 passed: Basic lifecycle and operation counts verified.\n" COLOR_RESET);
    return 0;
}
