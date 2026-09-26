#ifndef FORK_H
#define FORK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int id;
    int holder_id;
} fork_t;

int fork_init(fork_t *fork, int id);
void fork_destroy(fork_t *fork);
bool fork_is_available(const fork_t *fork);
void fork_assign(fork_t *fork, int philosopher_id);
void fork_release_holder(fork_t *fork, int philosopher_id);

#endif