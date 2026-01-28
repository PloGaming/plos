#include "pic.h"
#include <hal/portsIO.h>

void disable_pic(void) {
    // PIC Initialization
    outb(PIC_COMMAND_MASTER, ICW_1);
    outb(PIC_COMMAND_SLAVE, ICW_1);

    // PIC remapping
    outb(PIC_DATA_MASTER, ICW_2_M);
    outb(PIC_DATA_SLAVE, ICW_2_S);
    outb(PIC_DATA_MASTER, ICW_3_M);
    outb(PIC_DATA_SLAVE, ICW_3_S);

    // Telling the PIC we're using the 8086 mode
    outb(PIC_DATA_MASTER, ICW4_8086);
    outb(PIC_DATA_SLAVE, ICW4_8086);

    // Masking off every interrupt since we're going to use LAPIC/IOAPIC
    outb(PIC_DATA_MASTER, 0xFF);
    outb(PIC_DATA_SLAVE, 0xFF);
}