#ifndef PROTOCOL_H
#define PROTOCOL_H

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>

#define DEFAULT_BROKER_PORT 9876
#define DEFAULT_BROKER_HOST "127.0.0.1"
#define DATA_PAYLOAD_SIZE 8

typedef enum {
    /* Client -> Broker */
    MSG_REGISTER       = 1, /* Client registers role and ID */
    MSG_PUBLISH        = 2, /* Writer publishes data payload to Broker Ingestion Queue */
    MSG_FETCH          = 3, /* Reader requests data snapshot from Broker */
    MSG_DISCONNECT     = 4, /* Client disconnects */
    MSG_STATS_REQ      = 5, /* Query invariant and telemetry stats */
    MSG_SHUTDOWN       = 6, /* Command to shutdown the broker */

    /* Broker -> Client */
    MSG_REGISTER_ACK   = 10,
    MSG_WRITE_ACK      = 11, /* Write committed: version and confirmation */
    MSG_FETCH_RESP     = 12, /* Read response containing snapshot */
    MSG_STATS_RESP     = 13, /* Stats response */
    MSG_SHUTDOWN_ACK   = 14,
    MSG_ERROR          = 99
} msg_type_t;

typedef enum {
    ROLE_READER = 1,
    ROLE_WRITER = 2,
    ROLE_ADMIN  = 3
} client_role_t;

typedef struct {
    int32_t values[DATA_PAYLOAD_SIZE];
    int32_t version;
    int32_t last_writer_id;
    uint64_t timestamp_us;
} resource_snapshot_t;

typedef struct {
    int32_t active_readers;
    int32_t active_writers;
    int32_t max_concurrent_readers;
    int32_t total_reads_completed;
    int32_t total_writes_completed;
    int32_t invariant_violations;
} broker_telemetry_t;

typedef struct {
    int32_t type;                      /* msg_type_t */
    int32_t client_id;                 /* Client process ID / logical ID */
    int32_t role;                      /* client_role_t */
    int32_t status;                    /* 0 = OK, error code otherwise */
    resource_snapshot_t snapshot;      /* Resource data payload / snapshot */
    broker_telemetry_t telemetry;      /* Broker statistics */
} broker_msg_t;

/* Socket transmission helpers */
ssize_t send_msg(int fd, const broker_msg_t *msg);
ssize_t recv_msg(int fd, broker_msg_t *msg);

/* Timing utility */
uint64_t get_time_us(void);
void get_formatted_time(char *buffer, size_t buf_size);

#endif /* PROTOCOL_H */
