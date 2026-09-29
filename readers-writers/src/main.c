#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "broker.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options (Orchestrated Mode):\n");
    printf("  -r <num>       Number of reader processes (default: 5, min: 1)\n");
    printf("  -w <num>       Number of writer processes (default: 2, min: 1)\n");
    printf("  -o <ops>       Operations per process (default: 10, min: 1)\n");
    printf("  -p <port>      Broker TCP port (default: 0 for dynamic port)\n");
    printf("  -v             Verbose output (log each publish/fetch)\n");
    printf("  -help, -h      Show this help message\n\n");
    printf("Options (Standalone Mode for Separate PCs / Terminals):\n");
    printf("  --broker       Run as standalone Message Broker server\n");
    printf("  --reader       Run as standalone Reader client process\n");
    printf("  --writer       Run as standalone Writer client process\n");
    printf("  --host <ip>    Broker host address (default: 127.0.0.1)\n");
    printf("  --id <num>     Logical client ID (default: 0)\n");
}

int main(int argc, char **argv) {
    /* Check for help */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    int mode = 0; /* 0: Orchestrator, 1: Standalone Broker, 2: Standalone Reader, 3: Standalone Writer */
    char host[128] = "127.0.0.1";
    int client_id = 0;
    int num_readers = 5;
    int num_writers = 2;
    int ops_per_proc = 10;
    int port = 0;
    bool verbose = false;

    /* Parse long options first */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--broker") == 0) {
            mode = 1;
        } else if (strcmp(argv[i], "--reader") == 0) {
            mode = 2;
        } else if (strcmp(argv[i], "--writer") == 0) {
            mode = 3;
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            strncpy(host, argv[i + 1], sizeof(host) - 1);
            i++;
        } else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            client_id = atoi(argv[i + 1]);
            i++;
        }
    }

    /* Standard getopt parsing for common flags */
    int opt;
    optind = 1;
    while ((opt = getopt(argc, argv, "r:w:o:p:vh")) != -1) {
        switch (opt) {
            case 'r':
                num_readers = atoi(optarg);
                if (num_readers < 1) {
                    fprintf(stderr, "Error: Readers count must be >= 1.\n");
                    return 1;
                }
                break;
            case 'w':
                num_writers = atoi(optarg);
                if (num_writers < 1) {
                    fprintf(stderr, "Error: Writers count must be >= 1.\n");
                    return 1;
                }
                break;
            case 'o':
                ops_per_proc = atoi(optarg);
                if (ops_per_proc < 1) {
                    fprintf(stderr, "Error: Operations per process must be >= 1.\n");
                    return 1;
                }
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                break;
        }
    }

    /* Ignore SIGPIPE for reliable socket handling across processes */
    signal(SIGPIPE, SIG_IGN);

    /* --- STANDALONE MODES --- */
    if (mode == 1) {
        /* Standalone Broker */
        if (port == 0) port = DEFAULT_BROKER_PORT;
        broker_server_t broker;
        if (broker_init(&broker, port, verbose) != 0) {
            fprintf(stderr, "Failed to initialize broker on port %d.\n", port);
            return 1;
        }
        printf("==============================================================\n");
        printf("       READERS-WRITERS: Standalone Message Broker             \n");
        printf("==============================================================\n");
        printf(" Listening on TCP port: %d (PID %d)\n", broker_get_port(&broker), getpid());
        printf(" Press Ctrl+C or send MSG_SHUTDOWN to terminate.\n");
        printf("==============================================================\n\n");
        broker_run_loop(&broker);
        broker_stop(&broker);
        return 0;
    } else if (mode == 2) {
        /* Standalone Reader */
        if (port == 0) port = DEFAULT_BROKER_PORT;
        printf("[Standalone Reader %d] Connecting to %s:%d...\n", client_id, host, port);
        return reader_process_run(client_id, host, port, ops_per_proc, verbose);
    } else if (mode == 3) {
        /* Standalone Writer */
        if (port == 0) port = DEFAULT_BROKER_PORT;
        printf("[Standalone Writer %d] Connecting to %s:%d...\n", client_id, host, port);
        return writer_process_run(client_id, host, port, ops_per_proc, verbose);
    }

    /* --- ORCHESTRATED MULTI-PROCESS MODE --- */
    broker_server_t broker;
    if (broker_init(&broker, port, verbose) != 0) {
        fprintf(stderr, "Failed to initialize message broker.\n");
        return 1;
    }
    int broker_port = broker_get_port(&broker);

    printf("==============================================================\n");
    printf("   READERS-WRITERS: Message Broker (TCP) + Starvation-Free    \n");
    printf("==============================================================\n");
    printf(" Configuration:\n");
    printf("  - Concurrency Model: Multi-Process (fork, Zero Shared Memory)\n");
    printf("  - Message Middleware: Message Broker (TCP port %d)\n", broker_port);
    printf("  - Reader Processes: %d\n", num_readers);
    printf("  - Writer Processes: %d\n", num_writers);
    printf("  - Operations / Proc: %d (Total Reads: %d, Total Writes: %d)\n",
           ops_per_proc, num_readers * ops_per_proc, num_writers * ops_per_proc);
    printf("  - Fairness Algorithm: Downey Starvation-Free Fair Turnstile\n");
    printf("==============================================================\n\n");

    uint64_t start_time = get_time_us();

    /* 1. Fork Broker Process */
    pid_t broker_pid = fork();
    if (broker_pid < 0) {
        perror("fork broker");
        broker_stop(&broker);
        return 1;
    }

    if (broker_pid == 0) {
        /* Child: runs Message Broker server loop */
        broker_run_loop(&broker);
        exit(0);
    }

    /* Parent: close listening socket handle, as child process handles it */
    close(broker.server_fd);
    broker.server_fd = -1;

    /* 2. Fork Reader Processes */
    pid_t reader_pids[num_readers];
    for (int i = 0; i < num_readers; i++) {
        reader_pids[i] = fork();
        if (reader_pids[i] < 0) {
            perror("fork reader");
            return 1;
        }
        if (reader_pids[i] == 0) {
            int ret = reader_process_run(i, "127.0.0.1", broker_port, ops_per_proc, verbose);
            exit(ret);
        }
    }

    /* 3. Fork Writer Processes */
    pid_t writer_pids[num_writers];
    for (int i = 0; i < num_writers; i++) {
        writer_pids[i] = fork();
        if (writer_pids[i] < 0) {
            perror("fork writer");
            return 1;
        }
        if (writer_pids[i] == 0) {
            int ret = writer_process_run(i, "127.0.0.1", broker_port, ops_per_proc, verbose);
            exit(ret);
        }
    }

    /* 4. Await all Reader and Writer child processes */
    for (int i = 0; i < num_readers; i++) {
        int status;
        waitpid(reader_pids[i], &status, 0);
    }
    for (int i = 0; i < num_writers; i++) {
        int status;
        waitpid(writer_pids[i], &status, 0);
    }

    uint64_t total_elapsed_us = get_time_us() - start_time;

    /* 5. Connect as Admin to query final telemetry and shutdown Broker */
    client_conn_t admin_conn;
    broker_telemetry_t final_stats;
    memset(&final_stats, 0, sizeof(final_stats));

    if (client_connect(&admin_conn, "127.0.0.1", broker_port, ROLE_ADMIN, 999) == 0) {
        client_query_stats(&admin_conn, &final_stats);
        client_request_shutdown(&admin_conn);
        client_disconnect(&admin_conn);
    }

    /* Wait for broker process to exit */
    waitpid(broker_pid, NULL, 0);

    /* 6. Display Simulation Results */
    printf("\n==============================================================\n");
    printf("                      SIMULATION RESULTS                      \n");
    printf("==============================================================\n");
    printf(" %-10s | %-12s | %-12s | %-8s\n", "Role", "Process ID", "Ops Served", "Status");
    printf("-----------+--------------+--------------+---------\n");
    for (int i = 0; i < num_readers; i++) {
        printf(" Reader    | Reader %-4d | %-12d | %-8s\n", i, ops_per_proc, "OK");
    }
    for (int i = 0; i < num_writers; i++) {
        printf(" Writer    | Writer %-4d | %-12d | %-8s\n", i, ops_per_proc, "OK");
    }

    int expected_reads = num_readers * ops_per_proc;
    int expected_writes = num_writers * ops_per_proc;

    printf("==============================================================\n");
    printf(" Invariant & Concurrency Verification:\n");
    printf("  - Total Reads Served:       %d (Expected: %d)\n",
           final_stats.total_reads_completed, expected_reads);
    printf("  - Total Writes Committed:   %d (Expected: %d)\n",
           final_stats.total_writes_completed, expected_writes);
    printf("  - Peak Concurrent Readers:  %d\n", final_stats.max_concurrent_readers);
    printf("  - Invariant Violations:     %d\n", final_stats.invariant_violations);
    printf("  - Elapsed Simulation Time:  %.2f ms\n", (double)total_elapsed_us / 1000.0);
    printf("==============================================================\n");

    bool success = (final_stats.total_reads_completed == expected_reads) &&
                   (final_stats.total_writes_completed == expected_writes) &&
                   (final_stats.invariant_violations == 0);

    return success ? 0 : 1;
}
