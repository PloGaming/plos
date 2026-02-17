#ifndef LAPIC_H
#define LAPIC_H

#include <stdbool.h>
#include <stdint.h>

#define MSR_IA32_APIC_BASE  0x1B
#define CPUID_LAPIC_LEAF    0x1

#define CPUID_X2APIC_LEAF   1

#define LAPIC_MSR_BASE      0x800
#define APIC_BASE_MASK      0xFFFFFFFFFFFFF000
#define APIC_ENABLE_BIT     0x100

#define LAPIC_REG_SPURIOUS  0x0F0
#define SPURIOUS_VECTOR     0xFF
#define LAPIC_REG_EOI       0x00B0

void lapic_initialize(void);
void lapic_write(uint32_t reg, uint32_t value);
uint32_t lapic_read(uint32_t reg);
void lapic_spurious_isr();
void lapic_send_EOI();

#endif // LAPIC_H