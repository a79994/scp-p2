#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "broker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

typedef struct {
    broker_server_t *broker;
    int client_fd;
} client_handler_arg_t;

static void acquire_read_lock(broker_server_t *b) {
    pthread_mutex_lock(&b->turnstile);

    pthread_mutex_lock(&b->read_mutex);
    b->readers_count++;
    if (b->readers_count == 1) {
        pthread_mutex_lock(&b->room_mutex);
        while (b->writer_in_room) {
            pthread_cond_wait(&b->room_empty_cv, &b->room_mutex);
        }
        b->room_has_readers = true;
        pthread_mutex_unlock(&b->room_mutex);
    }

    pthread_mutex_lock(&b->stats_lock);
    b->stats.active_readers = b->readers_count;
    if (b->stats.active_readers > b->stats.max_concurrent_readers) {
        b->stats.max_concurrent_readers = b->stats.active_readers;
    }
    if (b->writer_in_room) {
        b->stats.invariant_violations++;
    }
    pthread_mutex_unlock(&b->stats_lock);

    pthread_mutex_unlock(&b->read_mutex);
    pthread_mutex_unlock(&b->turnstile);
}

static void release_read_lock(broker_server_t *b) {
    pthread_mutex_lock(&b->read_mutex);
    b->readers_count--;

    pthread_mutex_lock(&b->stats_lock);
    b->stats.active_readers = b->readers_count;
    b->stats.total_reads_completed++;
    if (b->writer_in_room) {
        b->stats.invariant_violations++;
    }
    pthread_mutex_unlock(&b->stats_lock);

    if (b->readers_count == 0) {
        pthread_mutex_lock(&b->room_mutex);
        b->room_has_readers = false;
        pthread_cond_broadcast(&b->room_empty_cv);
        pthread_mutex_unlock(&b->room_mutex);
    }
    pthread_mutex_unlock(&b->read_mutex);
}

static void acquire_write_lock(broker_server_t *b) {
    pthread_mutex_lock(&b->turnstile);

    pthread_mutex_lock(&b->room_mutex);
    while (b->writer_in_room || b->room_has_readers) {
        pthread_cond_wait(&b->room_empty_cv, &b->room_mutex);
    }
    b->writer_in_room = true;

    pthread_mutex_lock(&b->stats_lock);
    b->stats.active_writers = 1;
    if (b->readers_count > 0) {
        b->stats.invariant_violations++;
    }
    pthread_mutex_unlock(&b->stats_lock);
    pthread_mutex_unlock(&b->room_mutex);
}

static void release_write_lock(broker_server_t *b) {
    pthread_mutex_lock(&b->room_mutex);
    b->writer_in_room = false;

    pthread_mutex_lock(&b->stats_lock);
    b->stats.active_writers = 0;
    b->stats.total_writes_completed++;
    if (b->readers_count > 0) {
        b->stats.invariant_violations++;
    }
    pthread_mutex_unlock(&b->stats_lock);

    pthread_cond_broadcast(&b->room_empty_cv);
    pthread_mutex_unlock(&b->room_mutex);

    pthread_mutex_unlock(&b->turnstile);
}

static void *client_handler_thread(void *arg) {
    client_handler_arg_t *carg = (client_handler_arg_t *)arg;
    broker_server_t *b = carg->broker;
    int fd = carg->client_fd;
    free(carg);

    broker_msg_t msg;
    while (b->running) {
        ssize_t n = recv_msg(fd, &msg);
        if (n <= 0) {
            break;
        }

        broker_msg_t resp;
        memset(&resp, 0, sizeof(resp));

        switch (msg.type) {
            case MSG_REGISTER: {
                resp.type = MSG_REGISTER_ACK;
                resp.client_id = msg.client_id;
                resp.role = msg.role;
                resp.status = 0;
                send_msg(fd, &resp);
                if (b->verbose) {
                    char tbuf[32];
                    get_formatted_time(tbuf, sizeof(tbuf));
                    printf("[%s][Broker] Registered Client %d (Role: %s)\n",
                           tbuf, msg.client_id,
                           msg.role == ROLE_WRITER ? "WRITER" : (msg.role == ROLE_READER ? "READER" : "ADMIN"));
                    fflush(stdout);
                }
                break;
            }

            case MSG_FETCH: {
                acquire_read_lock(b);

                pthread_mutex_lock(&b->resource_lock);
                resp.snapshot = b->resource;
                pthread_mutex_unlock(&b->resource_lock);

                usleep(300);

                release_read_lock(b);

                resp.type = MSG_FETCH_RESP;
                resp.client_id = msg.client_id;
                resp.status = 0;
                send_msg(fd, &resp);

                if (b->verbose) {
                    char tbuf[32];
                    get_formatted_time(tbuf, sizeof(tbuf));
                    printf("[%s][Broker] Served FETCH to Reader %2d (Val: %5d, Ver: %d, Concurrent: %d)\n",
                           tbuf, msg.client_id, resp.snapshot.values[0], resp.snapshot.version,
                           b->stats.active_readers);
                    fflush(stdout);
                }
                break;
            }

            case MSG_PUBLISH: {
                acquire_write_lock(b);

                pthread_mutex_lock(&b->resource_lock);
                for (int i = 0; i < DATA_PAYLOAD_SIZE; i++) {
                    b->resource.values[i] = msg.snapshot.values[i];
                    if (i == DATA_PAYLOAD_SIZE / 2) {
                        usleep(150);
                    }
                }
                b->resource.version++;
                b->resource.last_writer_id = msg.client_id;
                b->resource.timestamp_us = get_time_us();

                resp.snapshot = b->resource;
                pthread_mutex_unlock(&b->resource_lock);

                release_write_lock(b);

                resp.type = MSG_WRITE_ACK;
                resp.client_id = msg.client_id;
                resp.status = 0;
                send_msg(fd, &resp);

                if (b->verbose) {
                    char tbuf[32];
                    get_formatted_time(tbuf, sizeof(tbuf));
                    printf("[%s][Broker] Ingested PUBLISH from Writer %2d (Val: %5d, New Ver: %d)\n",
                           tbuf, msg.client_id, msg.snapshot.values[0], resp.snapshot.version);
                    fflush(stdout);
                }
                break;
            }

            case MSG_STATS_REQ: {
                pthread_mutex_lock(&b->stats_lock);
                resp.telemetry = b->stats;
                pthread_mutex_unlock(&b->stats_lock);

                pthread_mutex_lock(&b->resource_lock);
                resp.snapshot = b->resource;
                pthread_mutex_unlock(&b->resource_lock);

                resp.type = MSG_STATS_RESP;
                resp.status = 0;
                send_msg(fd, &resp);
                break;
            }

            case MSG_SHUTDOWN: {
                resp.type = MSG_SHUTDOWN_ACK;
                resp.status = 0;
                send_msg(fd, &resp);
                b->running = false;
                shutdown(b->server_fd, SHUT_RDWR);
                close(fd);
                return NULL;
            }

            case MSG_DISCONNECT: {
                close(fd);
                return NULL;
            }

            default:
                break;
        }
    }

    close(fd);
    return NULL;
}

static void *broker_accept_loop(void *arg) {
    broker_server_t *b = (broker_server_t *)arg;

    while (b->running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(b->server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd < 0) {
            if (!b->running) break;
            if (errno == EINTR || errno == EAGAIN) continue;
            break;
        }

        client_handler_arg_t *carg = malloc(sizeof(client_handler_arg_t));
        if (!carg) {
            close(client_fd);
            continue;
        }
        carg->broker = b;
        carg->client_fd = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler_thread, carg) == 0) {
            pthread_detach(tid);
        } else {
            free(carg);
            close(client_fd);
        }
    }

    return NULL;
}

int broker_init(broker_server_t *b, int port, bool verbose) {
    memset(b, 0, sizeof(*b));
    b->port = port;
    b->verbose = verbose;
    b->running = false;
    b->server_fd = -1;

    pthread_mutex_init(&b->turnstile, NULL);
    pthread_mutex_init(&b->read_mutex, NULL);
    pthread_mutex_init(&b->room_mutex, NULL);
    pthread_cond_init(&b->room_empty_cv, NULL);
    pthread_mutex_init(&b->resource_lock, NULL);
    pthread_mutex_init(&b->stats_lock, NULL);

    b->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (b->server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(b->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    if (bind(b->server_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind");
        close(b->server_fd);
        b->server_fd = -1;
        return -1;
    }

    if (listen(b->server_fd, 64) < 0) {
        perror("listen");
        close(b->server_fd);
        b->server_fd = -1;
        return -1;
    }

    /* If port was 0, retrieve dynamic port chosen by the OS */
    socklen_t len = sizeof(sin);
    if (getsockname(b->server_fd, (struct sockaddr *)&sin, &len) == 0) {
        b->port = ntohs(sin.sin_port);
    }

    return 0;
}

int broker_start(broker_server_t *b) {
    if (b->server_fd < 0) return -1;
    b->running = true;
    if (pthread_create(&b->accept_thread, NULL, broker_accept_loop, b) != 0) {
        b->running = false;
        return -1;
    }
    return 0;
}

void broker_stop(broker_server_t *b) {
    if (!b->running) return;
    b->running = false;
    if (b->server_fd >= 0) {
        shutdown(b->server_fd, SHUT_RDWR);
        close(b->server_fd);
        b->server_fd = -1;
    }
    pthread_join(b->accept_thread, NULL);

    pthread_mutex_destroy(&b->turnstile);
    pthread_mutex_destroy(&b->read_mutex);
    pthread_mutex_destroy(&b->room_mutex);
    pthread_cond_destroy(&b->room_empty_cv);
    pthread_mutex_destroy(&b->resource_lock);
    pthread_mutex_destroy(&b->stats_lock);
}

void broker_run_loop(broker_server_t *b) {
    if (b->server_fd < 0) return;
    b->running = true;
    broker_accept_loop(b);
}

int broker_get_port(broker_server_t *b) {
    return b->port;
}

bool broker_verify_invariants(broker_server_t *b) {
    pthread_mutex_lock(&b->stats_lock);
    bool ok = (b->stats.invariant_violations == 0) &&
              (b->stats.active_writers == 0) &&
              (b->stats.active_readers == 0);
    pthread_mutex_unlock(&b->stats_lock);
    return ok;
}
