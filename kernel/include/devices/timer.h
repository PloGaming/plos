#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <interrupts/isr.h>

#define TIMER_FREQUENCY_HZ 100

// PIT costansts
#define PIT_CMD_PORT  0x43
#define PIT_DATA_PORT 0x40
#define PIT_BASE_FREQ 1193182

// LAPIC timer costants
#define LAPIC_LVT_TIMER_REG     0x320
#define LAPIC_DIVIDE_REG        0x3E0
#define LAPIC_INITIAL_COUNT_REG 0x380
#define LAPIC_CURRENT_COUNT_REG 0x390

#define LAPIC_LVT_ONESHOT  (0 << 17)
#define LAPIC_LVT_PERIODIC (1 << 17)
#define LAPIC_LVT_VECTOR   32

void timer_init(void);

struct cpu_status* timer_handler(struct cpu_status* context);

uint64_t timer_get_uptime_ms();
uint64_t timer_get_uptime_ticks();

void timer_sleep();

#endif // TIMER_H