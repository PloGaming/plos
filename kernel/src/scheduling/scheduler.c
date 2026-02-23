#include <common/logging.h>
#include <cpu.h>
#include <interrupts/isr.h>
#include <memory/kheap.h>
#include <memory/vmm.h>
#include <scheduling/scheduler.h>
#include <scheduling/task.h>
#include <libk/string.h>
#include <stddef.h>

// The current task executing
struct task *task_current = NULL;
// A circular linked list of the current tasks in the system
struct task *task_list = NULL;

/**
 * @brief Initializes the scheduler, simply creates an idle task
 * This task is the main kernel task
 */
void scheduler_init()
{
    //We don't need to call task_create because the idle task already exists
    struct task *idle = kmalloc(sizeof(struct task));
    if(!idle)
    {
        log_line(LOG_ERROR, "%s: Cannot allocate the idle task struct", __FUNCTION__);
        hcf();
    }

    memset(idle, 0, sizeof(struct task));
    
    // Set the default values
    idle->pid = 0;
    strcpy(idle->name, "Idle");
    idle->state = TASK_RUNNING;
    idle->vas = vmm_get_kernel_vas();
    idle->ticks_remaining = TASK_INITIAL_TICKS;

    task_current = idle;
    idle->next = idle;
    task_list = idle;

    log_line(LOG_SUCCESS, "%s: Scheduler initialized", __FUNCTION__);
}

/**
 * @brief Round robin scheduler
 * Chooses the next ready task to execute and "pauses the other"
 * @param status The status of the previous task
 * @return struct cpu_status* The status of the new task to execute
 */
struct cpu_status *scheduler_schedule(struct cpu_status *status)
{
    if(!task_current) return status;

    // We "pause" the other task
    if(task_current->state == TASK_RUNNING)
    {
        task_current->state = TASK_READY;
    }
    
    // We save it's state so it can resume at another scheduling
    task_current->kstack_sp = status;
    
    // Search for a ready task
    do 
    {
        task_current = task_current->next;
    } while (task_current->state != TASK_READY);

    task_current->state = TASK_RUNNING;

    return task_current->kstack_sp;
}

/**
 * @brief Yields the cpu for the current process
 */
void scheduler_yield()
{
    asm volatile("int $50");
}