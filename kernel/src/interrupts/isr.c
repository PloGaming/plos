#include <common/logging.h>
#include <drivers/lapic.h>
#include <memory/vmm.h>
#include <interrupts/isr.h>
#include <cpu.h>

/**
 * @brief Main interrupt handler
 * Each interrupt will eventually execute this function,
 * it's a sort of dispatcher
 * @param context container of all the info of the code executing before the interrupt
 */
void interrupt_handler(struct isr_context *context)
{
    switch(context->vector_number)
    {
        case 14: // Page fault
            vmm_page_fault_handler(context);
            break;
        case 0xFF: // Spurious interrupt
            lapic_spurious_isr();
            break;
        default:
            log_line(LOG_DEBUG, "Interrupt fired: 0x%llx error code: 0x%llx", context->vector_number, context->error_code);
            break;
    }
}