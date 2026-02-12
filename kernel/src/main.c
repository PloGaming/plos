#include <memory/kheap.h>
#include <memory/paging.h>
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
#include <memory/hhdm.h>

extern struct limine_framebuffer_request framebuffer_request;

// This is our kernel's entry point.
void kmain(void) {

    limine_verify_requests();

    init_serial();
    log_init(LOG_SERIAL);
    
    gdt_initialize_gdtTable();
    idt_initialize_idtTable();
    
    pic_disable();

    pmm_printUsableRegions();
    pmm_initialize();
    pmm_dump_state();
    paging_init();
    kheap_init();

    void *a = kmalloc(100);
    kheap_print_nodes();
    void *b = kmalloc(100);
    kheap_print_nodes();
    void *c = kmalloc(100);
    kheap_print_nodes();

    // Heap: [A][B][C]...

    kfree(a); 
    kheap_print_nodes();
    // Heap: [Free][B][C]...

    kfree(c);
    kheap_print_nodes();
    // Heap: [Free][B][Free]...

    kfree(b);
    kheap_print_nodes();

    if(!acpi_set_correct_RSDT())
    {
        log_log_line(LOG_ERROR, "Error, cannot initialize RSDT");
        hcf();
    }

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
