#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <hal/cpu.h>
#include <hal/devices.h>

extern uint64_t limine_base_revision[];
extern struct limine_framebuffer_request framebuffer_request;
extern struct limine_rsdp_request rsdp_request;

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hal_hcf();
    }

    // Ensure we got the rsdp
    if(rsdp_request.response == NULL)
    {
        hal_hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hal_hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    hal_initialize_cpu();
    hal_initialize_devices();
    
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    int a = 1;
    int b = 0;
    int c = a / b;

    // We're done, just hang...
    hal_hcf();
}
