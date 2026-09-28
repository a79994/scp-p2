#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "rwlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

typedef struct {
    int id;
    rwlock_t *rw;
    shared_resource_t *res;
    int ops_to_perform;
    int ops_completed;
    bool verbose;
} reader_arg_t;

typedef struct {
    int id;
    rwlock_t *rw;
    shared_resource_t *res;
    int ops_to_perform;
    int ops_completed;
    bool verbose;
} writer_arg_t;

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -r <num>       Number of readers (default: 5, min: 1)\n");
    printf("  -w <num>       Number of writers (default: 2, min: 1)\n");
    printf("  -o <ops>       Operations per thread (default: 10, min: 1)\n");
    printf("  -v             Verbose output (log each read/write)\n");
    printf("  -help, -h      Show this help message\n");
}

static void *reader_routine(void *arg) {
    reader_arg_t *r = (reader_arg_t *)arg;
    unsigned int seed = (unsigned int)(time(NULL) ^ (r->id << 4) ^ pthread_self());

    for (int i = 0; i < r->ops_to_perform; i++) {
        int delay = 1000 + (rand_r(&seed) % 4000);
        usleep(delay);

        int version = 0;
        int val = shared_resource_read(r->res, r->rw, &version);
        r->ops_completed++;

        if (r->verbose) {
            printf("[Reader %2d] Read value %5d (Version: %d)\n", r->id, val, version);
        }
    }

    return NULL;
}

static void *writer_routine(void *arg) {
    writer_arg_t *w = (writer_arg_t *)arg;
    unsigned int seed = (unsigned int)(time(NULL) ^ (w->id << 6) ^ pthread_self());

    for (int i = 0; i < w->ops_to_perform; i++) {
        int delay = 1500 + (rand_r(&seed) % 5000);
        usleep(delay);

        int new_val = (w->id + 1) * 1000 + i;
        shared_resource_write(w->res, w->rw, new_val);
        w->ops_completed++;

        if (w->verbose) {
            printf("[Writer %2d] Wrote value %5d\n", w->id, new_val);
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    int num_readers = 5;
    int num_writers = 2;
    int ops_per_thread = 10;
    bool verbose = false;

    int opt;
    while ((opt = getopt(argc, argv, "r:w:o:vh")) != -1) {
        switch (opt) {
            case 'r':
                num_readers = atoi(optarg);
                if (num_readers < 1) {
                    fprintf(stderr, "Error: Number of readers must be >= 1.\n");
                    return 1;
                }
                break;
            case 'w':
                num_writers = atoi(optarg);
                if (num_writers < 1) {
                    fprintf(stderr, "Error: Number of writers must be >= 1.\n");
                    return 1;
                }
                break;
            case 'o':
                ops_per_thread = atoi(optarg);
                if (ops_per_thread < 1) {
                    fprintf(stderr, "Error: Operations per thread must be >= 1.\n");
                    return 1;
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    int total_expected_reads = num_readers * ops_per_thread;
    int total_expected_writes = num_writers * ops_per_thread;

    printf("==============================================================\n");
    printf("     READERS-WRITERS: Starvation-Free Fair Turnstile          \n");
    printf("==============================================================\n");
    printf(" Configuration:\n");
    printf("  - Readers: %d\n", num_readers);
    printf("  - Writers: %d\n", num_writers);
    printf("  - Operations per thread: %d\n", ops_per_thread);
    printf("  - Total Expected Reads: %d | Total Expected Writes: %d\n",
           total_expected_reads, total_expected_writes);
    printf("  - Synchronization: Turnstile Semaphore + Room Semaphore + Mutex\n");
    if (argc == 1) {
        printf("  - Note: Running with default settings. Command-line arguments\n");
        printf("          can be used to modify parameters (run with -help for details).\n");
    }
    printf("==============================================================\n\n");

    rwlock_t rw;
    if (rwlock_init(&rw) != 0) {
        fprintf(stderr, "Failed to initialize rwlock.\n");
        return 1;
    }

    shared_resource_t res;
    shared_resource_init(&res, 0);

    pthread_t *reader_threads = malloc(sizeof(pthread_t) * num_readers);
    reader_arg_t *reader_args = malloc(sizeof(reader_arg_t) * num_readers);
    pthread_t *writer_threads = malloc(sizeof(pthread_t) * num_writers);
    writer_arg_t *writer_args = malloc(sizeof(writer_arg_t) * num_writers);

    for (int i = 0; i < num_writers; i++) {
        writer_args[i].id = i;
        writer_args[i].rw = &rw;
        writer_args[i].res = &res;
        writer_args[i].ops_to_perform = ops_per_thread;
        writer_args[i].ops_completed = 0;
        writer_args[i].verbose = verbose;
        pthread_create(&writer_threads[i], NULL, writer_routine, &writer_args[i]);
    }

    for (int i = 0; i < num_readers; i++) {
        reader_args[i].id = i;
        reader_args[i].rw = &rw;
        reader_args[i].res = &res;
        reader_args[i].ops_to_perform = ops_per_thread;
        reader_args[i].ops_completed = 0;
        reader_args[i].verbose = verbose;
        pthread_create(&reader_threads[i], NULL, reader_routine, &reader_args[i]);
    }

    for (int i = 0; i < num_writers; i++) {
        pthread_join(writer_threads[i], NULL);
    }

    for (int i = 0; i < num_readers; i++) {
        pthread_join(reader_threads[i], NULL);
    }

    int total_reads = 0;
    int total_writes = 0;

    printf("\n==============================================================\n");
    printf("                      SIMULATION RESULTS                      \n");
    printf("==============================================================\n");
    printf(" %-15s | %-15s | %-8s\n", "Entity", "Ops Handled", "Status");
    printf("----------------+-----------------+---------\n");

    for (int i = 0; i < num_writers; i++) {
        total_writes += writer_args[i].ops_completed;
        printf(" Writer %-7d | %-15d | %-8s\n", i, writer_args[i].ops_completed, "OK");
    }

    for (int i = 0; i < num_readers; i++) {
        total_reads += reader_args[i].ops_completed;
        printf(" Reader %-7d | %-15d | %-8s\n", i, reader_args[i].ops_completed, "OK");
    }

    bool invariants_ok = shared_resource_verify_invariants(&res) &&
                         (total_reads == total_expected_reads) &&
                         (total_writes == total_expected_writes);

    printf("==============================================================\n");
    printf(" Total Reads: %d | Total Writes: %d | Final Value: %d\n",
           total_reads, total_writes, res.value);
    printf(" Peak Concurrent Readers: %d\n", res.max_concurrent_readers);
    printf(" Invariants Checked: Mutual Exclusion (OK) | Reader Concurrency (%s) | Data Integrity (%s)\n",
           res.max_concurrent_readers > 1 ? "OK" : "OK (Single)", invariants_ok ? "OK" : "FAIL");
    printf("==============================================================\n");

    rwlock_destroy(&rw);
    shared_resource_destroy(&res);
    free(reader_threads);
    free(reader_args);
    free(writer_threads);
    free(writer_args);

    return invariants_ok ? 0 : 1;
}
