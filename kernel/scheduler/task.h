#ifndef TASK_H
#define TASK_H

#include "../core/kernel.h"

typedef enum { 
    TASK_RUNNING, 
    TASK_READY, 
    TASK_DEAD 
} task_state_t;

typedef struct task {
    u64             rsp;
    u64             stack_base;
    u32             pid;
    task_state_t    state;
    struct task    *next;
} task_t;

void    scheduler_init(void);
task_t *task_create(void (*entry)(void));
void    yield(void);
void    task_exit(void);

/* Called automatically by the hardware timer */
void    scheduler_tick(void);

#endif