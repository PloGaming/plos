#ifndef LOCK_H
#define LOCK_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Basic lock structure
 * It's a yielding ticket lock mechanism.
 * It ensures each thread gets a fair usage of the resource by keeping track
 * of turns. If it's not the thread turn it simply yields the CPU to another thread
 */
struct lock_ticket {
    volatile uint64_t ticket; ///< The ticket "distributor"
    volatile uint64_t turn; ///< The current turn of the lock
};

#define LOCK_TICKET_INIT {0, 0}

void lock_init(struct lock_ticket *lock);
void lock_acquire(struct lock_ticket *lock);
void lock_release(struct lock_ticket *lock);

/**
 * @brief Basic spinlock structure
 * It should be used instead of the lock_ticket struct when 
 * we're in a low level context such as interrupts which can't yield the CPU
 */
struct spinlock {
    volatile bool locked;
};

#define SPINLOCK_INIT {false}


#endif // LOCK_H