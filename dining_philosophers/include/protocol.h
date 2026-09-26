#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum {
    MSG_REQ_FORKS   = 1,   /* Philosopher requests forks to eat */
    MSG_GRANT_FORKS = 2,   /* Coordinator grants forks to philosopher */
    MSG_REL_FORKS   = 3,   /* Philosopher releases forks after eating */
    MSG_TERMINATE   = 4,   /* Coordinator instructs philosopher to terminate */
    MSG_DONE        = 5    /* Philosopher informs coordinator it has finished */
} msg_type_t;

typedef struct {
    int32_t type;          /* msg_type_t */
    int32_t phil_id;       /* ID of the philosopher */
    uint64_t wait_time_us; /* Elapsed wait time reported when releasing forks */
} ipc_msg_t;

#endif
