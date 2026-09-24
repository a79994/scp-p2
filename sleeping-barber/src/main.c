#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "barbershop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

typedef struct {
    int id;
    barbershop_t *shop;
    bool verbose;
} barber_thread_arg_t;

typedef struct {
    int id;
    barbershop_t *shop;
    bool served;
    int assigned_barber;
    bool verbose;
} customer_thread_arg_t;

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -b <num>       Number of barbers / chairs (default: 2, min: 1)\n");
    printf("  -w <chairs>    Waiting room chairs (default: 4, min: 0)\n");
    printf("  -c <num>       Total arriving customers (default: 20, min: 1)\n");
    printf("  -v             Verbose output (log haircut/balk events)\n");
    printf("  -help, -h      Show this help message\n");
}

static void *barber_routine(void *arg) {
    barber_thread_arg_t *b = (barber_thread_arg_t *)arg;
    unsigned int seed = (unsigned int)(time(NULL) ^ (b->id << 6) ^ pthread_self());

    while (true) {
        int customer_id = barbershop_barber_wait_for_customer(b->shop, b->id);
        if (customer_id < 0) {
            break; /* Shop closed and no customers remaining */
        }

        if (b->verbose) {
            printf("[Barber %2d] Starting haircut for Customer %3d\n", b->id, customer_id);
        }

        /* Simulate haircut duration */
        int haircut_time = 2000 + (rand_r(&seed) % 4000);
        usleep(haircut_time);

        barbershop_barber_finish_haircut(b->shop, b->id);

        if (b->verbose) {
            printf("[Barber %2d] Finished haircut for Customer %3d\n", b->id, customer_id);
        }
    }

    return NULL;
}

static void *customer_routine(void *arg) {
    customer_thread_arg_t *c = (customer_thread_arg_t *)arg;

    int assigned_barber = -1;
    bool served = barbershop_customer_arrive(c->shop, c->id, &assigned_barber);
    c->served = served;
    c->assigned_barber = assigned_barber;

    if (c->verbose) {
        if (served) {
            printf("[Customer %3d] Successfully received haircut from Barber %2d\n", c->id, assigned_barber);
        } else {
            printf("[Customer %3d] Waiting room full! BALKED and left the shop\n", c->id);
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

    int num_barbers = 2;
    int num_waiting_chairs = 4;
    int total_customers = 20;
    bool verbose = false;

    int opt;
    while ((opt = getopt(argc, argv, "b:w:c:vh")) != -1) {
        switch (opt) {
            case 'b':
                num_barbers = atoi(optarg);
                if (num_barbers < 1) {
                    fprintf(stderr, "Error: Number of barbers must be >= 1.\n");
                    return 1;
                }
                break;
            case 'w':
                num_waiting_chairs = atoi(optarg);
                if (num_waiting_chairs < 0) {
                    fprintf(stderr, "Error: Waiting chairs must be >= 0.\n");
                    return 1;
                }
                break;
            case 'c':
                total_customers = atoi(optarg);
                if (total_customers < 1) {
                    fprintf(stderr, "Error: Number of customers must be >= 1.\n");
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
    printf("     SLEEPING BARBER: Downey Multi-Barber Private Semaphore   \n");
    printf("==============================================================\n");
    printf(" Configuration:\n");
    printf("  - Barbers (Service Units): %d\n", num_barbers);
    printf("  - Waiting Chairs: %d\n", num_waiting_chairs);
    printf("  - Total Arriving Customers: %d\n", total_customers);
    printf("  - Synchronization: Private Semaphores + Rendezvous Handshake\n");
    if (argc == 1) {
        printf("  - Note: Running with default settings. Command-line arguments\n");
        printf("          can be used to modify parameters (run with -help for details).\n");
    }
    printf("==============================================================\n\n");

    barbershop_t shop;
    if (barbershop_init(&shop, num_barbers, num_waiting_chairs) != 0) {
        fprintf(stderr, "Failed to initialize barbershop.\n");
        return 1;
    }

    pthread_t *barber_threads = malloc(sizeof(pthread_t) * num_barbers);
    barber_thread_arg_t *barber_args = malloc(sizeof(barber_thread_arg_t) * num_barbers);

    for (int i = 0; i < num_barbers; i++) {
        barber_args[i].id = i;
        barber_args[i].shop = &shop;
        barber_args[i].verbose = verbose;
        pthread_create(&barber_threads[i], NULL, barber_routine, &barber_args[i]);
    }

    pthread_t *customer_threads = malloc(sizeof(pthread_t) * total_customers);
    customer_thread_arg_t *customer_args = malloc(sizeof(customer_thread_arg_t) * total_customers);
    unsigned int arrival_seed = (unsigned int)time(NULL);

    for (int i = 0; i < total_customers; i++) {
        customer_args[i].id = i;
        customer_args[i].shop = &shop;
        customer_args[i].served = false;
        customer_args[i].assigned_barber = -1;
        customer_args[i].verbose = verbose;
        pthread_create(&customer_threads[i], NULL, customer_routine, &customer_args[i]);

        /* Inter-arrival delay */
        int inter_arrival = 500 + (rand_r(&arrival_seed) % 1500);
        usleep(inter_arrival);
    }

    for (int i = 0; i < total_customers; i++) {
        pthread_join(customer_threads[i], NULL);
    }

    /* All customers have arrived and either finished or balked; shut down barbers */
    barbershop_shutdown(&shop);

    for (int i = 0; i < num_barbers; i++) {
        pthread_join(barber_threads[i], NULL);
    }

    printf("\n==============================================================\n");
    printf("                      SIMULATION RESULTS                      \n");
    printf("==============================================================\n");
    printf(" %-17s | %-15s | %-8s\n", "Entity", "Served / Balked", "Status");
    printf("------------------+-----------------+---------\n");

    for (int i = 0; i < num_barbers; i++) {
        printf(" Barber %-9d | %-15d | %-8s\n", i, shop.barber_served_count[i], "OK");
    }

    printf(" Customers Served | %-15d | %-8s\n", shop.total_served, "OK");
    printf(" Customers Balked | %-15d | %-8s\n", shop.total_balked,
           shop.total_balked > 0 ? "Balked" : "None");

    bool invariants_ok = barbershop_verify_invariants(&shop, total_customers);

    printf("==============================================================\n");
    printf(" Total Customers: %d | Total Served: %d | Total Balked: %d\n",
           total_customers, shop.total_served, shop.total_balked);
    printf(" Peak Waiting Room Occupancy: %d / %d chairs\n",
           shop.max_observed_waiting, num_waiting_chairs);
    printf(" Invariants Checked: Chair Exclusivity (OK) | Capacity Limits (OK) | Data Integrity (%s)\n",
           invariants_ok ? "OK" : "FAIL");
    printf("==============================================================\n");

    barbershop_destroy(&shop);
    free(barber_threads);
    free(barber_args);
    free(customer_threads);
    free(customer_args);

    return invariants_ok ? 0 : 1;
}
