#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_NUM_ENTRIES 256 ///< The total number of entries in the interrupt descriptor table

#define IDT_GATE_INTERRUPT_TYPE 0b1110
#define IDT_GATE_TRAP_TYPE 0b1111

/**
 * @brief The IDT entry structure
 * It describes the function to call when a certain
 * interrupt is called and which code segment to use
 * 
 */
struct idt_descriptor
{
    uint16_t offset_1; ///< First part of the address
    uint16_t segment_selector;
    uint8_t ist;  ///< Offset into the interrupt stack table
    uint8_t flags; ///< Flags that describe this IDT entry
    uint16_t offset_2; ///< Second part of the address
    uint32_t offset_3; ///< Third part of the address
    uint32_t reserved;
} __attribute__((packed));

/**
 * @brief The Interrupt descriptor table register
 * It says to the CPU where to look for the IDT
 */
struct idtr
{
    uint16_t size; ///< How bug is the table - 1
    uintptr_t offset; ///< Where to find the table
} __attribute__((packed));

extern void idt_load(struct idtr* idtr);
void idt_init(void);
void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags);

#endif // IDT_H