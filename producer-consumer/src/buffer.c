#define _GNU_SOURCE
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>

int buffer_init(buffer_t *b, int capacity) {
    if (!b || capacity <= 0) {
        return -1;
    }

    b->capacity = capacity;
    b->shutdown = false;
    b->items_pipe[0] = -1;
    b->items_pipe[1] = -1;
    b->slots_pipe[0] = -1;
    b->slots_pipe[1] = -1;

    signal(SIGPIPE, SIG_IGN);

    if (pipe(b->items_pipe) != 0) {
        return -1;
    }

    if (pipe(b->slots_pipe) != 0) {
        close(b->items_pipe[0]);
        close(b->items_pipe[1]);
        b->items_pipe[0] = -1;
        b->items_pipe[1] = -1;
        return -1;
    }

    int req_slots_bytes = capacity;
    if (req_slots_bytes > 65536) {
        fcntl(b->slots_pipe[1], F_SETPIPE_SZ, req_slots_bytes * 2);
    }

    int req_items_bytes = capacity * (int)sizeof(item_t);
    if (req_items_bytes > 65536) {
        fcntl(b->items_pipe[1], F_SETPIPE_SZ, req_items_bytes * 2);
    }

    char token = 'T';
    for (int i = 0; i < capacity; i++) {
        ssize_t w = write(b->slots_pipe[1], &token, 1);
        if (w != 1) {
            buffer_destroy(b);
            return -1;
        }
    }

    return 0;
}

void buffer_close_producer_unused(buffer_t *b) {
    if (!b) return;

    if (b->items_pipe[0] >= 0) {
        close(b->items_pipe[0]);
        b->items_pipe[0] = -1;
    }
    if (b->slots_pipe[1] >= 0) {
        close(b->slots_pipe[1]);
        b->slots_pipe[1] = -1;
    }
}

void buffer_close_consumer_unused(buffer_t *b) {
    if (!b) return;
    if (b->items_pipe[1] >= 0) {
        close(b->items_pipe[1]);
        b->items_pipe[1] = -1;
    }
    if (b->slots_pipe[0] >= 0) {
        close(b->slots_pipe[0]);
        b->slots_pipe[0] = -1;
    }
}

void buffer_destroy(buffer_t *b) {
    if (!b) return;

    if (b->items_pipe[0] >= 0) { close(b->items_pipe[0]); b->items_pipe[0] = -1; }
    if (b->items_pipe[1] >= 0) { close(b->items_pipe[1]); b->items_pipe[1] = -1; }
    if (b->slots_pipe[0] >= 0) { close(b->slots_pipe[0]); b->slots_pipe[0] = -1; }
    if (b->slots_pipe[1] >= 0) { close(b->slots_pipe[1]); b->slots_pipe[1] = -1; }
}

bool buffer_push(buffer_t *b, item_t item) {
    if (!b || b->items_pipe[1] < 0 || b->slots_pipe[0] < 0) {
        return false;
    }

    char token;
    ssize_t r;
    while ((r = read(b->slots_pipe[0], &token, 1)) <= 0) {
        if (r < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }

    ssize_t written = 0;
    const char *buf = (const char *)&item;
    while (written < (ssize_t)sizeof(item_t)) {
        ssize_t w = write(b->items_pipe[1], buf + written, sizeof(item_t) - written);
        if (w <= 0) {
            if (w < 0 && errno == EINTR) {
                continue;
            }
            if (b->slots_pipe[1] >= 0) {
                (void)write(b->slots_pipe[1], &token, 1);
            }
            return false;
        }
        written += w;
    }

    return true;
}

bool buffer_pop(buffer_t *b, item_t *item) {
    if (!b || !item || b->items_pipe[0] < 0) {
        return false;
    }

    item_t tmp;
    ssize_t total = 0;
    char *buf = (char *)&tmp;
    while (total < (ssize_t)sizeof(item_t)) {
        ssize_t r = read(b->items_pipe[0], buf + total, sizeof(item_t) - total);
        if (r == 0) {
            return false;
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        total += r;
    }

    *item = tmp;

    if (b->slots_pipe[1] >= 0) {
        char token = 'T';
        while (write(b->slots_pipe[1], &token, 1) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
    }

    return true;
}

void buffer_shutdown(buffer_t *b) {
    if (!b) return;
    b->shutdown = true;

    if (b->items_pipe[1] >= 0) {
        close(b->items_pipe[1]);
        b->items_pipe[1] = -1;
    }

    if (b->slots_pipe[1] >= 0) {
        close(b->slots_pipe[1]);
        b->slots_pipe[1] = -1;
    }
}

int buffer_get_count(buffer_t *b) {
    if (!b) return 0;
    int fd = (b->items_pipe[0] >= 0) ? b->items_pipe[0] : b->items_pipe[1];
    if (fd < 0) return 0;

    int bytes = 0;
    if (ioctl(fd, FIONREAD, &bytes) == 0) {
        return bytes / (int)sizeof(item_t);
    }
    return 0;
}
