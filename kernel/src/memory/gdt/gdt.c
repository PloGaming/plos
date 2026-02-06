#include <common/logging.h>
#include <stdint.h>
#include <memory/gdt/gdt.h>
#include <drivers/serial.h>

// Aligned for performance
__attribute__((aligned(0x08))) 
uint64_t gdt_table[NUM_GDT_ENTRIES];

static uint64_t gdt_create_descriptor(uint32_t base, uint32_t limit, uint16_t flag)
{
    uint64_t descriptor;
 
    // Create the high 32 bit segment
    descriptor  =  limit       & 0x000F0000;         // set limit bits 19:16
    descriptor |= (flag <<  8) & 0x00F0FF00;         // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor |= (base >> 16) & 0x000000FF;         // set base bits 23:16
    descriptor |=  base        & 0xFF000000;         // set base bits 31:24
 
    // Shift by 32 to allow for low part of segment
    descriptor <<= 32;
 
    // Create the low 32 bit segment
    descriptor |= base  << 16;                       // set base bits 15:0
    descriptor |= limit  & 0x0000FFFF;               // set limit bits 15:0

    return descriptor;
}

void gdt_initialize_gdtTable(void)
{
    // The first entry must be set to zero
    gdt_table[0] = 0;

    // Set the other entries (Base and limit are ignored in long mode)
    gdt_table[GDT_KERNEL_CS] = gdt_create_descriptor(0, 0, GDT_CODE_PL0);
    gdt_table[GDT_KERNEL_DS] = gdt_create_descriptor(0, 0, GDT_DATA_PL0);
    gdt_table[GDT_USER_CS] = gdt_create_descriptor(0, 0, GDT_CODE_PL3);
    gdt_table[GDT_USER_DS] = gdt_create_descriptor(0, 0, GDT_DATA_PL3); 

    // It's okay if the gdtr is saved on the stack since lgdt stores a copy of it
    struct GDTR gdtr;
    gdtr.size = sizeof(gdt_table) - 1;
    gdtr.offset = (uint64_t) &gdt_table;
    gdt_load(&gdtr);

    log_logLine(LOG_SUCCESS, "%s: GDT initialized", __FUNCTION__);
}