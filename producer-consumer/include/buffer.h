#ifndef BUFFER_H
#define BUFFER_H

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    int value;
    int producer_id;
} item_t;

typedef struct {
    int capacity;
    int items_pipe[2]; /* [0] = read end (data retrieval), [1] = write end (data insertion) */
    int slots_pipe[2]; /* [0] = read end (acquire empty slot), [1] = write end (release empty slot) */
    bool shutdown;
} buffer_t;

int buffer_init(buffer_t *b, int capacity);
void buffer_destroy(buffer_t *b);
bool buffer_push(buffer_t *b, item_t item);
bool buffer_pop(buffer_t *b, item_t *item);
void buffer_shutdown(buffer_t *b);
int buffer_get_count(buffer_t *b);

/* Helper functions to close unused pipe directions in child processes */
void buffer_close_producer_unused(buffer_t *b);
void buffer_close_consumer_unused(buffer_t *b);

#endif
