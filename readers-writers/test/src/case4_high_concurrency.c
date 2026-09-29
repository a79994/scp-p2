#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define HIGH_READERS 12
#define HIGH_WRITERS 4
#define OPS_PER_PROCESS 20

static void reader_proc(int id, int port) {
    client_conn_t conn;
    if (client_connect(&conn, "127.0.0.1", port, ROLE_READER, id) != 0) {
        exit(1);
    }

    for (int i = 0; i < OPS_PER_PROCESS; i++) {
        usleep(200);
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
        usleep(400);
        int32_t values[DATA_PAYLOAD_SIZE];
        for (int k = 0; k < DATA_PAYLOAD_SIZE; k++) {
            values[k] = (id + 1) * 1000 + i;
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
    printf(COLOR_CYAN "=== [TEST CASE 4] High Concurrency Stress Test (%d Readers, %d Writers) ===" COLOR_RESET "\n",
           HIGH_READERS, HIGH_WRITERS);
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

    pid_t readers[HIGH_READERS];
    pid_t writers[HIGH_WRITERS];

    for (int i = 0; i < HIGH_READERS; i++) {
        readers[i] = fork();
        if (readers[i] == 0) {
            reader_proc(i, port);
        }
    }

    for (int i = 0; i < HIGH_WRITERS; i++) {
        writers[i] = fork();
        if (writers[i] == 0) {
            writer_proc(i, port);
        }
    }

    int errors = 0;
    for (int i = 0; i < HIGH_READERS; i++) {
        int status;
        waitpid(readers[i], &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            errors++;
        }
    }
    for (int i = 0; i < HIGH_WRITERS; i++) {
        int status;
        waitpid(writers[i], &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            errors++;
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

    int expected_reads = HIGH_READERS * OPS_PER_PROCESS;
    int expected_writes = HIGH_WRITERS * OPS_PER_PROCESS;

    printf(" Checking results:\n");
    printf("  - Total reads completed: %d (expected: %d)\n", stats.total_reads_completed, expected_reads);
    printf("  - Total writes completed: %d (expected: %d)\n", stats.total_writes_completed, expected_writes);
    printf("  - Peak concurrent readers: %d\n", stats.max_concurrent_readers);
    printf("  - Invariant violations: %d\n", stats.invariant_violations);
    printf("  - Child process failures: %d\n", errors);

    if (errors > 0 || stats.invariant_violations != 0) {
        fprintf(stderr, COLOR_RED "[FAIL] Errors or invariant violations detected!\n" COLOR_RESET);
        return 1;
    }

    if (stats.total_reads_completed != expected_reads || stats.total_writes_completed != expected_writes) {
        fprintf(stderr, COLOR_RED "[FAIL] Operation count mismatch!\n" COLOR_RESET);
        return 1;
    }

    if (stats.max_concurrent_readers < 2) {
        fprintf(stderr, COLOR_YELLOW "[WARN] Concurrency low: peak concurrent readers was %d\n" COLOR_RESET,
                stats.max_concurrent_readers);
    }

    printf(COLOR_GREEN "[PASS] Case 4 passed: High concurrency handled flawlessly without deadlocks or violations.\n" COLOR_RESET);
    return 0;
}
