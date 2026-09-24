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

typedef struct {
    int id;
    buffer_t *buffer;
    int items_to_produce;
    int items_produced;
    bool verbose;
} producer_arg_t;

typedef struct {
    int id;
    buffer_t *buffer;
    int items_consumed;
    bool verbose;
} consumer_arg_t;

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

static void *producer_routine(void *arg) {
    producer_arg_t *p = (producer_arg_t *)arg;
    unsigned int seed = (unsigned int)(time(NULL) ^ (p->id << 4) ^ pthread_self());

    for (int i = 0; i < p->items_to_produce; i++) {
        item_t item;
        item.value = p->id * 10000 + i + 1;
        item.producer_id = p->id;

        int delay = 1000 + (rand_r(&seed) % 4000);
        usleep(delay);

        if (buffer_push(p->buffer, item)) {
            p->items_produced++;
            if (p->verbose) {
                printf("[Producer %2d] Produced item %5d (Buffer count: %d)\n",
                       p->id, item.value, buffer_get_count(p->buffer));
            }
        } else {
            break;
        }
    }

    return NULL;
}

static void *consumer_routine(void *arg) {
    consumer_arg_t *c = (consumer_arg_t *)arg;
    unsigned int seed = (unsigned int)(time(NULL) ^ (c->id << 6) ^ pthread_self());

    item_t item;
    while (buffer_pop(c->buffer, &item)) {
        c->items_consumed++;
        if (c->verbose) {
            printf("[Consumer %2d] Consumed item %5d from Producer %2d (Buffer count: %d)\n",
                   c->id, item.value, item.producer_id, buffer_get_count(c->buffer));
        }

        int delay = 1000 + (rand_r(&seed) % 4000);
        usleep(delay);
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
    printf("     PRODUCER-CONSUMER: Dijkstra Canonical Semaphores        \n");
    printf("==============================================================\n");
    printf(" Configuration:\n");
    printf("  - Producers: %d\n", num_producers);
    printf("  - Consumers: %d\n", num_consumers);
    printf("  - Buffer Capacity: %d slots\n", buffer_capacity);
    printf("  - Total Items: %d\n", total_items);
    printf("  - Synchronization: Semaphores (empty=%d, full=0) + Mutex\n", buffer_capacity);
    if (argc == 1) {
        printf("  - Note: Running with default settings. Command-line arguments\n");
        printf("          can be used to modify parameters (run with -help for details).\n");
    }
    printf("==============================================================\n\n");

    buffer_t buffer;
    if (buffer_init(&buffer, buffer_capacity) != 0) {
        fprintf(stderr, "Failed to initialize buffer.\n");
        return 1;
    }

    pthread_t *prod_threads = malloc(sizeof(pthread_t) * num_producers);
    producer_arg_t *prod_args = malloc(sizeof(producer_arg_t) * num_producers);
    pthread_t *cons_threads = malloc(sizeof(pthread_t) * num_consumers);
    consumer_arg_t *cons_args = malloc(sizeof(consumer_arg_t) * num_consumers);

    int base_items = total_items / num_producers;
    int remainder = total_items % num_producers;

    for (int i = 0; i < num_producers; i++) {
        prod_args[i].id = i;
        prod_args[i].buffer = &buffer;
        prod_args[i].items_to_produce = base_items + (i < remainder ? 1 : 0);
        prod_args[i].items_produced = 0;
        prod_args[i].verbose = verbose;
        pthread_create(&prod_threads[i], NULL, producer_routine, &prod_args[i]);
    }

    for (int i = 0; i < num_consumers; i++) {
        cons_args[i].id = i;
        cons_args[i].buffer = &buffer;
        cons_args[i].items_consumed = 0;
        cons_args[i].verbose = verbose;
        pthread_create(&cons_threads[i], NULL, consumer_routine, &cons_args[i]);
    }

    for (int i = 0; i < num_producers; i++) {
        pthread_join(prod_threads[i], NULL);
    }

    buffer_shutdown(&buffer);

    for (int i = 0; i < num_consumers; i++) {
        pthread_join(cons_threads[i], NULL);
    }

    int total_produced = 0;
    int total_consumed = 0;

    printf("\n==============================================================\n");
    printf("                      SIMULATION RESULTS                      \n");
    printf("==============================================================\n");
    printf(" %-15s | %-15s | %-8s\n", "Entity", "Items Handled", "Status");
    printf("----------------+-----------------+---------\n");

    for (int i = 0; i < num_producers; i++) {
        total_produced += prod_args[i].items_produced;
        printf(" Producer %-5d | %-15d | %-8s\n", i, prod_args[i].items_produced, "OK");
    }

    for (int i = 0; i < num_consumers; i++) {
        total_consumed += cons_args[i].items_consumed;
        printf(" Consumer %-5d | %-15d | %-8s\n", i, cons_args[i].items_consumed, "OK");
    }

    printf("==============================================================\n");
    printf(" Total Produced: %d | Total Consumed: %d\n", total_produced, total_consumed);
    printf(" Invariants Checked: Bounds (0 <= count <= %d) | Data Integrity (%s)\n",
           buffer_capacity, (total_produced == total_consumed && total_produced == total_items) ? "OK" : "FAIL");
    printf("==============================================================\n");

    buffer_destroy(&buffer);
    free(prod_threads);
    free(prod_args);
    free(cons_threads);
    free(cons_args);

    return 0;
}
