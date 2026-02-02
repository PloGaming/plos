#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <cpu.h>
#include <drivers/lapic.h>

bool lapic_initialize(void)
{
    uint32_t cpuidResult;
    cpu_cpuid(CPUID_LAPIC_LEAF, 0, NULL, NULL, NULL, &cpuidResult);
    
    // Lapic is available only if the 9th bit is set
    if((cpuidResult & (1 << 9)) == 0)
    {
        return false;
    }

    // Lapic registers are MMIO (memory mapped I/O) so we need to get that address
    // by reading a specific MSR: IA32_APIC_BASE
    uint64_t lapicMSR = cpu_rdmsr(MSR_IA32_APIC_BASE);
    

    return true;
}