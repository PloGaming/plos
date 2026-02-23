#ifndef SCHEDULING_H
#define SCHEDULING_H

#include <interrupts/isr.h>

void scheduler_init();
struct cpu_status *scheduler_schedule(struct cpu_status *status);

#endif // SCHEDULING_H