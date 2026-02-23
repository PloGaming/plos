#ifndef TASK_H
#define TASK_H

#include <interrupts/isr.h>
#include <memory/vmm.h>
#include <memory/paging.h>
#include <stdint.h>

#define TASK_INITIAL_STACK_SIZE (4 * PAGING_PAGE_SIZE)
#define TASK_NAME_MAX_LENGTH 32
#define TASK_INITIAL_TICKS 4

/**
 * @brief Describes the state of a task
 */
enum task_state 
{
    TASK_READY, ///< Ready to be executed
    TASK_RUNNING, ///< Currently executing
    TASK_BLOCKED, ///< Waiting for I/O
    TASK_ZOMBIE ///< Finished, waiting for cleanup
};

/**
 * @brief The fundamental structure of a task
 * It's a node in a linked list
 */
struct task
{
    uint64_t pid; ///< Process ID
    char name[TASK_NAME_MAX_LENGTH]; ///< Name of the process (useful for debug)

    struct cpu_status *kstack_sp; ///< Pointer to the kernel stack of this task

    struct vm_address_space *vas; ///< Its virtual address space

    enum task_state state; ///< Its current state
    uint64_t ticks_remaining;

    struct task *next;
};

struct task *task_create(const char *name, void (*entry_point)(), uint64_t pid);

#endif // TASK_H