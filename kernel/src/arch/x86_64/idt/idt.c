#include <stdint.h>
#include <stddef.h>
#include "idt.h"
#include "../gdt/gdt.h"

// Aligned for performance
__attribute__((aligned(0x10))) 
struct idt_descriptor idt[IDT_NUM_ENTRIES];

// This is the array of ISR's function pointers
extern void* isr_stub_table[];

static struct idt_descriptor idt_create_descriptor(void *isr, uint8_t flags)
{
    struct idt_descriptor currentDescriptor;
    
    currentDescriptor.offset_1 = (uintptr_t)isr & 0xffff;
    currentDescriptor.segment_selector = GDT_CODE_PL0 * sizeof(uint64_t);
    currentDescriptor.ist = 0;
    currentDescriptor.flags = flags;
    currentDescriptor.offset_2 = ((uintptr_t)isr >> 16) & 0xffff;
    currentDescriptor.offset_3 = (uintptr_t)isr >> 32;
    return currentDescriptor;
}

void idt_initialize_idtTable(void)
{
    for(size_t i = 0; i < 32; i++)
    {
        idt[i] = idt_create_descriptor(isr_stub_table[i], 
        IDT_GATE_INTERRUPT_TYPE | (1 << 7));
    }

    struct idtr idtr;
    idtr.offset = (uintptr_t)idt;
    idtr.size = sizeof(idt) - 1;
    
    idt_load(&idtr);
}
