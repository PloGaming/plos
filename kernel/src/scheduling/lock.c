#include <scheduling/lock.h>
#include <scheduling/scheduler.h>
#include <scheduling/task.h>
#include <stddef.h>
#include <stdint.h>

static inline uint64_t interrupts_save_and_disable(void) 
{
    uint64_t flags;
    asm volatile (
        "pushfq\n\t"        // push RFLAGS
        "pop %0\n\t"        // pop RFLAGS in flags
        "cli"               // Disables interrupts
        : "=r" (flags)
        :
        : "memory"
    );
    return flags;
}

static inline void interrupts_restore(uint64_t flags) 
{
    asm volatile (
        "push %0\n\t"       // push the old RFLAGS
        "popfq"             // pops into RFLAGS
        :
        : "r" (flags)
        : "memory", "cc"
    );
}

/**
 * @brief Acquisition function for a struct spinlock_irq
 * This function is blocking and it's safe for use in ISR's
 * @param lock pointer to the spinlock_irq
 * @param flags pointer to the old flags
 */
void spinlock_irq_acquire(struct spinlock_irq *lock, uint64_t *flags)
{
    if(!lock || !flags) return;

    // We disable interrupts and save the old RFLAGS
    *flags = interrupts_save_and_disable();

    // We take the ticket
    uint64_t myticket = __atomic_fetch_add(&lock->ticket, 1, __ATOMIC_SEQ_CST);
    
    // We spin until it's our turn
    while(lock->turn != myticket)
    {
        asm volatile ("pause");
    }
}

/**
 * @brief Release function for struct spinlock_irq
 * @param lock Pointer to the spinlock_irq
 * @param flags pointer to the old flags
 */
void spinlock_irq_release(struct spinlock_irq *lock, uint64_t *flags)
{
    if(!lock || !flags) return;

    // We pass the lock to the next in line
    __atomic_fetch_add(&lock->turn, 1, __ATOMIC_SEQ_CST);
    interrupts_restore(*flags);
}

extern struct thread *thread_current;

/**
 * @brief Acquisition function for the mutex
 * @param mutex Pointer to the mutex
 */
void mutex_acquire(struct mutex *mutex)
{
    uint64_t irq_flags;

    // Take the internal lock
    spinlock_irq_acquire(&mutex->internal_lock, &irq_flags);

    // If it's unlocked we take it
    if(!mutex->is_locked)
    {
        mutex->is_locked = true;
        spinlock_irq_release(&mutex->internal_lock, &irq_flags);
        return;
    }

    // Otherwise we have to put ourselves to sleep
    thread_current->next_waiter = NULL;
    
    // It's the first thread of the waiting queue
    if(mutex->waiting_queue_tail == NULL)
    {
        mutex->waiting_queue_head = thread_current;
        mutex->waiting_queue_tail = thread_current;
    }
    else
    {
        // Otherwise we put ourselves at the end of the queue
        mutex->waiting_queue_tail->next_waiter = thread_current;
        mutex->waiting_queue_tail = thread_current;
    }

    // Set us as blocked so that the scheduler won't schedule us
    thread_current->state = THREAD_BLOCKED;

    // Release the internal lock
    spinlock_irq_release(&mutex->internal_lock, &irq_flags);

    // The next time this thread will execute will be when we've been waken up by another thread
    scheduler_yield();
}

/**
 * @brief Realising function for the mutex
 * @param mutex 
 */
void mutex_release(struct mutex *mutex)
{
    uint64_t irq_flags;

    // Take the internal lock
    spinlock_irq_acquire(&mutex->internal_lock, &irq_flags);

    // If no one is in queue we simply unlock it
    if(mutex->waiting_queue_head == NULL)
    {
        mutex->is_locked = false;
    }
    else 
    {
        // Otherwise we wake up another thread
        struct thread *woken_thread = mutex->waiting_queue_head;
        mutex->waiting_queue_head = mutex->waiting_queue_head->next_waiter;
        if(mutex->waiting_queue_head == NULL)
        {
            mutex->waiting_queue_tail = NULL;
        }

        // Set the thread as ready
        woken_thread->state = THREAD_READY;
    }

    // Release the internal lock
    spinlock_irq_release(&mutex->internal_lock, &irq_flags);
}