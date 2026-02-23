#include <devices/timer.h>
#include <drivers/console.h>
#include <drivers/lapic.h>
#include <flanterm.h>
#include <memory/kheap.h>
#include <memory/paging.h>
#include <memory/vmm.h>
#include <scheduling/scheduler.h>
#include <scheduling/task.h>
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
#include <libk/stdio.h>

void thread_a() {
    while(1) {
        log_line(LOG_DEBUG, "Ping dal Thread A!");
        
        scheduler_yield();
    }
}

void thread_b() {
    while(1) {
        log_line(LOG_DEBUG, "Pong dal Thread B!");
        
        for(volatile int i = 0; i < 50000000; i++);
    }
}

// This is our kernel's entry point.
void kmain(void) {

    limine_verify_requests();
    
    // Global descriptor table
    gdt_init();
    // Interrupt descriptor table
    idt_init();

    // Physical memory manager initialization
    pmm_printUsableRegions();
    pmm_init();
    pmm_dump_state();

    // Paging setup
    paging_init();

    // Kernel heap initialization
    kheap_init();

    // Virtual memory manager setup
    vmm_init();
    
    console_init();

    acpi_init();
    lapic_initialize();

    timer_init();

    scheduler_init();

    task_create("Thread a", thread_a);
    task_create("Thread b", thread_b);

    asm volatile ("sti");

    // We're done, just hang...
    hcf();
}

