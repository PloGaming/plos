#ifndef LOCK_H
#define LOCK_H

#include <stdint.h>
#include <stdbool.h>


/**
 * @brief A ticket lock safe to use inside ISRs
 * It spins (loop) until it's your turn. It ensures
 * fairness among the users of the lock
 */
struct spinlock_irq 
{
    volatile uint64_t ticket; ///< The ticket "distributor"
    volatile uint64_t turn; ///< The current turn of the lock
};

#define SPINLOCK_IRQ_INIT {0, 0}

void spinlock_irq_acquire(struct spinlock_irq *lock, uint64_t *flags);
void spinlock_irq_release(struct spinlock_irq *lock, uint64_t *flags);


struct thread;

/**
 * @brief 
 * 
 */
struct mutex 
{
    bool is_locked;
    struct spinlock_irq internal_lock;
    struct thread *waiting_queue_head;
    struct thread *waiting_queue_tail;
};

#define MUTEX_INIT {false, SPINLOCK_IRQ_INIT, NULL, NULL}

void mutex_acquire(struct mutex *mutex);
void mutex_release(struct mutex *mutex);

#endif // LOCK_H