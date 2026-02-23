#include <common/logging.h>
#include <devices/timer.h>
#include <drivers/lapic.h>
#include <memory/vmm.h>
#include <interrupts/isr.h>
#include <cpu.h>
#include <scheduling/scheduler.h>

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
        case PAGE_FAULT_VECTOR:
            vmm_page_fault_handler(context);
            break;
        case LAPIC_TIMER_VECTOR: // forced pre-emption 
            next_stack = timer_handler(context);
            break;
        case YIELD_VECTOR: // voluntarily pre-emption
            next_stack = scheduler_schedule(context);
            break;
        case SPURIOUS_VECTOR: // LAPIC 
            lapic_spurious_isr();
            break;
        default:
            log_line(LOG_DEBUG, "Interrupt fired: 0x%llx error code: 0x%llx", context->vector_number, context->error_code);
            break;
    }

    return next_stack;
}