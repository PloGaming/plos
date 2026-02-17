#include <common/logging.h>
#include <memory/vmm.h>
#include <memory/paging.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <cpu.h>
#include <drivers/lapic.h>

static void *lapic_MMIO = NULL;

/**
 * @brief Initialize the Local APIC
 * There's one LAPIC per CPU core, it's responsible for handling
 * cpu specific interrupts.
 */
void lapic_initialize(void)
{
    uint32_t cpuidResult;

    // Check for lapic/xapic support
    cpu_cpuid(CPUID_LAPIC_LEAF, 0, NULL, NULL, NULL, &cpuidResult);
    
    // Lapic is available only if the 9th bit is set
    if((cpuidResult & (1 << 9)) == 0)
    {
        log_line(LOG_ERROR, "%s: LAPIC is unavailable on this CPU", __FUNCTION__);
        hcf();
    }

    uint64_t lapicMSR = cpu_rdmsr(MSR_IA32_APIC_BASE);
    uint64_t phys_lapic_base = lapicMSR & APIC_BASE_MASK;

    // Map the MMIO page for the LAPIC configuration
    lapic_MMIO = vmm_alloc(vmm_get_kernel_vas(), 
        PAGING_PAGE_SIZE, 
        VMM_FLAGS_MMIO | VMM_FLAGS_READ | VMM_FLAGS_WRITE | VMM_FLAGS_UC, 
        phys_lapic_base);

    if(!lapic_MMIO)
    {
        log_line(LOG_ERROR, "%s: Cannot map LAPIC", __FUNCTION__);
        hcf();
    }

    log_line(LOG_SUCCESS, "%s: LAPIC/XAPIC initialized", __FUNCTION__);

    // Enable the spurious vector
    lapic_write(LAPIC_REG_SPURIOUS, SPURIOUS_VECTOR | APIC_ENABLE_BIT);
}

/**
 * @brief Helper function for writing to a LAPIC reg
 * @param reg The register offset
 * @param value The new value we want to assign
 */
void lapic_write(uint32_t reg, uint32_t value)
{
    volatile uint32_t* lapic_reg_ptr = (volatile uint32_t *)((uint64_t) lapic_MMIO + reg);
    *lapic_reg_ptr = value;
}

/**
 * @brief Helper function for reading from a LAPIC reg
 * 
 * @param reg The register offset
 * @return uint32_t The value present in the LAPIC reg
 */
uint32_t lapic_read(uint32_t reg)
{
    return *(volatile uint32_t *)((uint64_t) lapic_MMIO + reg);
}

/**
 * @brief Our ISR for the spurious vector 
 */
void lapic_spurious_isr()
{
    // Simply acknowledge the event
}

/**
 * @brief The function to call after executing each
 * interrupt from the LAPIC
 */
void lapic_send_EOI()
{
    lapic_write(LAPIC_REG_EOI, 0);
}