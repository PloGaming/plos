#include <common/logging.h>
#include <memory/vmm.h>
#include <memory/pmm.h>
#include <memory/hhdm.h>
#include <libk/stdio.h>
#include <cpu.h>
#include <stdint.h>
#include <stdbool.h>
#include <libk/string.h>

extern struct limine_executable_address_request executable_addr_request;
extern struct limine_hhdm_request hhdm_request;

static uint64_t *kernel_pml4_phys;

// We are going to implement 4 level paging with 4kb pages

static uint64_t* vmm_get_pte(uint64_t *pml4_root, uint64_t virt_addr, bool allocate, bool is_huge)
{
    // We align the addresses to a 4096 boundary
    if(virt_addr % VMM_PAGE_SIZE)
        virt_addr -= virt_addr % VMM_PAGE_SIZE;

    uint64_t pml4Index, pdprIndex, pdIndex, ptIndex;

    // Extract the indexes of page directories/tables from the virtual address
    pml4Index = VMM_GET_PML4INDEX(virt_addr);
    pdprIndex = VMM_GET_PDPRINDEX(virt_addr);
    pdIndex = VMM_GET_PDINDEX(virt_addr);
    ptIndex = VMM_GET_PTINDEX(virt_addr);

    // ************************ PML4 -> PDPR ******************************
    uint64_t *virtual_pdpr;

    if(!(pml4_root[pml4Index] & PTE_FLAG_PRESENT))
    {
        if(!allocate) return NULL;
        
        // We allocate a new page for our new pdpr
        uint64_t phys_new_pdpr = pmm_alloc(VMM_PAGE_SIZE);
        if(!phys_new_pdpr) return NULL;

        virtual_pdpr = hhdm_physToVirt((void *)phys_new_pdpr);

        // We have to set it to zero
        memset(virtual_pdpr, 0x00, VMM_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all
        pml4_root[pml4Index] = phys_new_pdpr | PTE_FLAG_US | PTE_FLAG_PRESENT | PTE_FLAG_RW;
    }

    // we get the addr and convert it using hhdm
    virtual_pdpr = hhdm_physToVirt((void *)(pml4_root[pml4Index] & VMM_PTE_ADDR_MASK));
    
    // ************************ PDPR -> PD ********************************
    uint64_t *virtual_pd;

    if(!(virtual_pdpr[pdprIndex] & PTE_FLAG_PRESENT))
    {
        if(!allocate) return NULL;

        // We allocate a new page for our new pd
        uint64_t phys_new_pd = pmm_alloc(VMM_PAGE_SIZE);
        if(!phys_new_pd) return NULL;

        virtual_pd = hhdm_physToVirt((void *)phys_new_pd);

        // We have to set it to zero
        memset(virtual_pd, 0x00, VMM_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all 
        virtual_pdpr[pdprIndex] = phys_new_pd | PTE_FLAG_US | PTE_FLAG_PRESENT | PTE_FLAG_RW;
    }
    // else we get the addr and convert it using hhdm
    virtual_pd = hhdm_physToVirt((void *)(virtual_pdpr[pdprIndex] & VMM_PTE_ADDR_MASK));
    
    // If the page is huge we stop here
    if(is_huge) return &virtual_pd[pdIndex];

    // ************************ PD -> PT ********************************
    uint64_t *virtual_pt;
    // If the pd entry doesn't exist we must create it
    if(!(virtual_pd[pdIndex] & PTE_FLAG_PRESENT))
    {
        if(!allocate) return NULL;

        // We allocate a new page for our new pt
        uint64_t phys_new_pt = pmm_alloc(VMM_PAGE_SIZE);
        if(!phys_new_pt) return NULL;

        virtual_pt = hhdm_physToVirt((void *)phys_new_pt);

        // We have to set it to zero
        memset(virtual_pt, 0x00, VMM_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all 
        virtual_pd[pdIndex] = phys_new_pt | PTE_FLAG_US | PTE_FLAG_PRESENT | PTE_FLAG_RW;
    }

    virtual_pt = hhdm_physToVirt((void *)(virtual_pd[pdIndex] & VMM_PTE_ADDR_MASK));
    return &virtual_pt[ptIndex];
}

/*
    This function takes a virtual address and maps the page associated to it to the physical page 
    associated with the physical address
*/
void vmm_map_page(uint64_t *pml4_root, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, bool isHugePage)
{
    if(isHugePage)
    {
        // We align the addresses to a 2MB boundary
        if(virt_addr % VMM_HUGE_PAGE_SIZE)
            virt_addr -= virt_addr % VMM_HUGE_PAGE_SIZE;

        if(phys_addr % VMM_HUGE_PAGE_SIZE)
            phys_addr -= phys_addr % VMM_HUGE_PAGE_SIZE;
    }
    else 
    {
        // We align the addresses to a 4096 boundary
        if(virt_addr % VMM_PAGE_SIZE)
            virt_addr -= virt_addr % VMM_PAGE_SIZE;

        if(phys_addr % PMM_PAGE_SIZE)
            phys_addr -= phys_addr % PMM_PAGE_SIZE;
    }
    
    // The root MUST point to a valid address and we won't map to page zero
    if(!pml4_root || !virt_addr)
    {
        log_logLine(LOG_ERROR, "%s: Error address invalid", __FUNCTION__);
        hcf();
    }

    // Allocate the page tables
    uint64_t *pte = vmm_get_pte(hhdm_physToVirt(pml4_root), virt_addr, true, isHugePage);
    if(!pte)
    {
        log_logLine(LOG_ERROR, "%s: Cannot map new page %llx: OOP", __FUNCTION__, virt_addr);
        hcf();
    }

    // Set the new page table entry
    *pte = phys_addr | flags | PTE_FLAG_PRESENT | (isHugePage ? PTE_FLAG_PS : 0);

    // Invalidate the corresponding tlb entry
    asm volatile("invlpg (%0)" :: "r" (virt_addr) : "memory");
}

// This function should be called at the start to initialize the vmm
void vmm_init(void)
{
    // Allocate the kernel pml4
    kernel_pml4_phys = (uint64_t *) pmm_alloc(VMM_PAGE_SIZE);
    if(!kernel_pml4_phys)
    {
        log_logLine(LOG_ERROR, "%s: Cannot allocate the kernel PML4", __FUNCTION__);
        hcf();
    }
    memset(hhdm_physToVirt((void *)kernel_pml4_phys), 0x00, VMM_PAGE_SIZE);
    
    log_logLine(LOG_DEBUG, "%s: Created the kernel_pml4_phys and zeroed it; physical address: 0x%llx", __FUNCTION__, kernel_pml4_phys);

    
    // ************ Kernel mapping ****************
    uint64_t virtual, physical;

    // Import the linker script symbols
    extern char _KERNEL_START, _KERNEL_END, 
        _LIMINE_REQUESTS_START, _LIMINE_REQUESTS_END,
        _TEXT_START, _TEXT_END,
        _RODATA_START, _RODATA_END,
        _DATA_START, _DATA_END;

    uint64_t k_phys = executable_addr_request.response->physical_base;
    
    // Map the limine requests segment (Read + Write)
    uint64_t pte_flags = PTE_FLAG_RW | PTE_FLAG_NO_EXEC | PTE_FLAG_GLOBAL;
    for(virtual = (uint64_t)&_LIMINE_REQUESTS_START; virtual <= (uint64_t)&_LIMINE_REQUESTS_END; virtual += VMM_PAGE_SIZE)
    {
        uint64_t offset = virtual - (uint64_t) &_KERNEL_START;
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, k_phys + offset, pte_flags, false);
    }
    log_logLine(LOG_DEBUG, "%s: limine requests mapped\n\tvirtual range: 0x%llx - 0x%llx", 
        __FUNCTION__, &_LIMINE_REQUESTS_START, &_LIMINE_REQUESTS_END);
    
    // Map the code segment (Read + Exec)
    pte_flags = PTE_FLAG_GLOBAL;
    for(virtual = (uint64_t)&_TEXT_START; virtual <= (uint64_t)&_TEXT_END; virtual += VMM_PAGE_SIZE)
    {
        uint64_t offset = virtual - (uint64_t) &_KERNEL_START;
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, k_phys + offset, pte_flags, false);
    }
    log_logLine(LOG_DEBUG, "%s: Code segment mapped\n\tvirtual range: 0x%llx - 0x%llx", 
        __FUNCTION__, &_TEXT_START, &_TEXT_END);

    // Map the rodata segment (Read)
    pte_flags = PTE_FLAG_NO_EXEC | PTE_FLAG_GLOBAL;
    for(virtual = (uint64_t)&_RODATA_START; virtual <= (uint64_t)&_RODATA_END; virtual += VMM_PAGE_SIZE)
    {
        uint64_t offset = virtual - (uint64_t) &_KERNEL_START;
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, k_phys + offset, pte_flags, false);
    }
    log_logLine(LOG_DEBUG, "%s: Code segment mapped\n\tvirtual range: 0x%llx - 0x%llx", 
        __FUNCTION__, &_RODATA_START, &_RODATA_END);

    // Map the data segment (Read + Write)
    pte_flags = PTE_FLAG_NO_EXEC | PTE_FLAG_RW | PTE_FLAG_GLOBAL;
    for(virtual = (uint64_t)&_DATA_START; virtual <= (uint64_t)&_DATA_END; virtual += VMM_PAGE_SIZE)
    {
        uint64_t offset = virtual - (uint64_t) &_KERNEL_START;
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, k_phys + offset, pte_flags, false);
    }
    log_logLine(LOG_DEBUG, "%s: Code segment mapped\n\tvirtual range: 0x%llx - 0x%llx", 
        __FUNCTION__, &_DATA_START, &_DATA_END);

    // ************ HHDM mapping ****************

    // Mapping all RAM to HHDM offset
    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t phys_highestAddr = pmm_getHighestAddr();

    // Map the RAM using huge pages
    for(virtual = hhdm_offset, physical = 0; physical < phys_highestAddr; virtual += VMM_HUGE_PAGE_SIZE, physical += VMM_HUGE_PAGE_SIZE)
    {
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, physical, PTE_FLAG_RW | PTE_FLAG_GLOBAL, true);
    }
    log_logLine(LOG_DEBUG, "%s: RAM mapped \n\tvirtual range: 0x%llx - 0x%llx\n\tphysical range: 0x%llx, - 0x%llx", __FUNCTION__, hhdm_offset, virtual,  0ull, physical);

    // We need to enable global pages
    uint64_t cr4 = read_cr4();
    if (!(cr4 & CR4_PGE_BIT)) 
    {
        cr4 |= CR4_PGE_BIT;
        write_cr4(cr4);
        log_logLine(LOG_DEBUG, "%s: CR4 Global Pages (PGE) Enabled", __FUNCTION__);
    }

    vmm_switchContext(kernel_pml4_phys);
    log_logLine(LOG_SUCCESS, "%s: Switched to kernel pml4.", __FUNCTION__);
}

inline void vmm_switchContext(uint64_t *kernel_pml4_phys)
{
    asm volatile("mov %0, %%cr3" :: "r"(kernel_pml4_phys) : "memory");
}

uint64_t *vmm_getKernelRoot(void)
{
    return kernel_pml4_phys;
}