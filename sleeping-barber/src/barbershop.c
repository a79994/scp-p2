#include "barbershop.h"
#include <stdlib.h>
#include <string.h>

int barbershop_init(barbershop_t *shop, int num_barbers, int num_waiting_chairs) {
    if (!shop || num_barbers < 1 || num_waiting_chairs < 0) {
        return -1;
    }

    shop->num_barbers = num_barbers;
    shop->num_waiting_chairs = num_waiting_chairs;
    shop->waiting_customers = 0;
    shop->shutdown = false;
    shop->total_served = 0;
    shop->total_balked = 0;
    shop->max_observed_waiting = 0;
    shop->invariant_violations = 0;

    shop->idle_barbers_queue = malloc(sizeof(int) * num_barbers);
    shop->barber_served_count = calloc(num_barbers, sizeof(int));
    shop->chair_occupant = malloc(sizeof(int) * num_barbers);
    shop->customer_seated = malloc(sizeof(sem_t) * num_barbers);
    shop->haircut_done = malloc(sizeof(sem_t) * num_barbers);
    shop->customer_left = malloc(sizeof(sem_t) * num_barbers);

    if (!shop->idle_barbers_queue || !shop->barber_served_count ||
        !shop->chair_occupant || !shop->customer_seated ||
        !shop->haircut_done || !shop->customer_left) {
        barbershop_destroy(shop);
        return -1;
    }

    shop->idle_head = 0;
    shop->idle_tail = 0;
    shop->idle_count = 0;

    for (int i = 0; i < num_barbers; i++) {
        shop->chair_occupant[i] = -1;
        sem_init(&shop->customer_seated[i], 0, 0);
        sem_init(&shop->haircut_done[i], 0, 0);
        sem_init(&shop->customer_left[i], 0, 0);
    }

    pthread_mutex_init(&shop->lock, NULL);
    sem_init(&shop->customers_waiting, 0, 0);
    sem_init(&shop->idle_barbers_sem, 0, 0);

    return 0;
}

void barbershop_destroy(barbershop_t *shop) {
    if (!shop) return;

    if (shop->customer_seated) {
        for (int i = 0; i < shop->num_barbers; i++) {
            sem_destroy(&shop->customer_seated[i]);
            sem_destroy(&shop->haircut_done[i]);
            sem_destroy(&shop->customer_left[i]);
        }
        free(shop->customer_seated);
        free(shop->haircut_done);
        free(shop->customer_left);
    }

    pthread_mutex_destroy(&shop->lock);
    sem_destroy(&shop->customers_waiting);
    sem_destroy(&shop->idle_barbers_sem);

    free(shop->idle_barbers_queue);
    free(shop->barber_served_count);
    free(shop->chair_occupant);

    shop->idle_barbers_queue = NULL;
    shop->barber_served_count = NULL;
    shop->chair_occupant = NULL;
    shop->customer_seated = NULL;
    shop->haircut_done = NULL;
    shop->customer_left = NULL;
}

void barbershop_shutdown(barbershop_t *shop) {
    if (!shop) return;

    pthread_mutex_lock(&shop->lock);
    shop->shutdown = true;

    /* Wake up any barbers waiting for customers */
    for (int i = 0; i < shop->num_barbers; i++) {
        if (shop->chair_occupant[i] == -1) {
            sem_post(&shop->customer_seated[i]);
        }
    }
    pthread_mutex_unlock(&shop->lock);
}

int barbershop_barber_wait_for_customer(barbershop_t *shop, int barber_id) {
    if (!shop || barber_id < 0 || barber_id >= shop->num_barbers) {
        return -1;
    }

    /* Enqueue this barber as ready / idle */
    pthread_mutex_lock(&shop->lock);
    if (shop->shutdown) {
        pthread_mutex_unlock(&shop->lock);
        return -1;
    }

    shop->idle_barbers_queue[shop->idle_tail] = barber_id;
    shop->idle_tail = (shop->idle_tail + 1) % shop->num_barbers;
    shop->idle_count++;
    pthread_mutex_unlock(&shop->lock);

    /* Signal that an idle barber chair is ready */
    sem_post(&shop->idle_barbers_sem);

    /* Wait for a customer to be assigned and sit in this barber's chair */
    sem_wait(&shop->customer_seated[barber_id]);

    pthread_mutex_lock(&shop->lock);
    if (shop->shutdown && shop->chair_occupant[barber_id] == -1) {
        pthread_mutex_unlock(&shop->lock);
        return -1;
    }

    int customer_id = shop->chair_occupant[barber_id];
    pthread_mutex_unlock(&shop->lock);

    return customer_id;
}

void barbershop_barber_finish_haircut(barbershop_t *shop, int barber_id) {
    if (!shop || barber_id < 0 || barber_id >= shop->num_barbers) {
        return;
    }

    /* Signal customer that the haircut is complete */
    sem_post(&shop->haircut_done[barber_id]);

    /* Wait for customer to stand up and vacate chair */
    sem_wait(&shop->customer_left[barber_id]);

    pthread_mutex_lock(&shop->lock);
    shop->chair_occupant[barber_id] = -1;
    shop->barber_served_count[barber_id]++;
    shop->total_served++;
    pthread_mutex_unlock(&shop->lock);
}

bool barbershop_customer_arrive(barbershop_t *shop, int customer_id, int *assigned_barber_id) {
    if (!shop) return false;

    pthread_mutex_lock(&shop->lock);

    /* Check waiting capacity: if waiting room is full and no barber is immediately idle, balk */
    if (shop->waiting_customers >= shop->num_waiting_chairs && shop->idle_count == 0) {
        shop->total_balked++;
        pthread_mutex_unlock(&shop->lock);
        return false; /* Customer balks and leaves */
    }

    shop->waiting_customers++;
    if (shop->waiting_customers > shop->max_observed_waiting) {
        shop->max_observed_waiting = shop->waiting_customers;
    }
    pthread_mutex_unlock(&shop->lock);

    /* Wait for an available idle barber */
    sem_wait(&shop->idle_barbers_sem);

    /* Claim the next available barber from FIFO queue */
    pthread_mutex_lock(&shop->lock);
    shop->waiting_customers--;

    int barber_id = shop->idle_barbers_queue[shop->idle_head];
    shop->idle_head = (shop->idle_head + 1) % shop->num_barbers;
    shop->idle_count--;

    /* Invariant assertion: chair must be strictly vacant */
    if (shop->chair_occupant[barber_id] != -1) {
        shop->invariant_violations++;
    }
    shop->chair_occupant[barber_id] = customer_id;
    pthread_mutex_unlock(&shop->lock);

    if (assigned_barber_id) {
        *assigned_barber_id = barber_id;
    }

    /* Double-handshake rendezvous: Sit down in chair */
    sem_post(&shop->customer_seated[barber_id]);

    /* Wait for haircut to be finished */
    sem_wait(&shop->haircut_done[barber_id]);

    /* Vacate chair */
    sem_post(&shop->customer_left[barber_id]);

    return true; /* Successfully received haircut */
}

bool barbershop_verify_invariants(barbershop_t *shop, int expected_total_customers) {
    if (!shop) return false;

    pthread_mutex_lock(&shop->lock);

    int sum_served_by_barbers = 0;
    for (int i = 0; i < shop->num_barbers; i++) {
        sum_served_by_barbers += shop->barber_served_count[i];
    }

    bool ok = (shop->invariant_violations == 0) &&
              (sum_served_by_barbers == shop->total_served) &&
              (shop->total_served + shop->total_balked == expected_total_customers) &&
              (shop->waiting_customers == 0);

    pthread_mutex_unlock(&shop->lock);
    return ok;
}
