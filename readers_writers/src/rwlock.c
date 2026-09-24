#include "rwlock.h"
#include <stdlib.h>
#include <string.h>

int rwlock_init(rwlock_t *rw) {
    if (!rw) return -1;

    if (sem_init(&rw->turnstile, 0, 1) != 0) {
        return -1;
    }

    if (sem_init(&rw->room_empty, 0, 1) != 0) {
        sem_destroy(&rw->turnstile);
        return -1;
    }

    if (pthread_mutex_init(&rw->read_mutex, NULL) != 0) {
        sem_destroy(&rw->room_empty);
        sem_destroy(&rw->turnstile);
        return -1;
    }

    rw->readers_count = 0;
    return 0;
}

void rwlock_destroy(rwlock_t *rw) {
    if (!rw) return;

    sem_destroy(&rw->turnstile);
    sem_destroy(&rw->room_empty);
    pthread_mutex_destroy(&rw->read_mutex);
}

void rwlock_read_lock(rwlock_t *rw) {
    if (!rw) return;

    /* Pass through turnstile: blocks if a writer is waiting/writing */
    sem_wait(&rw->turnstile);
    sem_post(&rw->turnstile);

    pthread_mutex_lock(&rw->read_mutex);
    rw->readers_count++;
    if (rw->readers_count == 1) {
        /* First reader locks the room from writers */
        sem_wait(&rw->room_empty);
    }
    pthread_mutex_unlock(&rw->read_mutex);
}

void rwlock_read_unlock(rwlock_t *rw) {
    if (!rw) return;

    pthread_mutex_lock(&rw->read_mutex);
    rw->readers_count--;
    if (rw->readers_count == 0) {
        /* Last reader vacates the room, allowing writers to enter */
        sem_post(&rw->room_empty);
    }
    pthread_mutex_unlock(&rw->read_mutex);
}

void rwlock_write_lock(rwlock_t *rw) {
    if (!rw) return;

    /* Hold turnstile to block new readers from entering */
    sem_wait(&rw->turnstile);
    /* Wait until all existing readers have vacated the room */
    sem_wait(&rw->room_empty);
}

void rwlock_write_unlock(rwlock_t *rw) {
    if (!rw) return;

    /* Release the room and open the turnstile for next queued thread */
    sem_post(&rw->turnstile);
    sem_post(&rw->room_empty);
}

void shared_resource_init(shared_resource_t *res, int initial_val) {
    if (!res) return;

    res->value = initial_val;
    res->version = 0;
    res->active_readers = 0;
    res->active_writers = 0;
    res->max_concurrent_readers = 0;
    res->total_reads = 0;
    res->total_writes = 0;
    res->invariant_violations = 0;
    pthread_mutex_init(&res->stats_lock, NULL);
}

void shared_resource_destroy(shared_resource_t *res) {
    if (!res) return;
    pthread_mutex_destroy(&res->stats_lock);
}

int shared_resource_read(shared_resource_t *res, rwlock_t *rw, int *out_version) {
    if (!res || !rw) return 0;

    rwlock_read_lock(rw);

    pthread_mutex_lock(&res->stats_lock);
    res->active_readers++;
    if (res->active_readers > res->max_concurrent_readers) {
        res->max_concurrent_readers = res->active_readers;
    }
    if (res->active_writers > 0) {
        res->invariant_violations++;
    }
    res->total_reads++;
    pthread_mutex_unlock(&res->stats_lock);

    int val = res->value;
    if (out_version) {
        *out_version = res->version;
    }

    pthread_mutex_lock(&res->stats_lock);
    res->active_readers--;
    pthread_mutex_unlock(&res->stats_lock);

    rwlock_read_unlock(rw);
    return val;
}

void shared_resource_write(shared_resource_t *res, rwlock_t *rw, int new_val) {
    if (!res || !rw) return;

    rwlock_write_lock(rw);

    pthread_mutex_lock(&res->stats_lock);
    res->active_writers++;
    if (res->active_writers > 1 || res->active_readers > 0) {
        res->invariant_violations++;
    }
    res->total_writes++;
    pthread_mutex_unlock(&res->stats_lock);

    res->value = new_val;
    res->version++;

    pthread_mutex_lock(&res->stats_lock);
    res->active_writers--;
    pthread_mutex_unlock(&res->stats_lock);

    rwlock_write_unlock(rw);
}

bool shared_resource_verify_invariants(shared_resource_t *res) {
    if (!res) return false;

    pthread_mutex_lock(&res->stats_lock);
    bool ok = (res->invariant_violations == 0) &&
              (res->active_readers == 0) &&
              (res->active_writers == 0);
    pthread_mutex_unlock(&res->stats_lock);
    return ok;
}
