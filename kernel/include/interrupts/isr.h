#ifndef ISR_H
#define ISR_H

#include <stdint.h>

#define PAGE_FAULT_VECTOR   14
#define LAPIC_TIMER_VECTOR  32
#define YIELD_VECTOR        50
#define SPURIOUS_VECTOR     0xFF

/**
 * @brief All the information about the caller when the interrupt is called
 * It describes the previous state of the gpr, the interrupt vector number and
 * an error code which is sometimes pushed with some particular CPU exceptions
 */
struct cpu_status
{
    uint64_t general_purpose_registers[16]; ///< The 16 GPRs

    uint64_t vector_number;
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

struct cpu_status* interrupt_handler(struct cpu_status *context);

#endif // ISR_H