#include <common/logging.h>
#include <devices/timer.h>
#include <drivers/lapic.h>
#include <memory/vmm.h>
#include <interrupts/isr.h>
#include <cpu.h>

/**
 * @brief Main interrupt handler
 * Each interrupt will eventually execute this function,
 * it's a sort of dispatcher
 * @param context container of all the info of the code executing before the interrupt
 * @return struct cpu_status*, the new cpu context (useful only for context switching)
 */
struct cpu_status* interrupt_handler(struct cpu_status *context)
{
    struct cpu_status* next_stack = context;

    switch(context->vector_number)
    {
        case 14: // Page fault
            vmm_page_fault_handler(context);
            break;
        case 32: // Lapic timer (scheduling) 
            next_stack = timer_handler(context);
            break;
        case 0xFF: // Spurious interrupt
            lapic_spurious_isr();
            break;
        default:
            log_line(LOG_DEBUG, "Interrupt fired: 0x%llx error code: 0x%llx", context->vector_number, context->error_code);
            break;
    }

    return next_stack;
}