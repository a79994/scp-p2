#ifndef FORK_H
#define FORK_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int id;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    uint64_t next_ticket;
    uint64_t current_ticket;
    int holder_id;
} fork_t;

int fork_init(fork_t *fork, int id);
void fork_destroy(fork_t *fork);
void fork_acquire(fork_t *fork, int philosopher_id);
void fork_release(fork_t *fork, int philosopher_id);

#endif
