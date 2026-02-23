#include <common/logging.h>
#include <interrupts/isr.h>
#include <scheduling/task.h>
#include <memory/gdt/gdt.h>
#include <memory/kheap.h>
#include <memory/vmm.h>
#include <scheduling/scheduler.h>
#include <stddef.h>
#include <stdint.h>
#include <libk/string.h>
#include <libk/string.h>

extern struct task *task_current;
extern struct task *task_list;

/**
 * @brief Creates a new kernel process
 * 
 * @param name A name for the task, useful for debugging, could pass NULL
 * @param entry_point Pointer to the code of the task
 * @param pid Process identifier, an unique number for identifying our task
 * @return struct task* Pointer to the newly allocated task if successful, otherwise NULL
 */
struct task *task_create(const char *name, void (*entry_point)(), uint64_t pid)
{
    // Allocate space for the new kernel task struct
    struct task *new_task = kmalloc(sizeof(struct task));
    if(!new_task)
    {
        return NULL;
    }

    // We set it to zero
    memset(new_task, 0, sizeof(struct task));

    // We allocate a new area for the stack of the process
    void *new_stack_bottom = vmm_alloc(
        vmm_get_kernel_vas(), 
        TASK_INITIAL_STACK_SIZE, 
        VMM_FLAGS_READ | VMM_FLAGS_WRITE | VMM_FLAGS_ANON, // Note that the stack is NOT executable
        false);

    if(!new_stack_bottom)
    {
        kfree(new_task);
        return NULL;
    }

    // The stack grows downward so it's start is actually the last byte of the newly allocated memory
    uint64_t stack_top = (uint64_t) new_stack_bottom + TASK_INITIAL_STACK_SIZE;
    
    // This is the real process stack pointer, we have to reserve some space for the interrupt context
    uint64_t sp = stack_top - sizeof(struct cpu_status);
    struct cpu_status *status = (struct cpu_status *)sp;
    
    // We set every register to zero
    memset(status, 0, sizeof(struct cpu_status));

    // Where the task has to resume
    status->rip = (uint64_t)entry_point;
    status->cs = GDT_KERNEL_CS * sizeof(uint64_t);
    status->ss = GDT_KERNEL_DS * sizeof(uint64_t);

    // After the interrupt it can safely overwrite the last struct
    status->rsp = stack_top;
    status->rflags = 0x202; // We just set the Interrupt enabled flag

    new_task->vas = vmm_get_kernel_vas();
    new_task->kstack_sp = (struct cpu_status *)sp;
    strcpy(new_task->name, name);
    new_task->state = TASK_READY;
    new_task->ticks_remaining = TASK_INITIAL_TICKS;
    new_task->pid = pid;

    // We add it to the list of tasks
    if(task_list == NULL)
    {
        task_list = new_task;
        new_task->next = new_task;
    }
    else 
    {
        struct task *current = task_list;
        while(current->next != task_list)
        {
            current = current->next;
        }
        current->next = new_task;
        new_task->next = task_list;
    }

    log_line(LOG_DEBUG, "%s: Task created %s (PID %lld)", __FUNCTION__, new_task->name, new_task->pid);
    return new_task;
}