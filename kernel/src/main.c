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
#include <scheduling/lock.h>

extern struct task *task_current;
extern struct thread *thread_current;

void stress_test_worker() 
{
    uint64_t my_tid = thread_current->tid;

    log_line(LOG_DEBUG, "[TID %llu] Iniziando lo stress test...", my_tid);

    // Facciamo 3 cicli di stress per ogni thread
    for(int i = 0; i < 3; i++) 
    {
        // 1. STRESS KHEAP E MUTEX
        // Alloca dinamicamente. Se 5 thread lo fanno, 4 dormiranno sul Mutex.
        uint8_t *heap_buf = kmalloc(2048);
        if(heap_buf) {
            memset(heap_buf, 0xAA, 2048); // Scriviamo per verificare la memoria
        }

        // 2. STRESS VMM, PMM E PAGE FAULT (Spinlock IRQ-Safe)
        // Allochiamo una pagina virtuale "Lazy" (ANON)
        uint8_t *virt_buf = vmm_alloc(task_current->vas, PAGING_PAGE_SIZE, VMM_FLAGS_READ | VMM_FLAGS_WRITE | VMM_FLAGS_ANON, 0);
        if(virt_buf) {
            // ATTENZIONE: Questa riga CAUSA UN PAGE FAULT VOLONTARIO!
            // L'hardware interromperà il thread, chiamerà l'ISR, l'ISR prenderà
            // il lock del VMM, poi il lock del PMM, allocherà la pagina, aggiornerà
            // le page tables e restituirà il controllo. Tutto in modo invisibile!
            virt_buf[0] = 42; 
            virt_buf[4095] = 24;
            
            // Liberiamo la pagina virtuale (e indirettamente quella fisica)
            vmm_free(task_current->vas, (uint64_t)virt_buf);
        }

        // Liberiamo l'heap
        if(heap_buf) kfree(heap_buf);

        // 3. STRESS LOGGING E TERMINALE (Mutex)
        log_line(LOG_DEBUG, "[TID %llu] Ciclo %d completato. Vado a dormire...", my_tid, i);

        // 4. STRESS SCHEDULER (Spinlock + Timer ISR)
        // Cediamo la CPU volontariamente per costringere lo scheduler a lavorare
        thread_sleep(50); // Dorme 50ms
    }

    log_line(LOG_SUCCESS, "[TID %llu] Test completato! Divento zombie.", my_tid);
    
    // Uscendo dalla funzione, il thread salterà a task_current_thread_exit.
    // L'Idle Thread si sveglierà e farà il Reaper testando un'ultima volta i Lock!
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

   /**************************** TEST ******************************/
   log_line(LOG_DEBUG, "Avvio Stress Test Concorrenza...");

   // Creiamo il Processo A e gli diamo 3 thread
   struct task *process_a = task_create("Processo A");
   task_create_thread(process_a, stress_test_worker);
   task_create_thread(process_a, stress_test_worker);
   task_create_thread(process_a, stress_test_worker);

   // Creiamo il Processo B e gli diamo 3 thread
   struct task *process_b = task_create("Processo B");
   task_create_thread(process_b, stress_test_worker);
   task_create_thread(process_b, stress_test_worker);
   task_create_thread(process_b, stress_test_worker);
   /**************************** END TEST ******************************/
    
    asm volatile ("sti");

    kernel_idle_thread();
}