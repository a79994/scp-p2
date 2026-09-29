#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include <stdbool.h>

typedef struct {
    int fd;
    int client_id;
    client_role_t role;
    char host[128];
    int port;
} client_conn_t;

/* Connection management with retry capability */
int client_connect(client_conn_t *c, const char *host, int port, client_role_t role, int client_id);
void client_disconnect(client_conn_t *c);

/* Writer API: publishes new value payload to Broker Ingestion Queue */
bool client_publish(client_conn_t *c, const int32_t values[DATA_PAYLOAD_SIZE], int32_t *out_version);

/* Reader API: fetches snapshot from Broker */
bool client_fetch(client_conn_t *c, resource_snapshot_t *out_snapshot);

/* Management API */
bool client_query_stats(client_conn_t *c, broker_telemetry_t *out_stats);
bool client_request_shutdown(client_conn_t *c);

/* Standalone child process routines */
int reader_process_run(int id, const char *host, int port, int num_ops, bool verbose);
int writer_process_run(int id, const char *host, int port, int num_ops, bool verbose);

#endif /* CLIENT_H */
