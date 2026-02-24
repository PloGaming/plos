#include <common/logging.h>
#include <interrupts/isr.h>
#include <scheduling/task.h>
#include <memory/gdt/gdt.h>
#include <memory/kheap.h>
#include <memory/vmm.h>
#include <scheduling/scheduler.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <libk/string.h>
#include <libk/string.h>

extern struct task *task_current;
extern struct thread *thread_current;
extern struct task *task_list;

uint64_t next_pid = 1;
uint64_t next_tid = 1;

/**
 * @brief Creates a new kernel task/process
 * 
 * @param name A name for the task, useful for debugging, could pass NULL
 * @return struct task* Pointer to the newly allocated task if successful, otherwise NULL
 */
struct task *task_create(const char *name)
{
    // Allocate space for the new kernel task struct
    struct task *new_task = kmalloc(sizeof(struct task));
    if(!new_task)
    {
        return NULL;
    }

    // We set it to zero
    memset(new_task, 0, sizeof(struct task));

    strcpy(new_task->name, name);
    new_task->vas = vmm_get_kernel_vas();
    new_task->pid = next_pid++; // we have 2^64 possible pids, i won't check if we overflow :)
    new_task->threads = NULL; // No threads

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

struct thread *task_create_thread(struct task *task, void (*entry_point)())
{
    // Sanity check
    if(!task || !entry_point) return NULL;

    // Allocate space for our thread struct
    struct thread *new_thread = kmalloc(sizeof(struct thread));
    if(!new_thread)
    {
        return NULL;
    }

    // We zero the newly created thead structure
    memset(new_thread, 0x00, sizeof(struct thread));

    // We allocate a new area for the stack of the thread
    void *new_stack_bottom = vmm_alloc(
        vmm_get_kernel_vas(), 
        THREAD_INITIAL_STACK_SIZE, 
        VMM_FLAGS_READ | VMM_FLAGS_WRITE | VMM_FLAGS_ANON, // Note that the stack is NOT executable
        false);

    if(!new_stack_bottom)
    {
        kfree(new_thread);
        return NULL;
    }

    // The stack grows downward so it's start is actually the last byte of the newly allocated memory
    uint64_t stack_top = (uint64_t) new_stack_bottom + THREAD_INITIAL_STACK_SIZE;
    
    // We set the default returning function to be our uint64_t *
    uint64_t *ret_addr = (uint64_t *)(stack_top - sizeof(uint64_t *));
    *ret_addr = (uint64_t) task_current_thread_exit;

    // This is the real thread stack pointer, we have to reserve some space for the interrupt context
    uint64_t sp = (uint64_t) ret_addr - sizeof(struct cpu_status);
    struct cpu_status *status = (struct cpu_status *)sp;

    // We set every register to zero
    memset(status, 0, sizeof(struct cpu_status));

    // Where the thread has to resume + set the correct gdt segments
    status->rip = (uint64_t)entry_point;
    status->cs = GDT_KERNEL_CS * sizeof(uint64_t);
    status->ss = GDT_KERNEL_DS * sizeof(uint64_t);

    status->rsp = (uint64_t)ret_addr; // After the interrupt it can safely overwrite the last struct
    status->rflags = 0x202; // We just set the Interrupt enabled flag and a legacy flag

    // We set the newly crafted status to the thread
    new_thread->context = (struct cpu_status *)sp;
    new_thread->state = THREAD_READY;
    new_thread->ticks_remaining = THREAD_INITIAL_TICKS;
    new_thread->tid = next_tid++; // we have 2^64 possible tids, i won't check if we overflow :)

    // Disable interrupts while we modify the list
    asm volatile("cli");

    // We add it to the list of threads of the task
    if(task->threads == NULL)
    {
        task->threads = new_thread;
        new_thread->next = new_thread;
    }
    else 
    {
        struct thread *current = task->threads;
        while(current->next != task->threads)
        {
            current = current->next;
        }
        current->next = new_thread;
        new_thread->next = task->threads;
    }

    asm volatile("sti");

    log_line(LOG_DEBUG, "%s: Thread created to process PID %lld (TID %lld)", __FUNCTION__, task->pid, new_thread->tid);
    return new_thread;
}

/**
 * @brief The function to exit a thread
 */
__attribute__((noreturn))
void task_current_thread_exit()
{
    // Disable interrupts while we modify the status of a thread
    asm volatile("cli");

    log_line(LOG_DEBUG, "%s: Thread TID %lld of task PID %lld is terminated", __FUNCTION__, thread_current->tid, task_current->pid);

    // Change the status of a thread, the idle process will eventually free everything
    thread_current->state = THREAD_ZOMBIE;

    // Give up the CPU
    scheduler_yield();

    // We should never come here
    while(1)
    {
        asm volatile("hlt");
    }
}

/**
 * @brief This function is the reaper of the kernel 
 * It frees up the space
 */
__attribute__((noreturn))
void kernel_idle_thread()
{
    while(1)
    {
        // Disable interrupts
        asm volatile ("cli");

        if(task_list != NULL)
        {
            // Iterate all tasks
            struct task *curr_task = task_list;
            do
            {
                if(curr_task->threads != NULL)
                {
                    struct thread *prev_thread = curr_task->threads;
                    struct thread *curr_thread = prev_thread->next;
                    bool task_empty = false;

                    do
                    {
                        if(curr_thread->state == THREAD_ZOMBIE)
                        {
                            log_line(LOG_DEBUG, "%s: Removed TID %lld from PID %lld", __FUNCTION__, curr_thread->tid, curr_task->pid);
                            
                            // Special case it's the only thread in the task
                            if(curr_thread == curr_thread->next)
                            {
                                curr_task->threads = NULL;
                                task_empty = true;
                            }
                            else 
                            {
                                prev_thread->next = curr_thread->next;
                                if(curr_thread == curr_task->threads)
                                {
                                    curr_task->threads = curr_thread->next;
                                }
                            }

                            struct thread *thread_to_delete = curr_thread;
                            curr_thread = curr_thread->next;

                            vmm_free(curr_task->vas, thread_to_delete->context->rsp);

                            kfree(thread_to_delete);

                            if(task_empty) break;
                        }
                        else 
                        {
                            prev_thread = curr_thread;
                            curr_thread = curr_thread->next;
                        }
                        
                    } while(curr_task->threads != NULL && curr_thread != curr_task->threads);
                }

                curr_task = curr_task->next;
            } while(curr_task != task_list);
        }

        asm volatile ("sti");

        asm volatile ("hlt");
    }
}