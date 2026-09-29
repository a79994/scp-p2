#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define NUM_READERS 4
#define NUM_WRITERS 2
#define OPS_PER_PROCESS 25

static void reader_proc(int id, int port) {
    client_conn_t conn;
    if (client_connect(&conn, "127.0.0.1", port, ROLE_READER, id) != 0) {
        exit(1);
    }
    for (int i = 0; i < OPS_PER_PROCESS; i++) {
        usleep(300);
        resource_snapshot_t snap;
        if (!client_fetch(&conn, &snap)) {
            exit(1);
        }
    }
    client_disconnect(&conn);
    exit(0);
}

static void writer_proc(int id, int port) {
    client_conn_t conn;
    if (client_connect(&conn, "127.0.0.1", port, ROLE_WRITER, id) != 0) {
        exit(1);
    }
    for (int i = 0; i < OPS_PER_PROCESS; i++) {
        usleep(500);
        int32_t values[DATA_PAYLOAD_SIZE];
        for (int k = 0; k < DATA_PAYLOAD_SIZE; k++) {
            values[k] = (id + 1) * 100 + i;
        }
        int32_t ver = 0;
        if (!client_publish(&conn, values, &ver)) {
            exit(1);
        }
    }
    client_disconnect(&conn);
    exit(0);
}

int main(void) {
    printf(COLOR_CYAN "=== [TEST CASE 1] Basic Readers-Writers Lifecycle (Message Broker) ===" COLOR_RESET "\n");
    init_test_watchdog(12);

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

    for (int i = 0; i < NUM_READERS; i++) {
        int status;
        waitpid(readers[i], &status, 0);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        int status;
        waitpid(writers[i], &status, 0);
    }

    cancel_test_watchdog();

    int expected_reads = NUM_READERS * OPS_PER_PROCESS;
    int expected_writes = NUM_WRITERS * OPS_PER_PROCESS;

    client_conn_t admin;
    broker_telemetry_t stats;
    if (client_connect(&admin, "127.0.0.1", port, ROLE_ADMIN, 999) == 0) {
        client_query_stats(&admin, &stats);
        client_disconnect(&admin);
    } else {
        fprintf(stderr, COLOR_RED "[FAIL] Could not query broker stats!\n" COLOR_RESET);
        broker_stop(&broker);
        return 1;
    }

    printf(" Checking results:\n");
    printf("  - Total reads completed: %d (expected: %d)\n", stats.total_reads_completed, expected_reads);
    printf("  - Total writes completed: %d (expected: %d)\n", stats.total_writes_completed, expected_writes);
    printf("  - Peak concurrent readers: %d\n", stats.max_concurrent_readers);
    printf("  - Invariant violations: %d\n", stats.invariant_violations);

    broker_stop(&broker);

    if (stats.total_reads_completed != expected_reads || stats.total_writes_completed != expected_writes) {
        fprintf(stderr, COLOR_RED "[FAIL] Operation count mismatch!\n" COLOR_RESET);
        return 1;
    }

    if (stats.invariant_violations != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Invariant violations detected (%d)!\n" COLOR_RESET, stats.invariant_violations);
        return 1;
    }

    printf(COLOR_GREEN "[PASS] Case 1 passed: Basic lifecycle and operation counts verified.\n" COLOR_RESET);
    return 0;
}
