#include "fork.h"
#include <stdio.h>

int fork_init(fork_t *fork, int id) {
    if (!fork) return -1;
    fork->id = id;
    fork->holder_id = -1;
    return 0;
}

void fork_destroy(fork_t *fork) {
    if (!fork) return;
    fork->holder_id = -1;
}

bool fork_is_available(const fork_t *fork) {
    if (!fork) return false;
    return fork->holder_id == -1;
}

void fork_assign(fork_t *fork, int philosopher_id) {
    if (!fork) return;
    fork->holder_id = philosopher_id;
}

void fork_release_holder(fork_t *fork, int philosopher_id) {
    if (!fork) return;
    if (fork->holder_id == philosopher_id) {
        fork->holder_id = -1;
    }
}
