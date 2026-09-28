#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define FLOOD_READERS 10
#define FLOOD_DURATION_MS 1500

static rwlock_t g_rw;
static volatile bool g_keep_reading = true;
static volatile bool g_writer_serviced = false;
static int g_reads_during_flood = 0;
static pthread_mutex_t g_count_lock = PTHREAD_MUTEX_INITIALIZER;

static void *flood_reader(void *arg) {
    (void)arg;
    while (g_keep_reading) {
        rwlock_read_lock(&g_rw);

        pthread_mutex_lock(&g_count_lock);
        g_reads_during_flood++;
        pthread_mutex_unlock(&g_count_lock);

        /* Sleep briefly while reading to maintain continuous reader overlap */
        usleep(10000);

        rwlock_read_unlock(&g_rw);
        usleep(1000);
    }
    return NULL;
}

static void *waiting_writer(void *arg) {
    (void)arg;
    /* Wait until the readers flood is actively underway */
    usleep(200000);

    printf("  [Test] Writer requesting write lock amidst heavy reader stream...\n");
    rwlock_write_lock(&g_rw);

    printf("  [Test] Writer successfully acquired write lock!\n");
    g_writer_serviced = true;
    usleep(50000);

    rwlock_write_unlock(&g_rw);
    return NULL;
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 3] Writer Starvation Prevention (Fair Turnstile) ===" COLOR_RESET "\n");
    init_test_watchdog(8);

    if (rwlock_init(&g_rw) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize rwlock!\n" COLOR_RESET);
        return 1;
    }

    pthread_t readers[FLOOD_READERS];
    pthread_t writer;

    for (int i = 0; i < FLOOD_READERS; i++) {
        pthread_create(&readers[i], NULL, flood_reader, NULL);
    }

    pthread_create(&writer, NULL, waiting_writer, NULL);

    pthread_join(writer, NULL);

    g_keep_reading = false;
    for (int i = 0; i < FLOOD_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    cancel_test_watchdog();

    printf(" Checking results:\n");
    printf("  - Total reads during flood: %d\n", g_reads_during_flood);
    printf("  - Writer was serviced: %s\n", g_writer_serviced ? "YES" : "NO");

    if (!g_writer_serviced) {
        fprintf(stderr, COLOR_RED "[FAIL] Writer starved and was not serviced!\n" COLOR_RESET);
        rwlock_destroy(&g_rw);
        return 1;
    }

    rwlock_destroy(&g_rw);
    printf(COLOR_GREEN "[PASS] Case 3 passed: Writer successfully broke through reader flood (No Starvation).\n" COLOR_RESET);
    return 0;
}
