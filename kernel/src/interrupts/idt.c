#include <common/logging.h>
#include <stdint.h>
#include <stddef.h>
#include <interrupts/idt.h>
#include <memory/gdt/gdt.h>

// Aligned for performance
__attribute__((aligned(0x10))) 
static struct idt_descriptor idt[IDT_NUM_ENTRIES];

// This is the array of ISR's function pointers
extern void* isr_stub_table[];

// Set an idt descriptor in the table
void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags)
{
    struct idt_descriptor *currentDescriptor = &idt[vector];
    
    currentDescriptor->offset_1 = (uintptr_t)isr & 0xffff;
    currentDescriptor->segment_selector = GDT_KERNEL_CS * sizeof(uint64_t);
    currentDescriptor->ist = 0;
    currentDescriptor->flags = flags;
    currentDescriptor->offset_2 = ((uintptr_t)isr >> 16) & 0xffff;
    currentDescriptor->offset_3 = (uintptr_t)isr >> 32;
}

// Initializes the idt table
void idt_initialize_idtTable(void)
{
    // Fill the idt's 256 entries with the stub table
    for(size_t i = 0; i < IDT_NUM_ENTRIES; i++)
    {
        idt_set_descriptor(i, isr_stub_table[i], IDT_GATE_INTERRUPT_TYPE | (1 << 7));
    }

    // The idtr can stay on the stack since lidt stores a copy of it
    struct idtr idtr;
    idtr.offset = (uintptr_t)idt;
    idtr.size = sizeof(idt) - 1;
    
    // Finally load the new idt
    idt_load(&idtr);
    log_logLine(LOG_SUCCESS, "%s: IDT initialized", __FUNCTION__);
}
