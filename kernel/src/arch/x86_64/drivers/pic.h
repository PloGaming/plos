#ifndef PIC_H
#define PIC_H

#define PIC_COMMAND_MASTER  0x20
#define PIC_COMMAND_SLAVE   0xA0
#define PIC_DATA_MASTER     0x21
#define PIC_DATA_SLAVE      0xA1

#define ICW_1       0x11
#define ICW_2_M     0x20
#define ICW_2_S     0x28
#define ICW_3_M     0x4
#define ICW_3_S     0x2
#define ICW4_8086	0x01	

void disable_pic(void);

#endif