#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

typedef struct {
    int entity_type; /* 0: Producer, 1: Consumer */
    int id;
    int items_handled;
} process_report_t;

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -p <num>       Number of producers (default: 2, min: 1)\n");
    printf("  -c <num>       Number of consumers (default: 2, min: 1)\n");
    printf("  -b <size>      Buffer capacity (default: 5, min: 1)\n");
    printf("  -i <items>     Total items to produce (default: 20, min: 1)\n");
    printf("  -v             Verbose output (log each produce/consume)\n");
    printf("  -help, -h      Show this help message\n");
}

static void producer_process(int id, buffer_t *b, int items_to_produce, bool verbose, int report_fd) {
    buffer_close_producer_unused(b);
    unsigned int seed = (unsigned int)(time(NULL) ^ (id << 8) ^ getpid());
    int items_produced = 0;

    for (int i = 0; i < items_to_produce; i++) {
        item_t item;
        item.value = id * 10000 + i + 1;
        item.producer_id = id;

        int delay = 1000 + (rand_r(&seed) % 4000);
        usleep(delay);

        if (buffer_push(b, item)) {
            items_produced++;
            if (verbose) {
                printf("[Producer %2d (PID %d)] Produced item %5d (Buffer count: %d)\n",
                       id, getpid(), item.value, buffer_get_count(b));
                fflush(stdout);
            }
        } else {
            break;
        }
    }

    process_report_t report = {
        .entity_type = 0,
        .id = id,
        .items_handled = items_produced
    };
    (void)write(report_fd, &report, sizeof(report));
    close(report_fd);
    buffer_destroy(b);
    exit(0);
}

static void consumer_process(int id, buffer_t *b, bool verbose, int report_fd) {
    buffer_close_consumer_unused(b);
    unsigned int seed = (unsigned int)(time(NULL) ^ (id << 10) ^ getpid());
    int items_consumed = 0;

    item_t item;
    while (buffer_pop(b, &item)) {
        items_consumed++;
        if (verbose) {
            printf("[Consumer %2d (PID %d)] Consumed item %5d from Producer %2d (Buffer count: %d)\n",
                   id, getpid(), item.value, item.producer_id, buffer_get_count(b));
            fflush(stdout);
        }

        int delay = 1000 + (rand_r(&seed) % 4000);
        usleep(delay);
    }

    process_report_t report = {
        .entity_type = 1,
        .id = id,
        .items_handled = items_consumed
    };
    (void)write(report_fd, &report, sizeof(report));
    close(report_fd);
    buffer_destroy(b);
    exit(0);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    int num_producers = 2;
    int num_consumers = 2;
    int buffer_capacity = 5;
    int total_items = 20;
    bool verbose = false;

    int opt;
    while ((opt = getopt(argc, argv, "p:c:b:i:vh")) != -1) {
        switch (opt) {
            case 'p':
                num_producers = atoi(optarg);
                if (num_producers < 1) {
                    fprintf(stderr, "Error: Number of producers must be >= 1.\n");
                    return 1;
                }
                break;
            case 'c':
                num_consumers = atoi(optarg);
                if (num_consumers < 1) {
                    fprintf(stderr, "Error: Number of consumers must be >= 1.\n");
                    return 1;
                }
                break;
            case 'b':
                buffer_capacity = atoi(optarg);
                if (buffer_capacity < 1) {
                    fprintf(stderr, "Error: Buffer capacity must be >= 1.\n");
                    return 1;
                }
                break;
            case 'i':
                total_items = atoi(optarg);
                if (total_items < 1) {
                    fprintf(stderr, "Error: Total items must be >= 1.\n");
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

    printf("==============================================================\n");
    printf("     PRODUCER-CONSUMER: Process-Based (UNIX Pipes IPC)        \n");
    printf("==============================================================\n");
    printf(" Configuration:\n");
    printf("  - Producers (Processes): %d\n", num_producers);
    printf("  - Consumers (Processes): %d\n", num_consumers);
    printf("  - Buffer Capacity: %d slots\n", buffer_capacity);
    printf("  - Total Items: %d\n", total_items);
    if (argc == 1) {
        printf("  - Note: Running with default settings. Command-line arguments\n");
        printf("          can be used to modify parameters (run with -help for details).\n");
    }
    printf("==============================================================\n\n");

    buffer_t buffer;
    if (buffer_init(&buffer, buffer_capacity) != 0) {
        fprintf(stderr, "Failed to initialize bounded buffer.\n");
        return 1;
    }

    /* Report pipe used by child processes to report summary statistics to parent */
    int report_pipe[2];
    if (pipe(report_pipe) != 0) {
        fprintf(stderr, "Failed to create IPC report pipe.\n");
        buffer_destroy(&buffer);
        return 1;
    }

    pid_t *prod_pids = malloc(sizeof(pid_t) * num_producers);
    pid_t *cons_pids = malloc(sizeof(pid_t) * num_consumers);
    int *prod_counts = calloc(num_producers, sizeof(int));
    int *cons_counts = calloc(num_consumers, sizeof(int));

    int base_items = total_items / num_producers;
    int remainder = total_items % num_producers;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    /* Fork producer processes */
    for (int i = 0; i < num_producers; i++) {
        int items_to_produce = base_items + (i < remainder ? 1 : 0);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork producer failed");
            exit(1);
        }
        if (pid == 0) {
            /* In child producer process */
            close(report_pipe[0]);
            free(prod_pids);
            free(cons_pids);
            free(prod_counts);
            free(cons_counts);
            producer_process(i, &buffer, items_to_produce, verbose, report_pipe[1]);
        }
        prod_pids[i] = pid;
    }

    /* Fork consumer processes */
    for (int i = 0; i < num_consumers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork consumer failed");
            exit(1);
        }
        if (pid == 0) {
            /* In child consumer process */
            close(report_pipe[0]);
            free(prod_pids);
            free(cons_pids);
            free(prod_counts);
            free(cons_counts);
            consumer_process(i, &buffer, verbose, report_pipe[1]);
        }
        cons_pids[i] = pid;
    }

    /* Parent closes its write descriptor of report pipe */
    close(report_pipe[1]);

    /* Wait for all producers to finish production */
    for (int i = 0; i < num_producers; i++) {
        int status;
        waitpid(prod_pids[i], &status, 0);
    }

    /* Gracefully shutdown buffer: closes items_pipe write end in parent.
     * All producers have finished, so once consumers drain items, they receive EOF.
     */
    buffer_shutdown(&buffer);

    /* Wait for all consumers to complete */
    for (int i = 0; i < num_consumers; i++) {
        int status;
        waitpid(cons_pids[i], &status, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    /* Read reports from child processes via report pipe */
    process_report_t rep;
    while (read(report_pipe[0], &rep, sizeof(rep)) == sizeof(rep)) {
        if (rep.entity_type == 0 && rep.id >= 0 && rep.id < num_producers) {
            prod_counts[rep.id] = rep.items_handled;
        } else if (rep.entity_type == 1 && rep.id >= 0 && rep.id < num_consumers) {
            cons_counts[rep.id] = rep.items_handled;
        }
    }
    close(report_pipe[0]);

    int total_produced = 0;
    int total_consumed = 0;

    printf("\n==============================================================\n");
    printf("                      SIMULATION RESULTS                      \n");
    printf("==============================================================\n");
    printf(" %-15s | %-15s | %-8s\n", "Entity", "Items Handled", "Status");
    printf("----------------+-----------------+---------\n");

    for (int i = 0; i < num_producers; i++) {
        total_produced += prod_counts[i];
        printf(" Producer %-5d | %-15d | %-8s\n", i, prod_counts[i], "OK");
    }

    for (int i = 0; i < num_consumers; i++) {
        total_consumed += cons_counts[i];
        printf(" Consumer %-5d | %-15d | %-8s\n", i, cons_counts[i], "OK");
    }

    printf("==============================================================\n");
    printf(" Elapsed Time: %.2f ms | Total Produced: %d | Total Consumed: %d\n",
           elapsed_ms, total_produced, total_consumed);
    printf("==============================================================\n");

    buffer_destroy(&buffer);
    free(prod_pids);
    free(cons_pids);
    free(prod_counts);
    free(cons_counts);

    return 0;
}
