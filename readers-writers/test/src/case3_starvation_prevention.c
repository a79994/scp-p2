#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define FLOOD_READERS 8

static void flood_reader_proc(int id, int port) {
    client_conn_t conn;
    if (client_connect(&conn, "127.0.0.1", port, ROLE_READER, id) != 0) {
        exit(1);
    }

    uint64_t start = get_time_us();
    /* Run for 1.2 seconds */
    while (get_time_us() - start < 1200000ULL) {
        resource_snapshot_t snap;
        client_fetch(&conn, &snap);
        usleep(1000);
    }

    client_disconnect(&conn);
    exit(0);
}

static void waiting_writer_proc(int port) {
    /* Wait until the readers flood is actively underway */
    usleep(200000);

    client_conn_t conn;
    if (client_connect(&conn, "127.0.0.1", port, ROLE_WRITER, 100) != 0) {
        exit(1);
    }

    printf("  [Test] Writer publishing update amidst heavy continuous reader flood...\n");
    int32_t values[DATA_PAYLOAD_SIZE];
    for (int k = 0; k < DATA_PAYLOAD_SIZE; k++) {
        values[k] = 99999;
    }

    int32_t ver = 0;
    uint64_t t0 = get_time_us();
    bool ok = client_publish(&conn, values, &ver);
    uint64_t wait_ms = (get_time_us() - t0) / 1000;

    client_disconnect(&conn);

    if (ok) {
        printf("  [Test] Writer successfully serviced by Broker in %lu ms! (Committed Version: %d)\n",
               wait_ms, ver);
        exit(0);
    } else {
        fprintf(stderr, COLOR_RED "  [Test] Writer publish failed!\n" COLOR_RESET);
        exit(1);
    }
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 3] Writer Starvation Prevention (Fair Turnstile) ===" COLOR_RESET "\n");
    init_test_watchdog(10);

    broker_server_t broker;
    if (broker_init(&broker, 0, false) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to initialize broker!\n" COLOR_RESET);
        return 1;
    }
    int port = broker_get_port(&broker);

    if (broker_start(&broker) != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Failed to start broker!\n" COLOR_RESET);
        return 1;
    }

    pid_t readers[FLOOD_READERS];
    for (int i = 0; i < FLOOD_READERS; i++) {
        readers[i] = fork();
        if (readers[i] == 0) {
            flood_reader_proc(i, port);
        }
    }

    pid_t writer_pid = fork();
    if (writer_pid == 0) {
        waiting_writer_proc(port);
    }

    int writer_status;
    waitpid(writer_pid, &writer_status, 0);

    for (int i = 0; i < FLOOD_READERS; i++) {
        int status;
        waitpid(readers[i], &status, 0);
    }

    cancel_test_watchdog();

    client_conn_t admin;
    broker_telemetry_t stats;
    if (client_connect(&admin, "127.0.0.1", port, ROLE_ADMIN, 999) == 0) {
        client_query_stats(&admin, &stats);
        client_disconnect(&admin);
    }
    broker_stop(&broker);

    printf(" Checking results:\n");
    printf("  - Total reads during flood: %d\n", stats.total_reads_completed);
    printf("  - Total writes committed: %d\n", stats.total_writes_completed);
    printf("  - Invariant violations: %d\n", stats.invariant_violations);

    if (!WIFEXITED(writer_status) || WEXITSTATUS(writer_status) != 0 || stats.total_writes_completed < 1) {
        fprintf(stderr, COLOR_RED "[FAIL] Case 3 failed: Writer starved or failed!\n" COLOR_RESET);
        return 1;
    }

    if (stats.invariant_violations != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariant violations detected (%d)!\n" COLOR_RESET, stats.invariant_violations);
        return 1;
    }

    printf(COLOR_GREEN "[PASS] Case 3 passed: Writer successfully serviced without starvation.\n" COLOR_RESET);
    return 0;
}
