#ifndef BARBERSHOP_H
#define BARBERSHOP_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

/**
 * Downey's Multi-Barber Queue Rendezvous & Private Semaphore System
 * Based on the private semaphore and pass-the-baton patterns (Reek, Downey)
 */
typedef struct {
    int num_barbers;           /* Number of barbers / cutting chairs (K) */
    int num_waiting_chairs;    /* Capacity of waiting room chairs (M) */
    int waiting_customers;     /* Current number of customers in waiting room */
    bool shutdown;             /* Shutdown signal flag */

    pthread_mutex_t lock;      /* Mutex guarding waiting room and internal state */
    sem_t customers_waiting;   /* Counting semaphore tracking waiting customers */

    /* FIFO queue of idle barbers ready to take customers */
    int *idle_barbers_queue;
    int idle_head;
    int idle_tail;
    int idle_count;
    sem_t idle_barbers_sem;    /* Counting semaphore for available idle barbers */

    /* Private semaphores per barber chair for double-handshake rendezvous */
    sem_t *customer_seated;    /* Customer signals barber when seated in chair k */
    sem_t *haircut_done;       /* Barber signals customer when haircut is complete */
    sem_t *customer_left;      /* Customer signals barber when leaving chair k */

    /* Telemetry and invariants tracking */
    int *barber_served_count;  /* Number of haircuts performed by each barber */
    int *chair_occupant;       /* Customer ID in chair k (-1 if vacant) */
    int total_served;          /* Total customers successfully served */
    int total_balked;          /* Total customers that balked (turned away) */
    int max_observed_waiting;  /* High watermark of waiting room occupancy */
    int invariant_violations;  /* Invariant error counter */
} barbershop_t;

/* Lifecycle functions */
int barbershop_init(barbershop_t *shop, int num_barbers, int num_waiting_chairs);
void barbershop_destroy(barbershop_t *shop);
void barbershop_shutdown(barbershop_t *shop);

/* Barber actions */
int barbershop_barber_wait_for_customer(barbershop_t *shop, int barber_id);
void barbershop_barber_finish_haircut(barbershop_t *shop, int barber_id);

/* Customer actions */
bool barbershop_customer_arrive(barbershop_t *shop, int customer_id, int *assigned_barber_id);

/* Invariant verification */
bool barbershop_verify_invariants(barbershop_t *shop, int expected_total_customers);

#endif /* BARBERSHOP_H */
