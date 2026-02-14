#include <interrupts/isr.h>
#include <memory/hhdm.h>
#include <memory/kheap.h>
#include <memory/paging.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <common/logging.h>
#include <cpu.h>
#include <stddef.h>
#include <stdint.h>
#include <libk/string.h>

// The kernel address space
static struct vm_address_space *kernel_vas = NULL;
static struct vm_address_space *current_vas = NULL;

void vmm_init(void)
{
    // Allocate space for our struct
    kernel_vas = kmalloc(sizeof(struct vm_address_space));
    if(!kernel_vas)
    {
        log_log_line(LOG_ERROR, "%s: Cannot allocate space for kernel_vas", __FUNCTION__);
        hcf();
    }

    // Set the base root
    kernel_vas->pml4_phys = paging_getKernelRoot();
    kernel_vas->region_list = NULL;

    // Set the current vas as the kernel
    current_vas = kernel_vas;

    log_log_line(LOG_SUCCESS, "%s: Virtual memory manager initialized", __FUNCTION__);
}

// Creates a new address space for a process
struct vm_address_space *vmm_new_address_space(void)
{
    // Allocate memory for a new address space
    struct vm_address_space *new_address_space = kmalloc(sizeof(struct vm_address_space));
    if(!new_address_space) return NULL;

    // Allocate a new physical page for the pml4
    uint64_t new_pml4 = pmm_alloc(PAGING_PAGE_SIZE);
    if(!new_pml4)
    {
        kfree(new_address_space);
        return NULL;
    }

    // Set the correct fields
    new_address_space->pml4_phys = (uint64_t *) new_pml4;
    new_address_space->region_list = NULL;

    // Set all the entries as non present
    uint64_t *virt_new_pml4 = hhdm_physToVirt((void *) new_pml4);
    memset(virt_new_pml4, 0x00, PAGING_PAGE_SIZE);


    // Copy the higher half since the kernel and everything else should be always mapped into every VAS
    uint64_t *virt_kernel_pml4 = hhdm_physToVirt(kernel_vas->pml4_phys);
    for(size_t i = 256; i < 512; i++)
    {
        virt_new_pml4[i] = virt_kernel_pml4[i];
    }

    return new_address_space;
}

// Our vmm allocator, this function finds an available region in the virtual address space
// passed by argument and allocates it
void *vmm_alloc(struct vm_address_space *space, uint64_t size, uint64_t flags, uint64_t arg)
{
    // Align up the size
    if(size % PAGING_PAGE_SIZE) size += PAGING_PAGE_SIZE - (size % PAGING_PAGE_SIZE);

    if(!space || size == 0) return NULL;

    // Set the correct search start and end searching address
    uint64_t region_search_start, region_search_end;
    if(space == kernel_vas)
    {
        region_search_start = VMM_KERNEL_START;
        region_search_end = VMM_KERNEL_END;

        // Remove the user flag if present
        if(flags & VMM_FLAGS_USER) flags &= ~VMM_FLAGS_USER;
    }
    else 
    {
        region_search_start = VMM_USER_START;
        region_search_end = VMM_USER_END;
    }

    // Search for a free space in the virtual address space
    struct vm_area *current = space->region_list;
    struct vm_area *prev = NULL;
    uint64_t candidate = region_search_start;

    while(current != NULL)
    {
        // We found a free hole
        if(current->base >= candidate + size)
        {
            // We must not surpass the higher half region
            if(candidate + size <= region_search_end) break;
        }

        // We position ourselves after the block
        candidate = current->base + current->size;

        // OOM virtual
        if(candidate >= region_search_end) return NULL;

        prev = current;
        current = current->next;
    }

    // Allocate the new area in the kernel heap
    struct vm_area *new_area = kmalloc(sizeof(struct vm_area));
    if(!new_area) return NULL;

    new_area->base = candidate;
    new_area->size = size;
    new_area->flags = flags;
    new_area->next = current;

    // If it's the first area
    if(prev == NULL)
    {
        space->region_list = new_area;
    }
    else 
    {
        prev->next = new_area;
    }

    // If it's mapping for memory mapped I/O we map the physical address immediately
    if(flags & VMM_FLAGS_MMIO)
    {
        uint64_t phys_base = arg;
        
        // Convert the flags from generic to x86-64 specific
        uint64_t paging_flags = vmm_generic_to_x86_flags(flags);
        
        // Map the region
        paging_map_region(hhdm_physToVirt(space->pml4_phys), 
            new_area->base, 
            phys_base, 
            size, 
            paging_flags, 
            false);
        log_log_line(LOG_DEBUG, "%s: MMIO Mapped v=0x%llx -> p=0x%llx", __FUNCTION__, candidate, phys_base);
    }
    else 
    {
        // Demanding paging
        // We do nothing, the page fault handler will load the page
        // as soon as the prcess accesses those addresses
        log_log_line(LOG_DEBUG, "VMM: Lazy Allocation at v=0x%llx (Phys: None yet)", candidate);
    }

    return (void *) new_area->base;
}

// Function for returning the area a virtual address belongs to
struct vm_area *vmm_get_vm_area(struct vm_address_space *space, uint64_t vaddr)
{
    if(!space) return NULL;

    // Iterate the areas of the VAS
    struct vm_area *current = space->region_list;
    while(current != NULL)
    {
        // If the virtual address is inside the area we return that
        if(vaddr >= current->base && vaddr < current->base + current->size)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void vmm_free(struct vm_address_space *space, uint64_t addr)
{
    if(!space || !addr) return;

    struct vm_area *current = space->region_list;
    struct vm_area *prev = NULL;

    while(current != NULL)
    {
        // We found the region
        if(current->base == addr)
        {
            // Delete it from the list
            if(prev == NULL)
            {
                space->region_list = current->next;
            }
            else 
            {
                prev->next = current->next;
            }

            // Unmap the region in the page tables
            paging_unmap_region(hhdm_physToVirt(space->pml4_phys), 
                current->base, 
                current->size,
                false);

            kfree(current);
            return;
        }

        prev = current;
        current = current->next;
    }

    log_log_line(LOG_WARN, "%s: Attempted to free an invalid region: 0x%llx", __FUNCTION__, addr);
}

void vmm_destroy_address_space(struct vm_address_space *space)
{
    if(!space || space == kernel_vas) return;

    struct vm_area *current = space->region_list;
    
    // Free each area
    while(current != NULL)
    {
        struct vm_area *next = current->next;
        vmm_free(space, current->base);
        current = next;
    }

    // Decrement the usage of that table
    pmm_page_dec_ref((uint64_t) space->pml4_phys);

    // Free the address space struct
    kfree(space);
}

// Function to convert generic flags to x86 specific
uint64_t vmm_generic_to_x86_flags(uint64_t genericFlags)
{
    uint64_t x86_flags = 0;
    if(genericFlags & VMM_FLAGS_READ) x86_flags |= 0;
    if(genericFlags & VMM_FLAGS_WRITE) x86_flags |= PTE_FLAG_RW;
    if(!(genericFlags & VMM_FLAGS_EXEC)) x86_flags |= PTE_FLAG_NO_EXEC;
    if(genericFlags & VMM_FLAGS_USER) 
        x86_flags |= PTE_FLAG_US;
    else
        x86_flags |= PTE_FLAG_GLOBAL;

    return x86_flags;
}

void vmm_page_fault_handler(struct isr_context *context)
{
    // Cr2 is the address who caused the page fault
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    bool present = context->error_code & PAGE_FAULT_PRESENT;
    bool write = context->error_code & PAGE_FAULT_WRITE;
    bool execute = context->error_code & PAGE_FAULT_INSTRUCTION_FETCH;
    bool user = context->error_code & PAGE_FAULT_USER;
    log_log_line(LOG_DEBUG, "PF at 0x%llx (Present: %d, Write: %d, Execute: %d, User: %d)", cr2, present, write, execute, user);

    struct vm_address_space *target_vas;
    if(cr2 >= VMM_KERNEL_START)
    {
        target_vas = kernel_vas;
    }
    else 
    {
        target_vas = current_vas;
    }

    struct vm_area *target_area = vmm_get_vm_area(target_vas, cr2);
    
    // No mapped memory
    if(!target_area)
    {
        log_log_line(LOG_ERROR, "SEGFAULT: Access to unmapped memory at 0x%llx (RIP: 0x%llx)", cr2, context->rip);
        // TODO: kill the offending process
        hcf();
    }

    if (present) {
        if (write && !(target_area->flags & VMM_FLAGS_WRITE)) {
            log_log_line(LOG_ERROR, "PROTECTION FAULT: Write to Read-Only memory at 0x%llx", cr2);
            hcf();
        }
        if (execute && !(target_area->flags & VMM_FLAGS_EXEC)) {
             log_log_line(LOG_ERROR, "PROTECTION FAULT: Execution of NX memory at 0x%llx", cr2);
             hcf();
        }
        
        log_log_line(LOG_ERROR, "GENERIC PROTECTION FAULT at 0x%llx", cr2);
        hcf();
    }

    // Demand paging
    uint64_t phys_page = pmm_alloc(PAGING_PAGE_SIZE);
    if(!phys_page)
    {
        log_log_line(LOG_ERROR, "%s: OOM Cannot allocate a page", __FUNCTION__);
        hcf();
    }

    // Zero the page, fundamental for security
    memset(hhdm_physToVirt((void *)phys_page), 0x00, PAGING_PAGE_SIZE);
    
    
    // Map the page
    paging_map_page(hhdm_physToVirt(target_vas->pml4_phys), 
        cr2, 
        phys_page,
        vmm_generic_to_x86_flags(target_area->flags),
        false);
    
    log_log_line(LOG_DEBUG, "%s: Recovered Fault at 0x%llx -> Alloc Phys 0x%llx", __FUNCTION__,cr2, phys_page);
}

void vmm_switch_address_space(struct vm_address_space *space)
{
    if(!space) return;

    current_vas = space;

    paging_switch_context(space->pml4_phys);
}

struct vm_address_space* vmm_get_kernel_vas()
{
    return kernel_vas;
}