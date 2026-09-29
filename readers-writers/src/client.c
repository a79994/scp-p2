#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

int client_connect(client_conn_t *c, const char *host, int port, client_role_t role, int client_id) {
    memset(c, 0, sizeof(*c));
    c->client_id = client_id;
    c->role = role;
    c->port = port;
    strncpy(c->host, host ? host : "127.0.0.1", sizeof(c->host) - 1);
    c->fd = -1;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (inet_pton(AF_INET, c->host, &sin.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(c->host);
        if (!he) {
            fprintf(stderr, "client_connect: cannot resolve host %s\n", c->host);
            return -1;
        }
        memcpy(&sin.sin_addr, he->h_addr_list[0], he->h_length);
    }

    /* Connect with retries to tolerate broker startup latency */
    int retries = 25;
    while (retries-- > 0) {
        c->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (c->fd < 0) return -1;

        if (connect(c->fd, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
            break;
        }

        close(c->fd);
        c->fd = -1;
        usleep(40000); /* 40 ms */
    }

    if (c->fd < 0) {
        return -1;
    }

    /* Perform registration handshake */
    broker_msg_t reg_msg;
    memset(&reg_msg, 0, sizeof(reg_msg));
    reg_msg.type = MSG_REGISTER;
    reg_msg.client_id = client_id;
    reg_msg.role = role;

    if (send_msg(c->fd, &reg_msg) <= 0) {
        close(c->fd);
        c->fd = -1;
        return -1;
    }

    broker_msg_t ack_msg;
    if (recv_msg(c->fd, &ack_msg) <= 0 || ack_msg.type != MSG_REGISTER_ACK) {
        close(c->fd);
        c->fd = -1;
        return -1;
    }

    return 0;
}

void client_disconnect(client_conn_t *c) {
    if (c->fd >= 0) {
        broker_msg_t disc_msg;
        memset(&disc_msg, 0, sizeof(disc_msg));
        disc_msg.type = MSG_DISCONNECT;
        disc_msg.client_id = c->client_id;
        (void)send_msg(c->fd, &disc_msg);

        close(c->fd);
        c->fd = -1;
    }
}

bool client_publish(client_conn_t *c, const int32_t values[DATA_PAYLOAD_SIZE], int32_t *out_version) {
    if (c->fd < 0) return false;

    broker_msg_t req;
    memset(&req, 0, sizeof(req));
    req.type = MSG_PUBLISH;
    req.client_id = c->client_id;
    req.role = ROLE_WRITER;
    for (int i = 0; i < DATA_PAYLOAD_SIZE; i++) {
        req.snapshot.values[i] = values[i];
    }

    if (send_msg(c->fd, &req) <= 0) return false;

    broker_msg_t resp;
    if (recv_msg(c->fd, &resp) <= 0 || resp.type != MSG_WRITE_ACK) return false;

    if (out_version) {
        *out_version = resp.snapshot.version;
    }
    return true;
}

bool client_fetch(client_conn_t *c, resource_snapshot_t *out_snapshot) {
    if (c->fd < 0) return false;

    broker_msg_t req;
    memset(&req, 0, sizeof(req));
    req.type = MSG_FETCH;
    req.client_id = c->client_id;
    req.role = ROLE_READER;

    if (send_msg(c->fd, &req) <= 0) return false;

    broker_msg_t resp;
    if (recv_msg(c->fd, &resp) <= 0 || resp.type != MSG_FETCH_RESP) return false;

    if (out_snapshot) {
        *out_snapshot = resp.snapshot;
    }
    return true;
}

bool client_query_stats(client_conn_t *c, broker_telemetry_t *out_stats) {
    if (c->fd < 0) return false;

    broker_msg_t req;
    memset(&req, 0, sizeof(req));
    req.type = MSG_STATS_REQ;
    req.client_id = c->client_id;

    if (send_msg(c->fd, &req) <= 0) return false;

    broker_msg_t resp;
    if (recv_msg(c->fd, &resp) <= 0 || resp.type != MSG_STATS_RESP) return false;

    if (out_stats) {
        *out_stats = resp.telemetry;
    }
    return true;
}

bool client_request_shutdown(client_conn_t *c) {
    if (c->fd < 0) return false;

    broker_msg_t req;
    memset(&req, 0, sizeof(req));
    req.type = MSG_SHUTDOWN;
    req.client_id = c->client_id;

    if (send_msg(c->fd, &req) <= 0) return false;

    broker_msg_t resp;
    if (recv_msg(c->fd, &resp) <= 0 || resp.type != MSG_SHUTDOWN_ACK) return false;

    return true;
}

int reader_process_run(int id, const char *host, int port, int num_ops, bool verbose) {
    client_conn_t conn;
    if (client_connect(&conn, host, port, ROLE_READER, id) != 0) {
        fprintf(stderr, "[Reader %2d (PID %d)] Failed to connect to Broker on %s:%d\n",
                id, getpid(), host ? host : "127.0.0.1", port);
        return 1;
    }

    unsigned int seed = (unsigned int)(time(NULL) ^ (id << 5) ^ getpid());

    for (int i = 0; i < num_ops; i++) {
        int delay = 1000 + (rand_r(&seed) % 4000);
        usleep(delay);

        resource_snapshot_t snapshot;
        if (!client_fetch(&conn, &snapshot)) {
            fprintf(stderr, "[Reader %2d] Fetch failed\n", id);
            break;
        }

        /* Check atomicity invariant: all array elements must be consistent */
        int first_val = snapshot.values[0];
        bool consistent = true;
        for (int k = 1; k < DATA_PAYLOAD_SIZE; k++) {
            if (snapshot.values[k] != first_val) {
                consistent = false;
                break;
            }
        }

        if (!consistent) {
            fprintf(stderr, "[Reader %2d (PID %d)] CRITICAL: Read inconsistent/torn array data!\n",
                    id, getpid());
        }

        if (verbose) {
            char tbuf[32];
            get_formatted_time(tbuf, sizeof(tbuf));
            printf("[%s][Reader %2d (PID %d)] Read value %5d (Version: %d, Consistent: %s)\n",
                   tbuf, id, getpid(), first_val, snapshot.version, consistent ? "YES" : "NO");
            fflush(stdout);
        }
    }

    client_disconnect(&conn);
    return 0;
}

int writer_process_run(int id, const char *host, int port, int num_ops, bool verbose) {
    client_conn_t conn;
    if (client_connect(&conn, host, port, ROLE_WRITER, id) != 0) {
        fprintf(stderr, "[Writer %2d (PID %d)] Failed to connect to Broker on %s:%d\n",
                id, getpid(), host ? host : "127.0.0.1", port);
        return 1;
    }

    unsigned int seed = (unsigned int)(time(NULL) ^ (id << 7) ^ getpid());

    for (int i = 0; i < num_ops; i++) {
        int delay = 1500 + (rand_r(&seed) % 5000);
        usleep(delay);

        int32_t val = (id + 1) * 1000 + i;
        int32_t values[DATA_PAYLOAD_SIZE];
        for (int k = 0; k < DATA_PAYLOAD_SIZE; k++) {
            values[k] = val;
        }

        int32_t new_version = 0;
        if (!client_publish(&conn, values, &new_version)) {
            fprintf(stderr, "[Writer %2d] Publish failed\n", id);
            break;
        }

        if (verbose) {
            char tbuf[32];
            get_formatted_time(tbuf, sizeof(tbuf));
            printf("[%s][Writer %2d (PID %d)] Published value %5d (Committed Version: %d)\n",
                   tbuf, id, getpid(), val, new_version);
            fflush(stdout);
        }
    }

    client_disconnect(&conn);
    return 0;
}
