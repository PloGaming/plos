#include <memory/vmm.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <cpu.h>
#include <drivers/acpi.h>
#include <memory/gdt/gdt.h>
#include <interrupts/idt.h>
#include <drivers/pic.h>
#include <drivers/serial.h>
#include <memory/pmm.h>
#include <libk/string.h>
#include <common/logging.h>
#include <limine_requests.h>

extern uint64_t limine_base_revision[];
extern struct limine_framebuffer_request framebuffer_request;
extern struct limine_rsdp_request rsdp_request;
extern struct limine_hhdm_request hhdm_request;
extern struct limine_memmap_request memmap_request;

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {

    limine_verify_requests();

    init_serial();
    log_init(LOG_SERIAL);
    gdt_initialize_gdtTable();
    idt_initialize_idtTable();
    
    disable_pic();

    pmm_initialize(memmap_request.response);
    vmm_init();

    /*if(!acpi_set_correct_RSDT(rsdp_request.response->address))
    {
        log_to_serial("[ERROR] The RSDP is invalid\n");
        hcf();
    }
    */

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // We assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    // We're done, just hang...
    hcf();
}
