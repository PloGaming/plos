#include <stdint.h>
#include <hal/devices.h>
#include "drivers/serial.h"
#include "drivers/pic.h"

void hal_initialize_devices(void)
{
    init_serial();
    disable_pic();
}

// Prints a string to the serial output
void log_to_serial(char *string) {
    for(uint32_t i = 0; string[i] != '\0'; i++)
    {
        write_serial(string[i]);
    }
}