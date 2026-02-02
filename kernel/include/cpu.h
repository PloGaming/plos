#ifndef CPU_H
#define CPU_H

#include <stdint.h>

// Halt and catch fire function.
__attribute__((noreturn)) void hcf(void);
void cpu_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
inline uint64_t cpu_rdmsr(uint32_t msr_index);
inline void cpu_wrmsr(uint32_t msr_index, uint64_t value);

#endif // CPU_H