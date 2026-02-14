#include <common/logging.h>
#include <memory/paging.h>
#include <memory/pmm.h>
#include <memory/hhdm.h>
#include <libk/stdio.h>
#include <cpu.h>
#include <stdint.h>
#include <stdbool.h>
#include <libk/string.h>
#include <interrupts/isr.h>

extern struct limine_executable_address_request executable_addr_request;
extern struct limine_hhdm_request hhdm_request;

static uint64_t *kernel_pml4_phys;

// We are going to implement 4 level paging with 4kb pages

//This function will return the page table entry
static uint64_t* vmm_get_pte(uint64_t *pml4_root, uint64_t virt_addr, bool allocate, bool is_huge)
{
    // We align the addresses to a 4096 boundary
    if(virt_addr % PAGING_PAGE_SIZE)
        virt_addr -= virt_addr % PAGING_PAGE_SIZE;

    uint64_t pml4Index, pdprIndex, pdIndex, ptIndex;

    // Extract the indexes of page directories/tables from the virtual address
    pml4Index = PAGING_GET_PML4INDEX(virt_addr);
    pdprIndex = PAGING_GET_PDPRINDEX(virt_addr);
    pdIndex = PAGING_GET_PDINDEX(virt_addr);
    ptIndex = PAGING_GET_PTINDEX(virt_addr);

    // ************************ PML4 -> PDPR ******************************
    uint64_t *virtual_pdpr;

    if(!(pml4_root[pml4Index] & PTE_FLAG_PRESENT))
    {
        if(!allocate) return NULL;
        
        // We allocate a new page for our new pdpr
        uint64_t phys_new_pdpr = pmm_alloc(PAGING_PAGE_SIZE);
        if(!phys_new_pdpr) return NULL;

        virtual_pdpr = hhdm_physToVirt((void *)phys_new_pdpr);

        // We have to set it to zero
        memset(virtual_pdpr, 0x00, PAGING_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all
        pml4_root[pml4Index] = phys_new_pdpr | PTE_FLAG_US | PTE_FLAG_PRESENT | PTE_FLAG_RW;
    }

    // we get the addr and convert it using hhdm
    virtual_pdpr = hhdm_physToVirt((void *)(pml4_root[pml4Index] & PAGING_PTE_ADDR_MASK));
    
    // ************************ PDPR -> PD ********************************
    uint64_t *virtual_pd;

    if(!(virtual_pdpr[pdprIndex] & PTE_FLAG_PRESENT))
    {
        if(!allocate) return NULL;

        // We allocate a new page for our new pd
        uint64_t phys_new_pd = pmm_alloc(PAGING_PAGE_SIZE);
        if(!phys_new_pd) return NULL;

        virtual_pd = hhdm_physToVirt((void *)phys_new_pd);

        // We have to set it to zero
        memset(virtual_pd, 0x00, PAGING_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all 
        virtual_pdpr[pdprIndex] = phys_new_pd | PTE_FLAG_US | PTE_FLAG_PRESENT | PTE_FLAG_RW;
    }
    // else we get the addr and convert it using hhdm
    virtual_pd = hhdm_physToVirt((void *)(virtual_pdpr[pdprIndex] & PAGING_PTE_ADDR_MASK));
    
    // If the page is huge we stop here
    if(is_huge) return &virtual_pd[pdIndex];

    // ************************ PD -> PT ********************************
    uint64_t *virtual_pt;
    // If the pd entry doesn't exist we must create it
    if(!(virtual_pd[pdIndex] & PTE_FLAG_PRESENT))
    {
        if(!allocate) return NULL;

        // We allocate a new page for our new pt
        uint64_t phys_new_pt = pmm_alloc(PAGING_PAGE_SIZE);
        if(!phys_new_pt) return NULL;

        virtual_pt = hhdm_physToVirt((void *)phys_new_pt);

        // We have to set it to zero
        memset(virtual_pt, 0x00, PAGING_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all 
        virtual_pd[pdIndex] = phys_new_pt | PTE_FLAG_US | PTE_FLAG_PRESENT | PTE_FLAG_RW;
    }

    virtual_pt = hhdm_physToVirt((void *)(virtual_pd[pdIndex] & PAGING_PTE_ADDR_MASK));
    return &virtual_pt[ptIndex];
}

/*
    This function takes a virtual address and maps the page associated to it to the physical page 
    associated with the physical address
*/
void paging_map_page(uint64_t *pml4_root, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, bool isHugePage)
{
    // We align the addresses to a 2MB boundary
    if(isHugePage && phys_addr % PAGING_HUGE_PAGE_SIZE)
        phys_addr -= phys_addr % PAGING_HUGE_PAGE_SIZE;
    // We align the addresses to a 4KB boundary
    else if(phys_addr % PAGING_PAGE_SIZE)
        phys_addr -= phys_addr % PAGING_PAGE_SIZE;
    
    // The root MUST point to a valid address and we won't map to page zero
    if(!pml4_root || !virt_addr)
    {
        log_log_line(LOG_ERROR, "%s: Error address invalid", __FUNCTION__);
        hcf();
    }

    // Allocate the page tables
    uint64_t *pte = vmm_get_pte(pml4_root, virt_addr, true, isHugePage);
    if(!pte)
    {
        log_log_line(LOG_ERROR, "%s: Cannot map new page %llx: OOP", __FUNCTION__, virt_addr);
        hcf();
    }

    // Set the new page table entry
    *pte = phys_addr | flags | PTE_FLAG_PRESENT | (isHugePage ? PTE_FLAG_PS : 0);

    // Invalidate the corresponding tlb entry
    asm volatile("invlpg (%0)" :: "r" (virt_addr) : "memory");
}

// Unmaps a page from the virtual address space
void paging_unmap_page(uint64_t *pml4_root, uint64_t virt_addr, bool isHugePage)
{
    if(!pml4_root || !virt_addr)
    {
        log_log_line(LOG_ERROR, "%s: Error address invalid", __FUNCTION__);
        hcf();
    }

    uint64_t *pte = vmm_get_pte(pml4_root, virt_addr, false, isHugePage);
    if(!pte) return; // It's already unmapped

    uint64_t physAddr = *pte & PAGING_PTE_ADDR_MASK;
    *pte = 0; // We zero the pte

    // Invalidate the tlb entry
    asm volatile("invlpg (%0)" :: "r" (virt_addr) : "memory");

    // Decrement the number of references to the physical page
    pmm_page_dec_ref(physAddr);    
}

// This function changes the flags of the corresponding pte
// only if the page is marked present
void paging_change_page_flags(uint64_t *pml4_root, uint64_t virt_addr, uint64_t flags, bool isHugePage)
{
    if(!pml4_root || !virt_addr)
    {
        log_log_line(LOG_ERROR, "%s: Error address invalid", __FUNCTION__);
        hcf();
    }

    uint64_t *pte = vmm_get_pte(pml4_root, virt_addr, false, isHugePage);
    if(!pte) return; // If it's not present we return

    // substitute the flags
    *pte = (*pte & PAGING_PTE_ADDR_MASK) | PTE_FLAG_PRESENT | flags;

    // Invalidate the tlb entry
    asm volatile("invlpg (%0)" :: "r" (virt_addr) : "memory");
}

// This function maps a physically contiguos region into a virtually contiguos region
void paging_map_region(uint64_t *pml4_root, uint64_t virt_addr, uint64_t phys_addr, uint64_t size, uint64_t flags, bool isHugePage)
{
    uint64_t virtual, physical;
    for(virtual = virt_addr, physical = phys_addr; virtual < virt_addr + size; isHugePage ? 
        (virtual += PAGING_HUGE_PAGE_SIZE, physical += PAGING_HUGE_PAGE_SIZE) :
        (virtual += PAGING_PAGE_SIZE, physical += PAGING_PAGE_SIZE))
    {
        paging_map_page(pml4_root, virtual, physical, flags, isHugePage);
    }
    log_log_line(LOG_DEBUG, "%s: Memory region mapped\n\tvirtual range: 0x%llx - 0x%llx\n\tphysical range: virtual range: 0x%llx - 0x%llx", 
        __FUNCTION__, virt_addr, virtual, phys_addr, physical);
}

void paging_unmap_region(uint64_t *pml4_root, uint64_t virt_addr, uint64_t size, bool isHugePage)
{
    uint64_t virtual;
    for(virtual = virt_addr; virtual < virt_addr + size; isHugePage ? 
        (virtual += PAGING_HUGE_PAGE_SIZE) : 
        (virtual += PAGING_PAGE_SIZE))
    {
        paging_unmap_page(pml4_root, virtual, isHugePage);
    }
    log_log_line(LOG_DEBUG, "%s: Memory region unmapped\n\tvirtual range: 0x%llx - 0x%llx\n", 
        __FUNCTION__, virt_addr, virtual);
}

// This function should be called at the start to initialize the vmm
void paging_init(void)
{
    // Allocate the kernel pml4
    kernel_pml4_phys = (uint64_t *) pmm_alloc(PAGING_PAGE_SIZE);
    if(!kernel_pml4_phys)
    {
        log_log_line(LOG_ERROR, "%s: Cannot allocate the kernel PML4", __FUNCTION__);
        hcf();
    }
    memset(hhdm_physToVirt((void *)kernel_pml4_phys), 0x00, PAGING_PAGE_SIZE);
    
    log_log_line(LOG_DEBUG, "%s: Created the kernel_pml4_phys and zeroed it; physical address: 0x%llx", __FUNCTION__, kernel_pml4_phys);

    
    // ************ Kernel mapping ****************

    // Import the linker script symbols
    extern char _KERNEL_START, _KERNEL_END, 
        _LIMINE_REQUESTS_START, _LIMINE_REQUESTS_END,
        _TEXT_START, _TEXT_END,
        _RODATA_START, _RODATA_END,
        _DATA_START, _DATA_END;

    uint64_t k_phys = executable_addr_request.response->physical_base;
    
    // Map the limine requests segment (Read + Write)
    paging_map_region(hhdm_physToVirt(kernel_pml4_phys), 
        (uint64_t)&_LIMINE_REQUESTS_START, 
        k_phys + ((uint64_t)&_LIMINE_REQUESTS_START - (uint64_t) &_KERNEL_START), 
        (uint64_t)&_LIMINE_REQUESTS_END - (uint64_t)&_LIMINE_REQUESTS_START, 
        PTE_FLAG_RW | PTE_FLAG_NO_EXEC | PTE_FLAG_GLOBAL, false);
    
    // Map the code segment (Read + Exec)
    paging_map_region(hhdm_physToVirt(kernel_pml4_phys), 
        (uint64_t)&_TEXT_START, 
        k_phys + ((uint64_t)&_TEXT_START - (uint64_t) &_KERNEL_START), 
        (uint64_t)&_TEXT_END - (uint64_t)&_TEXT_START, 
        PTE_FLAG_GLOBAL, false);

    // Map the rodata segment (Read)
    paging_map_region(hhdm_physToVirt(kernel_pml4_phys), 
        (uint64_t)&_RODATA_START, 
        k_phys + ((uint64_t)&_RODATA_START - (uint64_t) &_KERNEL_START), 
        (uint64_t)&_RODATA_END - (uint64_t)&_RODATA_START, 
        PTE_FLAG_NO_EXEC | PTE_FLAG_GLOBAL, false);

    // Map the data segment (Read + Write)
    paging_map_region(hhdm_physToVirt(kernel_pml4_phys), 
        (uint64_t)&_DATA_START, 
        k_phys + ((uint64_t)&_DATA_START - (uint64_t) &_KERNEL_START), 
        (uint64_t)&_DATA_END - (uint64_t)&_DATA_START, 
        PTE_FLAG_NO_EXEC | PTE_FLAG_RW | PTE_FLAG_GLOBAL, false);

    // ************ HHDM mapping ****************

    // Mapping all RAM to HHDM offset
    uint64_t phys_highestAddr = pmm_getHighestAddr();

    paging_map_region(hhdm_physToVirt(kernel_pml4_phys), 
        hhdm_request.response->offset, 
        0, 
        phys_highestAddr - 1, 
        PTE_FLAG_RW | PTE_FLAG_GLOBAL, true);

    // We need to enable global pages
    uint64_t cr4 = read_cr4();
    if (!(cr4 & CR4_PGE_BIT)) 
    {
        cr4 |= CR4_PGE_BIT;
        write_cr4(cr4);
        log_log_line(LOG_DEBUG, "%s: CR4 Global Pages (PGE) Enabled", __FUNCTION__);
    }

    paging_switch_context(kernel_pml4_phys);
    log_log_line(LOG_SUCCESS, "%s: Switched to kernel pml4.", __FUNCTION__);
}

// Switches the page table root by updating the cr3 register
inline void paging_switch_context(uint64_t *kernel_pml4_phys)
{
    asm volatile("mov %0, %%cr3" :: "r"(kernel_pml4_phys) : "memory");
}

// Returns the physical address of the pointer to the pdl4 of the kernel
uint64_t *paging_getKernelRoot(void)
{
    return kernel_pml4_phys;
}