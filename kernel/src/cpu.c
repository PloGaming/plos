#include <stdint.h>
#include <stdbool.h>
#include <cpu.h>
#include <memory/gdt/gdt.h>
#include <interrupts/idt.h>

__attribute__((noreturn)) void hcf(void)
{
    for(;;)
    {
        asm("hlt");
    }
}

// Reads from a Model Specific Register (MSR)
inline uint64_t cpu_rdmsr(uint32_t msr_index) {
    uint32_t low, high;

    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high) // Output: EAX in 'low', EDX in 'high'
        : "c"(msr_index)        // Input: MSR index in ECX
    );

    // Combine the 2 registers
    return ((uint64_t)high << 32) | low;
}

// Writes into a Model Specific Register (MSR).
inline void cpu_wrmsr(uint32_t msr_index, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;

    __asm__ volatile (
        "wrmsr"
        : // No output
        : "c"(msr_index), "a"(low), "d"(high) // Input: ECX, EAX, EDX
    );
}

// Executes the cpuid instruction
void cpu_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) 
{
    uint32_t rax, rbx, rcx, rdx;

    __asm__ volatile (
    "cpuid"
    : "=a"(rax), "=b"(rbx), "=c"(rcx), "=d"(rdx)
    : "a"(leaf), "c"(subleaf)
    : "memory"
    );

    if (eax) *eax = rax;
    if (ebx) *ebx = rbx;
    if (ecx) *ecx = rcx;
    if (edx) *edx = rdx;
}

inline uint64_t read_cr4() 
{
    uint64_t val;
    asm volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

inline void write_cr4(uint64_t val) 
{
    asm volatile("mov %0, %%cr4" :: "r"(val));
}