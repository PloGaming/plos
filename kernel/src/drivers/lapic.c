#include <common/logging.h>
#include <memory/vmm.h>
#include <memory/paging.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <cpu.h>
#include <drivers/lapic.h>

static void *lapicMMIO = NULL;

/**
 * @brief Initialize the Local APIC
 * There's one LAPIC per CPU core, it's responsible for handling
 * cpu specific interrupts.
 */
void lapic_initialize(void)
{
    uint32_t cpuidResult;
    cpu_cpuid(CPUID_LAPIC_LEAF, 0, NULL, NULL, NULL, &cpuidResult);
    
    // Lapic is available only if the 9th bit is set
    if((cpuidResult & (1 << 9)) == 0)
    {
        log_line(LOG_ERROR, "%s: LAPIC is unavailable on this CPU", __FUNCTION__);
        hcf();
    }

    // Lapic registers are MMIO (memory mapped I/O) so we need to get that address
    // by reading a specific MSR: IA32_APIC_BASE
    uint64_t lapicMSR = cpu_rdmsr(MSR_IA32_APIC_BASE);
    uint64_t phys_lapic_base = lapicMSR & APIC_BASE_MASK;

    // Map the MMIO page for the LAPIC configuration
    lapicMMIO = vmm_alloc(vmm_get_kernel_vas(), 
        PAGING_PAGE_SIZE, 
        VMM_FLAGS_MMIO | VMM_FLAGS_READ | VMM_FLAGS_WRITE | VMM_FLAGS_UC, 
        phys_lapic_base);

    if(!lapicMMIO)
    {
        log_line(LOG_ERROR, "%s: Cannot map LAPIC", __FUNCTION__);
        hcf();
    }
}

/**
 * @brief Helper function for writing to a LAPIC reg
 * 
 * @param reg The register offset
 * @param value The new value we want to assign
 */
void lapic_write(uint32_t reg, uint32_t value)
{
    volatile uint32_t* lapic_ptr = (volatile uint32_t *)((uint64_t) lapicMMIO + reg);
    *lapic_ptr = value;
}

/**
 * @brief Helper function for reading from a LAPIC reg
 * 
 * @param reg The register offset
 * @return uint32_t The value present in the LAPIC reg
 */
uint32_t lapic_read(uint32_t reg)
{
    return *(volatile uint32_t *)((uint64_t) lapicMMIO + reg);
}