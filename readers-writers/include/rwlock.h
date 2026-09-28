#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Starvation-Free Fair (Turnstile) Readers-Writers Lock
 * Based on the two-phase turnstile pattern (Downey, Reek, Kawash)
 */
typedef struct {
    sem_t turnstile;           /* Turnstile for incoming threads; held by waiting writer */
    sem_t room_empty;          /* Resource/room access semaphore (1 = empty, 0 = occupied) */
    pthread_mutex_t read_mutex;/* Mutex protecting readers_count */
    int readers_count;         /* Number of active readers inside the critical section */
} rwlock_t;

/**
 * Shared resource structure with invariant telemetry
 */
typedef struct {
    int value;
    int version;
    int active_readers;
    int active_writers;
    int max_concurrent_readers;
    int total_reads;
    int total_writes;
    int invariant_violations;
    pthread_mutex_t stats_lock;
} shared_resource_t;

/* Lock management functions */
int rwlock_init(rwlock_t *rw);
void rwlock_destroy(rwlock_t *rw);
void rwlock_read_lock(rwlock_t *rw);
void rwlock_read_unlock(rwlock_t *rw);
void rwlock_write_lock(rwlock_t *rw);
void rwlock_write_unlock(rwlock_t *rw);

/* Shared resource access and validation */
void shared_resource_init(shared_resource_t *res, int initial_val);
void shared_resource_destroy(shared_resource_t *res);
int shared_resource_read(shared_resource_t *res, rwlock_t *rw, int *out_version);
void shared_resource_write(shared_resource_t *res, rwlock_t *rw, int new_val);
bool shared_resource_verify_invariants(shared_resource_t *res);

#endif /* RWLOCK_H */
