#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/**
 * @brief All the information about the caller when the interrupt is called
 * It describes the previous state of the gpr, the interrupt vector number and
 * an error code which is sometimes pushed with some particular CPU exceptions
 */
struct isr_context
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

void interrupt_handler(struct isr_context *context);

#endif // ISR_H