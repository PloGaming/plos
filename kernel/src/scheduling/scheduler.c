#include <common/logging.h>
#include <cpu.h>
#include <devices/timer.h>
#include <interrupts/isr.h>
#include <memory/kheap.h>
#include <memory/vmm.h>
#include <scheduling/lock.h>
#include <scheduling/scheduler.h>
#include <scheduling/task.h>
#include <libk/string.h>
#include <stddef.h>
#include <stdint.h>

// The current task/thread executing
struct task *task_current = NULL;
struct thread *thread_current = NULL;

// A circular linked list of the current tasks in the system
struct task *task_list = NULL;

struct spinlock_irq scheduler_lock = SPINLOCK_IRQ_INIT;

/**
 * @brief Initializes the scheduler, simply creates an idle task and thread
 * This task is the main kernel task (or idle task)
 */
void scheduler_init()
{
    // We don't need to call task_create because the idle task already exists
    struct task *idle = kmalloc(sizeof(struct task));
    if(!idle)
    {
        log_line(LOG_ERROR, "%s: Cannot allocate the idle task struct", __FUNCTION__);
        hcf();
    }

    memset(idle, 0, sizeof(struct task));
    
    // Set the default values for the task
    idle->pid = 0;
    strcpy(idle->name, "Idle");
    idle->vas = vmm_get_kernel_vas();

    // Set the global variables
    task_current = idle;
    idle->next = idle;
    task_list = idle;

    // Create the idle thread
    struct thread *idle_thread = kmalloc(sizeof(struct thread));
    if(!idle_thread)
    {
        kfree(idle);
        log_line(LOG_ERROR, "%s: Cannot allocate the idle thread struct", __FUNCTION__);
        hcf();
    }

    memset(idle_thread, 0, sizeof(struct thread));

    // Set the default values for the thread
    idle_thread->tid = 0;
    idle_thread->state = THREAD_RUNNING;
    idle_thread->ticks_remaining = THREAD_INITIAL_TICKS;

    // Link all the pieces
    idle->threads = idle_thread;
    idle_thread->next = idle_thread;
    thread_current = idle_thread;

    log_line(LOG_SUCCESS, "%s: Scheduler initialized", __FUNCTION__);
}

/**
 * @brief Round robin scheduler
 * Chooses the next ready thread to execute and "pauses the other"
 * @param status The status of the previous thread
 * @return struct cpu_status* The status of the new thread to execute
 */
struct cpu_status *scheduler_schedule(struct cpu_status *status)
{
    if(!task_current || !thread_current) return status;

    uint64_t irq_flags;
    spinlock_irq_acquire(&scheduler_lock, &irq_flags);

    // We "pause" the old thread
    if(thread_current->state == THREAD_RUNNING)
    {
        thread_current->state = THREAD_READY;
    }
    
    // We save it's state so it can resume at another scheduling
    thread_current->context = status;
    
    // Search for a ready thread
    struct task *search_task = task_current;
    struct thread *search_thread = thread_current->next;

    // We have to check if the previous thread was the only one in the task
    if(search_thread == search_task->threads)
    {
        search_task = search_task->next;
        if(search_task) 
        {
            search_thread = search_task->threads;
        }
    }

    while(true)
    {
        // If this task has no threads we skip it
        if(!search_thread)
        {
            search_task = search_task->next;
            search_thread = search_task->threads;
            continue;
        }

        // We found a thread to execute!
        if(search_thread->state == THREAD_READY)
        {
            break;
        }

        search_thread = search_thread->next;
        
        // This means we checked all threads from the task
        if(search_thread == search_task->threads)
        {
            search_task = search_task->next;
            search_thread = search_task->threads;
        }

        // Anti-freeze check
        if(search_task == task_current && search_thread == thread_current)
        {
            // If the thread is ready we put him up
            if(search_thread->state == THREAD_READY)
            {
                break;
            }
            // Otherwise we choose the idle thread
            else 
            {
                search_task = task_list;
                search_thread = search_task->threads;
            }
        }
    }

    // Update the global variables
    task_current = search_task;
    thread_current = search_thread;

    thread_current->state = THREAD_RUNNING;

    spinlock_irq_release(&scheduler_lock, &irq_flags);

    return thread_current->context;
}

/**
 * @brief Iterate all threads to wake up the sleeping ones
 */
void scheduler_wake_sleeping_threads()
{
    if(!task_list) return;

    uint64_t irq_flags;
    spinlock_irq_acquire(&scheduler_lock, &irq_flags);

    uint64_t currTimeMs = timer_get_uptime_ms();

    // Iterate all tasks
    struct task *curr_task = task_list;
    do 
    {
        if(curr_task->threads != NULL)
        {
            // Iterate all threads
            struct thread *curr_thread = curr_task->threads;
            do
            {
                // Wakeup the thread
                if(curr_thread->state == THREAD_SLEEPING && curr_thread->wake_time <= currTimeMs)
                {
                    curr_thread->state = THREAD_READY;
                    curr_thread->wake_time = 0;
                }

                curr_thread = curr_thread->next;
            } while(curr_thread != curr_task->threads);
        }

        curr_task = curr_task->next;
    } while (curr_task != task_list);

    spinlock_irq_release(&scheduler_lock, &irq_flags);
}

/**
 * @brief Yields the cpu for the current thread
 */
void scheduler_yield()
{
    asm volatile("int $50");
}