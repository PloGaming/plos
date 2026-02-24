#include <interrupts/isr.h>
#include <memory/hhdm.h>
#include <memory/kheap.h>
#include <memory/paging.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <common/logging.h>
#include <cpu.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <libk/string.h>

// The kernel and the current virrtual address space
static struct vm_address_space *kernel_vas = NULL, *current_vas = NULL;

/**
 * @brief Our virtual memory manager initialization function
 * 1) Creates the kernel VAS
 * 2) Set it to be the current VAS
 */
void vmm_init(void)
{
    // Allocate space for our struct
    kernel_vas = kmalloc(sizeof(struct vm_address_space));
    if(!kernel_vas)
    {
        log_line(LOG_ERROR, "%s: Cannot allocate space for kernel_vas", __FUNCTION__);
        hcf();
    }

    // Set the base root
    kernel_vas->pml4_phys = paging_getKernelRoot();
    kernel_vas->region_list = NULL;

    // Set the current vas as the kernel
    current_vas = kernel_vas;

    log_line(LOG_SUCCESS, "%s: Virtual memory manager initialized", __FUNCTION__);
}

/**
 * @brief Creates a new address space for a process
 * 
 * @return struct vm_address_space* Pointer to the newly allocated address space
 * returns NULL if the allocation failed
 */
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
 
/**
 * @brief Our vmm allocator
 * This function finds an available region in the virtual address space
 * passed by argument and allocates it
 * @param space Pointer to a valid vm_address_space struct
 * @param size The number of bytes to allocate, aligned to next page boundary
 * @param flags Generic flags to be applied to the pages of this areas
 * @param arg The meaning of the value depends on the type of mapping we do
 * @return void* A pointer to the start of the virtual memory region newly allocated
 * NULL on failure
 */
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

        log_line(LOG_DEBUG, "%s: MMIO Mapped v=0x%llx -> p=0x%llx", __FUNCTION__, candidate, phys_base);
    }
    else if(flags & VMM_FLAGS_ANON)
    {
        // Demanding paging
        // We do nothing, the page fault handler will load the page
        // as soon as the prcess accesses those addresses
        log_line(LOG_DEBUG, "VMM: Lazy Allocation at v=0x%llx (Phys: None yet)", candidate);
    }
    else 
    {
        log_line(LOG_ERROR, "%s: Invalid user access", __FUNCTION__);
        hcf();
    }

    return (void *) new_area->base;
}

/**
 * @brief Function for returning the area a virtual address belongs to
 * 
 * @param space Pointer to a valid vm_address_space struct
 * @param vaddr The virtual address belonging to the vm area we want
 * @return struct vm_area* A pointer to the vm_area that contains vaddr
 * or NULL if the area isn't allocated
 */
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

/**
 * @brief Function for removing the page mapping of a block
 * 
 * @param space The address space we're interested in
 * @param addr The address belonging in the virtual memory region
 */
void vmm_free(struct vm_address_space *space, uint64_t addr)
{
    if(!space || !addr) return;

    struct vm_area *current = space->region_list;
    struct vm_area *prev = NULL;

    while(current != NULL)
    {
        // We found the region
        if(current->base <= addr && current->base + current->size > addr)
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
                false,
                !(current->flags & VMM_FLAGS_MMIO)); // Free the physical page only if the mapped area is not MMIO

            kfree(current);
            return;
        }

        prev = current;
        current = current->next;
    }

    log_line(LOG_WARN, "%s: Attempted to free an invalid region: 0x%llx", __FUNCTION__, addr);
}

/**
 * @brief This function free's everything about a VAS
 * 1) It unmaps every area described by the vm_area list
 * 2) It frees the vm_area structs
 * 3) It decrements the usage of the pml4
 * 4) It frees the addess space struct  
 * @param space A pointer to a valid (not the kernel) address space
 */
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

/**
 * @brief Function to convert generic paging flags to x86 specific
 * 
 * @param genericFlags The generic flags (look into memory/vmm.h)
 * @return uint64_t The architecture specific x86 flags (look into memory/paging.h)
 */
uint64_t vmm_generic_to_x86_flags(uint64_t genericFlags)
{
    uint64_t x86_flags = 0;
    if(genericFlags & VMM_FLAGS_READ) x86_flags |= 0;
    if(genericFlags & VMM_FLAGS_WRITE) x86_flags |= PTE_FLAG_RW;
    if(!(genericFlags & VMM_FLAGS_EXEC)) x86_flags |= PTE_FLAG_NO_EXEC;
    if(genericFlags & VMM_FLAGS_WC) x86_flags |= PTE_CACHE_WC;
    if(genericFlags & VMM_FLAGS_UC) x86_flags |= PTE_CACHE_UC;
    if(genericFlags & VMM_FLAGS_USER) 
        x86_flags |= PTE_FLAG_US;
    else
        x86_flags |= PTE_FLAG_GLOBAL;

    return x86_flags;
}

/**
 * @brief Our page fault handler
 * When interrupt 0xe (14) is fired that means there was a problem accessing
 * a byte inside a specific page. This problem is either resolvable (because with
 * demand paging we delay the actual physical allocation until the memory is accessed)
 * so we simply allocate a physical page through our pmm and retry executing the instruction OR
 * it was the process fault, accessing a page it shouldn't have. 
 * @param context The state of the process before firing the page fault
 */
void vmm_page_fault_handler(struct cpu_status *context)
{
    // Cr2 is the address who caused the page fault
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    // What caused the fault?
    bool present = context->error_code & PAGE_FAULT_PRESENT;
    bool write = context->error_code & PAGE_FAULT_WRITE;
    bool execute = context->error_code & PAGE_FAULT_INSTRUCTION_FETCH;
    bool user = context->error_code & PAGE_FAULT_USER;
    log_line(LOG_DEBUG, "PF at 0x%llx (Present: %d, Write: %d, Execute: %d, User: %d)", cr2, present, write, execute, user);

    // The page fault was in user space or kernel space?
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
    
    // The memory was not mapped
    if(!target_area)
    {
        log_line(LOG_ERROR, "SEGFAULT: Access to unmapped memory at 0x%llx (RIP: 0x%llx)", cr2, context->rip);
        // TODO: kill the offending process
        hcf();
    }

    // If the page was present that means it's a permission violation
    if (present) {
        if (write && !(target_area->flags & VMM_FLAGS_WRITE)) {
            log_line(LOG_ERROR, "PROTECTION FAULT: Write to Read-Only memory at 0x%llx", cr2);
            hcf();
        }
        if (execute && !(target_area->flags & VMM_FLAGS_EXEC)) {
            log_line(LOG_ERROR, "PROTECTION FAULT: Execution of NX memory at 0x%llx", cr2);
            hcf();
        }
        
        log_line(LOG_ERROR, "GENERIC PROTECTION FAULT at 0x%llx", cr2);
        hcf();
    }

    // Demand paging
    uint64_t phys_page = pmm_alloc(PAGING_PAGE_SIZE);
    if(!phys_page)
    {
        // TODO: Implement swap memory mechainsm so this never happens
        log_line(LOG_ERROR, "%s: OOM Cannot allocate a page", __FUNCTION__);
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
    
    log_line(LOG_DEBUG, "%s: Recovered Fault at 0x%llx -> Alloc Phys 0x%llx", __FUNCTION__,cr2, phys_page);
}

/**
 * @brief Switch the current address space
 * 
 * @param space A pointer to the vm_address_space we want to switch to
 */
void vmm_switch_address_space(struct vm_address_space *space)
{
    if(!space) return;

    // Set it as the current VAS
    current_vas = space;

    // Change the CR3 register
    paging_switch_context(space->pml4_phys);
}

/**
 * @brief Getter for the kernel virtual address space
 * 
 * @return struct vm_address_space* A pointer to the kernel virtual address
 * space struct
 */
struct vm_address_space* vmm_get_kernel_vas()
{
    return kernel_vas;
}