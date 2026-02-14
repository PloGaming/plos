#include <common/logging.h>
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
    log_log_line(LOG_DEBUG, "Interrupt fired: 0x%llx error code: 0x%llx", context->vector_number, context->error_code);
    switch(context->vector_number)
    {
        // Page fault
        case 14:
            vmm_page_fault_handler(context);
        break;
    }
}