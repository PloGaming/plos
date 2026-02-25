#ifndef SCHEDULING_H
#define SCHEDULING_H

#include <interrupts/isr.h>

void scheduler_init();
void scheduler_yield();
struct cpu_status *scheduler_schedule(struct cpu_status *status);
void scheduler_wake_sleeping_threads();

#endif // SCHEDULING_H