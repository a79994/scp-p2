#ifndef BUFFER_H
#define BUFFER_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int value;
    int producer_id;
} item_t;

typedef struct {
    item_t *slots;
    int capacity;
    int head;
    int tail;
    int count;

    sem_t empty;
    sem_t full;
    pthread_mutex_t lock;

    bool shutdown;
} buffer_t;

int buffer_init(buffer_t *b, int capacity);
void buffer_destroy(buffer_t *b);
bool buffer_push(buffer_t *b, item_t item);
bool buffer_pop(buffer_t *b, item_t *item);
void buffer_shutdown(buffer_t *b);
int buffer_get_count(buffer_t *b);

#endif
