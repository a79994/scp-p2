#include "fork.h"
#include <stdio.h>

int fork_init(fork_t *fork, int id) {
    if (!fork) return -1;
    fork->id = id;
    fork->next_ticket = 0;
    fork->current_ticket = 0;
    fork->holder_id = -1;

    if (pthread_mutex_init(&fork->lock, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&fork->cond, NULL) != 0) {
        pthread_mutex_destroy(&fork->lock);
        return -1;
    }
    return 0;
}

void fork_destroy(fork_t *fork) {
    if (!fork) return;
    pthread_mutex_destroy(&fork->lock);
    pthread_cond_destroy(&fork->cond);
}

void fork_acquire(fork_t *fork, int philosopher_id) {
    if (!fork) return;
    pthread_mutex_lock(&fork->lock);

    uint64_t my_ticket = fork->next_ticket++;

    while (my_ticket != fork->current_ticket) {
        pthread_cond_wait(&fork->cond, &fork->lock);
    }

    fork->holder_id = philosopher_id;
    pthread_mutex_unlock(&fork->lock);
}

void fork_release(fork_t *fork, int philosopher_id) {
    if (!fork) return;
    pthread_mutex_lock(&fork->lock);

    if (fork->holder_id == philosopher_id) {
        fork->holder_id = -1;
        fork->current_ticket++;
        pthread_cond_broadcast(&fork->cond);
    }

    pthread_mutex_unlock(&fork->lock);
}
