#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "philosopher.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void random_delay(int min_us, int max_us, unsigned int *seed) {
    if (max_us <= 0) return;
    if (min_us < 0) min_us = 0;
    if (max_us < min_us) max_us = min_us;

    int range = max_us - min_us + 1;
    int duration = min_us + (rand_r(seed) % range);
    if (duration > 0) {
        usleep(duration);
    }
}

void philosopher_set_state(philosopher_t *p, philosopher_state_t new_state) {
    table_t *table = p->table;
    pthread_mutex_lock(&table->state_lock);

    philosopher_state_t old_state = p->state;
    if (old_state != new_state) {
        p->state = new_state;

        if (table->on_state_change) {
            table->on_state_change(table, p->id, old_state, new_state);
        }

        if (table->verbose) {
            const char *state_str = (new_state == STATE_THINKING) ? "THINKING" :
                                    (new_state == STATE_HUNGRY)   ? "HUNGRY  " :
                                                                    "EATING  ";
            printf("[Time %8.2f ms] Philosopher %2d is now %s (meals: %lu)\n",
                   (double)get_time_us() / 1000.0, p->id, state_str, p->meals_eaten);
        }
    }

    pthread_mutex_unlock(&table->state_lock);
}

void *philosopher_routine(void *arg) {
    philosopher_t *p = (philosopher_t *)arg;
    table_t *table = p->table;
    unsigned int seed = (unsigned int)(time(NULL) ^ (p->id << 8) ^ pthread_self());

    while (!table->stop_requested) {
        if (table->max_meals > 0 && p->meals_eaten >= (uint64_t)table->max_meals) {
            break;
        }

        philosopher_set_state(p, STATE_THINKING);
        random_delay(p->think_time_min_us, p->think_time_max_us, &seed);

        if (table->stop_requested) break;
        if (table->max_meals > 0 && p->meals_eaten >= (uint64_t)table->max_meals) {
            break;
        }

        philosopher_set_state(p, STATE_HUNGRY);
        uint64_t t_start = get_time_us();

        fork_acquire(p->first_fork, p->id);
        fork_acquire(p->second_fork, p->id);

        uint64_t t_end = get_time_us();
        p->total_wait_time_us += (t_end - t_start);

        philosopher_set_state(p, STATE_EATING);
        p->meals_eaten++;
        random_delay(p->eat_time_min_us, p->eat_time_max_us, &seed);

        philosopher_set_state(p, STATE_THINKING);

        fork_release(p->second_fork, p->id);
        fork_release(p->first_fork, p->id);
    }

    return NULL;
}

int table_init(table_t *table, int num_philosophers, int max_meals) {
    if (!table || num_philosophers < 2) {
        return -1;
    }

    table->num_philosophers = num_philosophers;
    table->max_meals = max_meals;
    table->stop_requested = false;
    table->on_state_change = NULL;
    table->user_data = NULL;
    table->verbose = false;

    if (pthread_mutex_init(&table->state_lock, NULL) != 0) {
        return -1;
    }

    table->forks = malloc(sizeof(fork_t) * num_philosophers);
    if (!table->forks) {
        pthread_mutex_destroy(&table->state_lock);
        return -1;
    }

    table->philosophers = malloc(sizeof(philosopher_t) * num_philosophers);
    if (!table->philosophers) {
        free(table->forks);
        pthread_mutex_destroy(&table->state_lock);
        return -1;
    }

    for (int i = 0; i < num_philosophers; i++) {
        if (fork_init(&table->forks[i], i) != 0) {
            for (int j = 0; j < i; j++) fork_destroy(&table->forks[j]);
            free(table->forks);
            free(table->philosophers);
            pthread_mutex_destroy(&table->state_lock);
            return -1;
        }
    }

    for (int i = 0; i < num_philosophers; i++) {
        philosopher_t *p = &table->philosophers[i];
        p->id = i;
        p->table = table;
        p->state = STATE_THINKING;
        p->meals_eaten = 0;
        p->total_wait_time_us = 0;

        p->think_time_min_us = 2000;
        p->think_time_max_us = 5000;
        p->eat_time_min_us = 2000;
        p->eat_time_max_us = 5000;

        p->left_fork_id = i;
        p->right_fork_id = (i + 1) % num_philosophers;

        if (i % 2 == 0) {
            p->first_fork = &table->forks[p->left_fork_id];
            p->second_fork = &table->forks[p->right_fork_id];
        } else {
            p->first_fork = &table->forks[p->right_fork_id];
            p->second_fork = &table->forks[p->left_fork_id];
        }
    }

    return 0;
}

int table_start(table_t *table) {
    if (!table) return -1;
    for (int i = 0; i < table->num_philosophers; i++) {
        if (pthread_create(&table->philosophers[i].thread, NULL, philosopher_routine, &table->philosophers[i]) != 0) {
            table->stop_requested = true;
            for (int j = 0; j < i; j++) {
                pthread_join(table->philosophers[j].thread, NULL);
            }
            return -1;
        }
    }
    return 0;
}

void table_wait(table_t *table) {
    if (!table) return;
    for (int i = 0; i < table->num_philosophers; i++) {
        pthread_join(table->philosophers[i].thread, NULL);
    }
}

void table_stop(table_t *table) {
    if (!table) return;
    table->stop_requested = true;
    for (int i = 0; i < table->num_philosophers; i++) {
        pthread_mutex_lock(&table->forks[i].lock);
        pthread_cond_broadcast(&table->forks[i].cond);
        pthread_mutex_unlock(&table->forks[i].lock);
    }
}

void table_destroy(table_t *table) {
    if (!table) return;
    for (int i = 0; i < table->num_philosophers; i++) {
        fork_destroy(&table->forks[i]);
    }
    pthread_mutex_destroy(&table->state_lock);
    free(table->forks);
    free(table->philosophers);
    table->forks = NULL;
    table->philosophers = NULL;
}
