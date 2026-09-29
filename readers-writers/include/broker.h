#ifndef BROKER_H
#define BROKER_H

#include "protocol.h"
#include <pthread.h>
#include <stdbool.h>

typedef struct {
    int port;
    int server_fd;
    volatile bool running;
    bool verbose;
    pthread_t accept_thread;

    /* Shared resource state stored in Broker */
    resource_snapshot_t resource;
    pthread_mutex_t resource_lock;

    /* Downey Starvation-Free Fair Turnstile Primitives */
    pthread_mutex_t turnstile;
    pthread_mutex_t read_mutex;
    pthread_mutex_t room_mutex;
    pthread_cond_t room_empty_cv;
    int readers_count;
    bool room_has_readers;
    bool writer_in_room;

    /* Invariant and Telemetry Tracking */
    broker_telemetry_t stats;
    pthread_mutex_t stats_lock;
} broker_server_t;

/* Broker Lifecycle */
int broker_init(broker_server_t *broker, int port, bool verbose);
int broker_start(broker_server_t *broker);
void broker_stop(broker_server_t *broker);
void broker_run_loop(broker_server_t *broker);
int broker_get_port(broker_server_t *broker);
bool broker_verify_invariants(broker_server_t *broker);

#endif /* BROKER_H */
