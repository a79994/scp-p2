#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define NUM_READERS 6
#define NUM_WRITERS 3
#define OPS_PER_PROCESS 30

static void writer_proc(int id, int port) {
    client_conn_t conn;
    if (client_connect(&conn, "127.0.0.1", port, ROLE_WRITER, id) != 0) {
        exit(1);
    }

    for (int i = 0; i < OPS_PER_PROCESS; i++) {
        int token = (id + 1) * 1000 + i;
        int32_t values[DATA_PAYLOAD_SIZE];
        for (int k = 0; k < DATA_PAYLOAD_SIZE; k++) {
            values[k] = token;
        }

        int32_t ver = 0;
        if (!client_publish(&conn, values, &ver)) {
            exit(1);
        }
        usleep(300);
    }

    client_disconnect(&conn);
    exit(0);
}

static void reader_proc(int id, int port) {
    client_conn_t conn;
    if (client_connect(&conn, "127.0.0.1", port, ROLE_READER, id) != 0) {
        exit(1);
    }

    int inconsistency_count = 0;

    for (int i = 0; i < OPS_PER_PROCESS; i++) {
        resource_snapshot_t snap;
        if (!client_fetch(&conn, &snap)) {
            exit(1);
        }

        /* Verify atomicity: all elements in the payload must be identical */
        int expected = snap.values[0];
        for (int k = 1; k < DATA_PAYLOAD_SIZE; k++) {
            if (snap.values[k] != expected) {
                inconsistency_count++;
                fprintf(stderr, COLOR_RED "[FAIL] Reader %d detected inconsistent data! values[0]=%d, values[%d]=%d\n" COLOR_RESET,
                        id, expected, k, snap.values[k]);
                break;
            }
        }
        usleep(200);
    }

    client_disconnect(&conn);
    exit(inconsistency_count > 0 ? 2 : 0);
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 2] Strict Mutual Exclusion & Data Atomicity ===" COLOR_RESET "\n");
    init_test_watchdog(15);

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

    pid_t readers[NUM_READERS];
    pid_t writers[NUM_WRITERS];

    for (int i = 0; i < NUM_READERS; i++) {
        readers[i] = fork();
        if (readers[i] == 0) {
            reader_proc(i, port);
        }
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        writers[i] = fork();
        if (writers[i] == 0) {
            writer_proc(i, port);
        }
    }

    int failures = 0;
    for (int i = 0; i < NUM_READERS; i++) {
        int status;
        waitpid(readers[i], &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            failures++;
        }
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        int status;
        waitpid(writers[i], &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            failures++;
        }
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
    printf("  - Invariant violations: %d\n", stats.invariant_violations);
    printf("  - Process exit failures: %d\n", failures);
    printf("  - Peak concurrent readers: %d\n", stats.max_concurrent_readers);

    if (failures > 0 || stats.invariant_violations != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Case 2 failed: Mutual exclusion violated or torn reads detected!\n" COLOR_RESET);
        return 1;
    }

    printf(COLOR_GREEN "[PASS] Case 2 passed: Strict mutual exclusion and write atomicity guaranteed.\n" COLOR_RESET);
    return 0;
}
