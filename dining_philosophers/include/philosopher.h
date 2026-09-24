#ifndef PHILOSOPHER_H
#define PHILOSOPHER_H

#include "fork.h"
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_THINKING = 0,
    STATE_HUNGRY   = 1,
    STATE_EATING   = 2
} philosopher_state_t;

typedef struct table_s table_t;

typedef struct {
    int id;
    table_t *table;
    pthread_t thread;
    philosopher_state_t state;

    int left_fork_id;
    int right_fork_id;

    fork_t *first_fork;
    fork_t *second_fork;

    uint64_t meals_eaten;
    uint64_t total_wait_time_us;

    int think_time_min_us;
    int think_time_max_us;
    int eat_time_min_us;
    int eat_time_max_us;
} philosopher_t;

typedef void (*state_change_cb)(table_t *table, int philosopher_id, philosopher_state_t old_state, philosopher_state_t new_state);

struct table_s {
    int num_philosophers;
    fork_t *forks;
    philosopher_t *philosophers;

    volatile bool stop_requested;
    int max_meals;

    pthread_mutex_t state_lock;
    state_change_cb on_state_change;
    void *user_data;
    bool verbose;
};

int table_init(table_t *table, int num_philosophers, int max_meals);
void table_destroy(table_t *table);
int table_start(table_t *table);
void table_wait(table_t *table);
void table_stop(table_t *table);
void philosopher_set_state(philosopher_t *p, philosopher_state_t new_state);
void *philosopher_routine(void *arg);
uint64_t get_time_us(void);

#endif
