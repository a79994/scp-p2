#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "philosopher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>

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

static ssize_t send_msg(int fd, const ipc_msg_t *msg) {
    size_t total = 0;
    const char *buf = (const char *)msg;
    while (total < sizeof(ipc_msg_t)) {
        ssize_t n = write(fd, buf + total, sizeof(ipc_msg_t) - total);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

static ssize_t recv_msg(int fd, ipc_msg_t *msg) {
    size_t total = 0;
    char *buf = (char *)msg;
    while (total < sizeof(ipc_msg_t)) {
        ssize_t n = read(fd, buf + total, sizeof(ipc_msg_t) - total);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return (n == 0 && total == 0) ? 0 : -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

void table_update_state(table_t *table, int philosopher_id, philosopher_state_t new_state) {
    philosopher_t *p = &table->philosophers[philosopher_id];
    philosopher_state_t old_state = p->state;
    if (old_state != new_state) {
        p->state = new_state;

        if (table->on_state_change) {
            table->on_state_change(table, philosopher_id, old_state, new_state);
        }

        if (table->verbose) {
            const char *state_str = (new_state == STATE_THINKING) ? "THINKING" :
                                    (new_state == STATE_HUNGRY)   ? "HUNGRY  " :
                                                                    "EATING  ";
                                                                    
            printf("[Time %8.2f ms] Philosopher %2d is now %s (meals: %lu)\n",
                   (double)get_time_us() / 1000.0, philosopher_id, state_str, p->meals_eaten);
        }
    }
}

static void try_grant_forks(table_t *table) {
    int n = table->num_philosophers;

    /* Check each hungry philosopher in order of arrival ticket (Fair FIFO) */
    bool granted_any = true;
    while (granted_any) {
        granted_any = false;
        int best_phil = -1;
        uint64_t lowest_ticket = UINT64_MAX;

        /* Find the hungry philosopher with the lowest ticket that can be served */
        for (int i = 0; i < n; i++) {
            philosopher_t *p = &table->philosophers[i];
            if (!p->is_hungry) continue;

            int left = p->left_fork_id;
            int right = p->right_fork_id;

            if (fork_is_available(&table->forks[left]) && fork_is_available(&table->forks[right])) {
                /* Check if any adjacent neighbor has an earlier request for either fork */
                int left_nbr = (i - 1 + n) % n;
                int right_nbr = (i + 1) % n;

                bool nbr_has_priority = false;
                if (table->philosophers[left_nbr].is_hungry &&
                    table->philosophers[left_nbr].request_ticket < p->request_ticket) {
                    nbr_has_priority = true;
                }
                if (table->philosophers[right_nbr].is_hungry &&
                    table->philosophers[right_nbr].request_ticket < p->request_ticket) {
                    nbr_has_priority = true;
                }

                if (!nbr_has_priority && p->request_ticket < lowest_ticket) {
                    lowest_ticket = p->request_ticket;
                    best_phil = i;
                }
            }
        }

        if (best_phil >= 0) {
            philosopher_t *p = &table->philosophers[best_phil];
            int left = p->left_fork_id;
            int right = p->right_fork_id;

            fork_assign(&table->forks[left], best_phil);
            fork_assign(&table->forks[right], best_phil);

            p->is_hungry = false;

            ipc_msg_t grant = {
                .type = MSG_GRANT_FORKS,
                .phil_id = best_phil,
                .wait_time_us = 0
            };
            send_msg(p->coordinator_sock, &grant);
            table_update_state(table, best_phil, STATE_EATING);
            granted_any = true;
        }
    }
}

static void philosopher_process_main(philosopher_t *p, int max_meals) {
    signal(SIGINT, SIG_IGN);
    close(p->coordinator_sock);
    p->coordinator_sock = -1;

    unsigned int seed = (unsigned int)(time(NULL) ^ (p->id << 8) ^ getpid());
    uint64_t meals = 0;

    while (max_meals == 0 || meals < (uint64_t)max_meals) {
        /* Thinking phase */
        random_delay(p->think_time_min_us, p->think_time_max_us, &seed);

        /* Request forks (Hungry) */
        uint64_t t_start = get_time_us();
        ipc_msg_t req = {
            .type = MSG_REQ_FORKS,
            .phil_id = p->id,
            .wait_time_us = 0
        };
        if (send_msg(p->child_sock, &req) <= 0) break;

        /* Wait for coordinator to grant forks */
        ipc_msg_t resp;
        if (recv_msg(p->child_sock, &resp) <= 0) break;
        if (resp.type == MSG_TERMINATE) break;

        uint64_t wait_us = get_time_us() - t_start;

        /* Eating phase */
        meals++;
        random_delay(p->eat_time_min_us, p->eat_time_max_us, &seed);

        /* Release forks */
        ipc_msg_t rel = {
            .type = MSG_REL_FORKS,
            .phil_id = p->id,
            .wait_time_us = wait_us
        };
        if (send_msg(p->child_sock, &rel) <= 0) break;
    }

    ipc_msg_t done = {
        .type = MSG_DONE,
        .phil_id = p->id,
        .wait_time_us = 0
    };
    send_msg(p->child_sock, &done);

    close(p->child_sock);
    p->child_sock = -1;
    exit(0);
}

int table_init(table_t *table, int num_philosophers, int max_meals) {
    if (!table || num_philosophers < 2) {
        return -1;
    }

    table->num_philosophers = num_philosophers;
    table->max_meals = max_meals;
    table->duration_sec = 0;
    table->stop_requested = false;
    table->next_ticket = 0;
    table->on_state_change = NULL;
    table->stop_check = NULL;
    table->user_data = NULL;
    table->verbose = false;

    table->forks = malloc(sizeof(fork_t) * num_philosophers);
    if (!table->forks) return -1;

    table->philosophers = malloc(sizeof(philosopher_t) * num_philosophers);
    if (!table->philosophers) {
        free(table->forks);
        return -1;
    }

    for (int i = 0; i < num_philosophers; i++) {
        fork_init(&table->forks[i], i);
    }

    for (int i = 0; i < num_philosophers; i++) {
        philosopher_t *p = &table->philosophers[i];
        p->id = i;
        p->pid = -1;
        p->coordinator_sock = -1;
        p->child_sock = -1;
        p->state = STATE_THINKING;
        p->is_hungry = false;
        p->request_ticket = 0;
        p->meals_eaten = 0;
        p->total_wait_time_us = 0;

        p->think_time_min_us = 2000;
        p->think_time_max_us = 5000;
        p->eat_time_min_us = 2000;
        p->eat_time_max_us = 5000;

        p->left_fork_id = i;
        p->right_fork_id = (i + 1) % num_philosophers;

        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            for (int j = 0; j < i; j++) {
                close(table->philosophers[j].coordinator_sock);
                close(table->philosophers[j].child_sock);
            }
            free(table->forks);
            free(table->philosophers);
            return -1;
        }
        p->coordinator_sock = sv[0];
        p->child_sock = sv[1];
    }

    return 0;
}

int table_start(table_t *table) {
    if (!table) return -1;

    for (int i = 0; i < table->num_philosophers; i++) {
        philosopher_t *p = &table->philosophers[i];
        pid_t pid = fork();
        if (pid < 0) {
            table_stop(table);
            return -1;
        }

        if (pid == 0) {
            /* Child process */
            /* Close all other child and coordinator sockets */
            for (int j = 0; j < table->num_philosophers; j++) {
                if (j != i) {
                    if (table->philosophers[j].child_sock >= 0) {
                        close(table->philosophers[j].child_sock);
                    }
                }
                if (table->philosophers[j].coordinator_sock >= 0) {
                    close(table->philosophers[j].coordinator_sock);
                }
            }
            philosopher_process_main(p, table->max_meals);
            /* Unreachable */
            exit(0);
        } else {
            /* Parent process */
            p->pid = pid;
            close(p->child_sock);
            p->child_sock = -1;
        }
    }

    return 0;
}

void table_wait(table_t *table) {
    if (!table) return;

    int n = table->num_philosophers;
    struct pollfd *pfds = malloc(sizeof(struct pollfd) * n);
    if (!pfds) return;

    bool *active = malloc(sizeof(bool) * n);
    if (!active) {
        free(pfds);
        return;
    }

    for (int i = 0; i < n; i++) {
        active[i] = true;
    }

    uint64_t start_time = get_time_us();
    uint64_t duration_limit_us = (table->duration_sec > 0) ? 
        ((uint64_t)table->duration_sec * 1000000ULL) : 0;

    while (!table->stop_requested) {
        int active_count = 0;
        for (int i = 0; i < n; i++) {
            if (active[i] && table->philosophers[i].coordinator_sock >= 0) {
                pfds[i].fd = table->philosophers[i].coordinator_sock;
                pfds[i].events = POLLIN;
                pfds[i].revents = 0;
                active_count++;
            } else {
                pfds[i].fd = -1;
                pfds[i].events = 0;
                pfds[i].revents = 0;
            }
        }

        if (active_count == 0) {
            break;
        }

        /* Calculate poll timeout based on duration */
        int timeout_ms = -1;
        if (duration_limit_us > 0) {
            uint64_t elapsed = get_time_us() - start_time;
            if (elapsed >= duration_limit_us) {
                table->stop_requested = true;
                break;
            }
            uint64_t remaining_us = duration_limit_us - elapsed;
            timeout_ms = (int)(remaining_us / 1000ULL);
            if (timeout_ms <= 0) timeout_ms = 1;
        }

        int ret = poll(pfds, n, timeout_ms);
        if (ret < 0) {
            if (errno == EINTR) {
                if (table->stop_requested) break;
                continue;
            }
            break;
        }

        if (ret == 0 && duration_limit_us > 0) {
            table->stop_requested = true;
            break;
        }

        for (int i = 0; i < n; i++) {
            if (pfds[i].fd < 0) continue;

            if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                ipc_msg_t msg;
                ssize_t nbytes = recv_msg(pfds[i].fd, &msg);
                if (nbytes <= 0) {
                    /* Socket closed by child */
                    close(table->philosophers[i].coordinator_sock);
                    table->philosophers[i].coordinator_sock = -1;
                    active[i] = false;
                    table->philosophers[i].is_hungry = false;
                    fork_release_holder(&table->forks[table->philosophers[i].left_fork_id], i);
                    fork_release_holder(&table->forks[table->philosophers[i].right_fork_id], i);
                    try_grant_forks(table);
                    continue;
                }

                if (msg.type == MSG_REQ_FORKS) {
                    table->philosophers[i].is_hungry = true;
                    table->philosophers[i].request_ticket = ++table->next_ticket;
                    table_update_state(table, i, STATE_HUNGRY);
                    try_grant_forks(table);
                } else if (msg.type == MSG_REL_FORKS) {
                    fork_release_holder(&table->forks[table->philosophers[i].left_fork_id], i);
                    fork_release_holder(&table->forks[table->philosophers[i].right_fork_id], i);
                    table->philosophers[i].meals_eaten++;
                    table->philosophers[i].total_wait_time_us += msg.wait_time_us;
                    table_update_state(table, i, STATE_THINKING);

                    if (table->stop_check && table->stop_check(table)) {
                        table->stop_requested = true;
                    }

                    try_grant_forks(table);
                } else if (msg.type == MSG_DONE) {
                    close(table->philosophers[i].coordinator_sock);
                    table->philosophers[i].coordinator_sock = -1;
                    active[i] = false;
                    table->philosophers[i].is_hungry = false;
                    try_grant_forks(table);
                }
            }
        }
    }

    /* Terminate any remaining children */
    ipc_msg_t term = { .type = MSG_TERMINATE, .phil_id = -1, .wait_time_us = 0 };
    for (int i = 0; i < n; i++) {
        if (table->philosophers[i].coordinator_sock >= 0) {
            send_msg(table->philosophers[i].coordinator_sock, &term);
            close(table->philosophers[i].coordinator_sock);
            table->philosophers[i].coordinator_sock = -1;
        }
    }

    for (int i = 0; i < n; i++) {
        if (table->philosophers[i].pid > 0) {
            waitpid(table->philosophers[i].pid, NULL, 0);
            table->philosophers[i].pid = -1;
        }
    }

    free(pfds);
    free(active);
}

void table_stop(table_t *table) {
    if (!table) return;
    table->stop_requested = true;
}

void table_destroy(table_t *table) {
    if (!table) return;

    for (int i = 0; i < table->num_philosophers; i++) {
        if (table->philosophers[i].coordinator_sock >= 0) {
            close(table->philosophers[i].coordinator_sock);
            table->philosophers[i].coordinator_sock = -1;
        }
        if (table->philosophers[i].child_sock >= 0) {
            close(table->philosophers[i].child_sock);
            table->philosophers[i].child_sock = -1;
        }
        fork_destroy(&table->forks[i]);
    }

    free(table->forks);
    free(table->philosophers);
    table->forks = NULL;
    table->philosophers = NULL;
}
