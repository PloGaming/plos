#include <common/logging.h>
#include <cpu.h>
#include <memory/hhdm.h>
#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdint.h>

extern char _KERNEL_END;
static uint64_t kheap_start;

// This function will initialize the kernel heap
void kheap_init(void)
{
    // We place the kernel heap 1MB after the end of our kernel
    kheap_start = (uint64_t)&_KERNEL_END + 0x100000;
    
    // Align it to the next virtual page
    if(kheap_start % VMM_PAGE_SIZE)
    {
        kheap_start += VMM_PAGE_SIZE - (kheap_start % VMM_PAGE_SIZE);
    }

    // The total number of pages we need to allocate for our kernel heap
    uint64_t numPages = KHEAP_STARTING_SIZE / VMM_PAGE_SIZE;
    if(KHEAP_STARTING_SIZE % VMM_PAGE_SIZE) numPages++;

    uint64_t *kernel_pml4_phys = vmm_getKernelRoot();
    
    // Start the mapping
    uint64_t virtual;
    for(virtual = kheap_start; virtual < kheap_start + (numPages * VMM_PAGE_SIZE); virtual += VMM_PAGE_SIZE)
    {
        // Allocate a new page
        uint64_t newPage = pmm_alloc(VMM_PAGE_SIZE);
        if(!newPage)
        {
            log_logLine(LOG_ERROR, "%s: Cannot allocate memory for kernel heap", __FUNCTION__);
            hcf();
        }
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, newPage, PTE_FLAG_RW | PTE_FLAG_GLOBAL, false);
    }

    log_logLine(LOG_SUCCESS, "%s: Kernel heap initialized\n\tVirtual range: 0x%llx - 0x%llx", __FUNCTION__, kheap_start, virtual);
}
