#include <scheduling/lock.h>
#include <scheduling/scheduler.h>
#include <stdint.h>

/**
 * @brief Initializes a lock
 * @param lock A pointer to the lock
 */
void lock_init(struct lock_ticket *lock)
{
    if(!lock) return;

    lock->ticket = 0;
    lock->turn = 0;
}

/**
 * @brief Acquires the lock
 * 
 * @param lock A pointer to the lock
 */
void lock_acquire(struct lock_ticket *lock)
{
    if(!lock) return;

    // Get our ticket
    uint64_t myTicket = __atomic_fetch_add(&lock->ticket, 1, __ATOMIC_SEQ_CST);
    
    // Wait until it's our turn
    while(lock->turn != myTicket)
    {
        // To reduce CPU load we simply yield
        scheduler_yield();
    }
}

/**
 * @brief Releases the lock
 * 
 * @param lock A pointer to the lock
 */
void lock_release(struct lock_ticket *lock)
{
    if(!lock) return;

    // We finished to use the lock so we simply increment the turn
    __atomic_fetch_add(&lock->ticket, 1, __ATOMIC_SEQ_CST);
}