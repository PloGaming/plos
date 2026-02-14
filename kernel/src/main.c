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

    log_log_line(LOG_DEBUG, "=== TEST DEMAND PAGING ===");

    // 1. Alloca 4KB Lazy
    // PMM Usage non deve aumentare qui.
    uint64_t *ptr = vmm_alloc(vmm_get_kernel_vas(), 4096, VMM_FLAGS_READ | VMM_FLAGS_WRITE, 0);
    
    log_log_line(LOG_DEBUG, "Allocated Virt: 0x%llx. Trying to write...", ptr);

    // 2. SCRITTURA (Il momento della verità)
    // Qui la CPU lancia INT 14 -> vmm_page_fault_handler -> pmm_alloc -> map -> return
    *ptr = 12345; 

    // 3. Se arrivi qui, ha funzionato!
    log_log_line(LOG_SUCCESS, "Write Success! Value: %d", *ptr);
    log_log_line(LOG_DEBUG, "=== TEST PASSED ===");

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
