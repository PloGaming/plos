#ifndef LAPIC_H
#define LAPIC_H

#include <stdbool.h>

#define MSR_IA32_APIC_BASE  0x1B
#define CPUID_LAPIC_LEAF    0x1

#define APIC_BASE_MASK  0xFFFFFFFFFFFFF000

void lapic_initialize(void);

#endif // LAPIC_H