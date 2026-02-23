#ifndef TASK_H
#define TASK_H

#include <interrupts/isr.h>
#include <memory/vmm.h>
#include <memory/paging.h>
#include <stdint.h>

#define THREAD_INITIAL_STACK_SIZE (4 * PAGING_PAGE_SIZE)
#define TASK_NAME_MAX_LENGTH    32
#define THREAD_INITIAL_TICKS 4

/**
 * @brief Describes the state of a thread
 */
enum thread_state 
{
    THREAD_READY, ///< Ready to be executed
    THREAD_RUNNING, ///< Currently executing
    THREAD_BLOCKED, ///< Waiting for I/O
    THREAD_ZOMBIE ///< Finished, waiting for cleanup
};

/**
 * @brief The fundamental structure of a task
 * It's a node in a circular linked list
 */
struct task
{
    uint64_t pid; ///< Process ID
    char name[TASK_NAME_MAX_LENGTH]; ///< Name of the process (useful for debug)

    struct vm_address_space *vas; ///< Its virtual address space
    struct thread *threads; ///< List of threads associated with that 

    struct task *next; ///< Pointer to the next task
};

/**
 * @brief The fundamental structure of a thread
 * It's a node in a circular linked list
 */
struct thread
{
    uint64_t tid; ///< Thread ID

    struct cpu_status *context; ///< Pointer to the kernel stack and current context of this thread

    enum thread_state state; ///< Its current state
    uint64_t ticks_remaining; ///< How many ticks before scheduling another thread?

    struct thread *next; ///< Pointer to the next thread
};

struct task *task_create(const char *name);
struct thread *task_create_thread(struct task *task, void (*entry_point)());

#endif // TASK_H