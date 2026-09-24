#include "buffer.h"
#include <stdlib.h>

int buffer_init(buffer_t *b, int capacity) {
    if (!b || capacity <= 0) {
        return -1;
    }

    b->slots = malloc(sizeof(item_t) * capacity);
    if (!b->slots) {
        return -1;
    }

    b->capacity = capacity;
    b->head = 0;
    b->tail = 0;
    b->count = 0;
    b->shutdown = false;

    if (pthread_mutex_init(&b->lock, NULL) != 0) {
        free(b->slots);
        return -1;
    }

    if (sem_init(&b->empty, 0, capacity) != 0) {
        pthread_mutex_destroy(&b->lock);
        free(b->slots);
        return -1;
    }

    if (sem_init(&b->full, 0, 0) != 0) {
        sem_destroy(&b->empty);
        pthread_mutex_destroy(&b->lock);
        free(b->slots);
        return -1;
    }

    return 0;
}

void buffer_destroy(buffer_t *b) {
    if (!b) return;
    sem_destroy(&b->empty);
    sem_destroy(&b->full);
    pthread_mutex_destroy(&b->lock);
    free(b->slots);
    b->slots = NULL;
}

bool buffer_push(buffer_t *b, item_t item) {
    if (!b) return false;

    if (sem_wait(&b->empty) != 0) {
        return false;
    }

    pthread_mutex_lock(&b->lock);

    if (b->shutdown) {
        pthread_mutex_unlock(&b->lock);
        sem_post(&b->empty);
        return false;
    }

    b->slots[b->tail] = item;
    b->tail = (b->tail + 1) % b->capacity;
    b->count++;

    pthread_mutex_unlock(&b->lock);
    sem_post(&b->full);
    return true;
}

bool buffer_pop(buffer_t *b, item_t *item) {
    if (!b || !item) return false;

    if (sem_wait(&b->full) != 0) {
        return false;
    }

    pthread_mutex_lock(&b->lock);

    if (b->count > 0) {
        *item = b->slots[b->head];
        b->head = (b->head + 1) % b->capacity;
        b->count--;

        pthread_mutex_unlock(&b->lock);
        sem_post(&b->empty);
        return true;
    }

    if (b->shutdown) {
        pthread_mutex_unlock(&b->lock);
        sem_post(&b->full);
        return false;
    }

    pthread_mutex_unlock(&b->lock);
    sem_post(&b->empty);
    return false;
}

void buffer_shutdown(buffer_t *b) {
    if (!b) return;

    pthread_mutex_lock(&b->lock);
    b->shutdown = true;
    pthread_mutex_unlock(&b->lock);

    sem_post(&b->full);
    sem_post(&b->empty);
}

int buffer_get_count(buffer_t *b) {
    if (!b) return 0;
    pthread_mutex_lock(&b->lock);
    int count = b->count;
    pthread_mutex_unlock(&b->lock);
    return count;
}
