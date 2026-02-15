#include "flanterm_backends/fb.h"
#include <flanterm.h>
#include <memory/kheap.h>
#include <memory/paging.h>
#include <memory/vmm.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <cpu.h>
#include <drivers/acpi.h>
#include <memory/gdt/gdt.h>
#include <interrupts/idt.h>
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

    log_init(LOG_SERIAL);
    
    gdt_init();
    idt_init();

    pmm_printUsableRegions();
    pmm_init();
    pmm_dump_state();
    paging_init();
    kheap_init();
    vmm_init();

    if(!acpi_set_correct_RSDT())
    {
        log_log_line(LOG_ERROR, "Error, cannot initialize RSDT");
        hcf();
    }


    // We're done, just hang...
    hcf();
}
