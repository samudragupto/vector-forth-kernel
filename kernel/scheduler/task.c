#include "task.h"
#include "../memory/heap.h"

#define TASK_STACK_SIZE 8192

extern void switch_context(u64 *old_rsp, u64 new_rsp);

static task_t *current_task = NULL;
static task_t *task_list = NULL;
static u32 next_pid = 1;

void scheduler_init(void) {
    task_t *main_task = kmalloc(sizeof(task_t));
    main_task->pid = 0;
    main_task->state = TASK_RUNNING;
    main_task->next = main_task;
    
    current_task = main_task;
    task_list = main_task;
}

void task_exit(void) {
    current_task->state = TASK_DEAD;
    yield();
    while(1) hlt();
}

task_t *task_create(void (*entry)(void)) {
    task_t *t = kmalloc(sizeof(task_t));
    t->pid = next_pid++;
    t->stack_base = (u64)kmalloc(TASK_STACK_SIZE);
    t->state = TASK_READY;

    u64 *stack = (u64 *)(t->stack_base + TASK_STACK_SIZE);
    
    *(--stack) = (u64)task_exit; /* Return address */
    *(--stack) = (u64)entry;     /* RIP */
    *(--stack) = 0x202;          /* RFLAGS (Interrupts Enabled) */
    *(--stack) = 0;              /* RBP */
    *(--stack) = 0;              /* RBX */
    *(--stack) = 0;              /* R12 */
    *(--stack) = 0;              /* R13 */
    *(--stack) = 0;              /* R14 */
    *(--stack) = 0;              /* R15 */
    
    t->rsp = (u64)stack;

    task_t *last = task_list;
    while(last->next != task_list) {
        last = last->next;
    }
    last->next = t;
    t->next = task_list;

    return t;
}

void yield(void) {
    if (!current_task) return;

    task_t *old = current_task;
    task_t *next = current_task->next;
    
    while(next->state != TASK_READY && next != current_task) {
        if (next->state == TASK_RUNNING) break;
        next = next->next;
    }
    
    if (next != current_task) {
        if (old->state == TASK_RUNNING) old->state = TASK_READY;
        next->state = TASK_RUNNING;
        current_task = next;
        
        switch_context(&old->rsp, next->rsp);
    }
}