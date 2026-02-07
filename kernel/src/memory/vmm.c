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
extern char _KERNEL_START;
extern char _KERNEL_END;

static uint64_t *kernel_pml4_phys;

// We are going to implement 4 level paging with 4kb pages

/*
    This function takes a virtual address and maps the page associated to it to the physical page 
    associated with the physical address
*/
void vmm_map_page(uint64_t *pml4_root, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, bool isHugePage)
{
    // We align the addresses to a 4096 boundary
    if(virt_addr % VMM_PAGE_SIZE)
        virt_addr -= virt_addr % VMM_PAGE_SIZE;

    if(phys_addr % PMM_PAGE_SIZE)
        phys_addr -= phys_addr % PMM_PAGE_SIZE;

    // The root MUST point to a valid address and we won't map to page zero
    if(!pml4_root || !virt_addr)
    {
        log_logLine(LOG_ERROR, "%s: Error address invalid", __FUNCTION__);
        hcf();
    }

    uint64_t pml4Index, pdprIndex, pdIndex, ptIndex;

    // Extract the indexes of page directories/tables from the virtual address
    pml4Index = VMM_GET_PML4INDEX(virt_addr);
    pdprIndex = VMM_GET_PDPRINDEX(virt_addr);
    pdIndex = VMM_GET_PDINDEX(virt_addr);
    ptIndex = VMM_GET_PTINDEX(virt_addr);
    
    // ************************ PML4 -> PDPR ******************************
    uint64_t *virtual_pdpr;
    // If the pml4 entry doesn't exist we must create it
    if(!(pml4_root[pml4Index] & PTE_FLAG_PRESENT))
    {
        // We allocate a new page for our new pdpr
        uint64_t phys_new_pdpr = pmm_alloc(VMM_PAGE_SIZE);
        if(!phys_new_pdpr)
        {
            log_logLine(LOG_ERROR, "%s: Error while allocating PDPR page", __FUNCTION__);
            hcf();
        }

        virtual_pdpr = hhdm_physToVirt((void *)phys_new_pdpr);

        // We have to set it to zero
        memset(virtual_pdpr, 0x00, VMM_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all 
        pml4_root[pml4Index] = phys_new_pdpr | PTE_FLAG_PRESENT | PTE_FLAG_RW;
        
        // If the page is accessible by the user then also the previous directory need to have this flag
        if(flags & PTE_FLAG_US)
        {
            pml4_root[pml4Index] |= PTE_FLAG_US;
        }
    }
    else 
    {
        // else we get the addr and convert it using hhdm
        virtual_pdpr = hhdm_physToVirt((void *)(pml4_root[pml4Index] & VMM_PTE_ADDR_MASK));
    }

    // ************************ PDPR -> PD ********************************
    uint64_t *virtual_pd;
    // If the pdpr entry doesn't exist we must create it
    if(!(virtual_pdpr[pdprIndex] & PTE_FLAG_PRESENT))
    {
        // We allocate a new page for our new pd
        uint64_t phys_new_pd = pmm_alloc(VMM_PAGE_SIZE);
        if(!phys_new_pd)
        {
            log_logLine(LOG_ERROR, "%s: Error while allocating PD page", __FUNCTION__);
            hcf();
        }

        virtual_pd = hhdm_physToVirt((void *)phys_new_pd);

        // We have to set it to zero
        memset(virtual_pd, 0x00, VMM_PAGE_SIZE);

        // We set the directory entry as present, readable and writable by all 
        virtual_pdpr[pdprIndex] = phys_new_pd | PTE_FLAG_PRESENT | PTE_FLAG_RW;
        
        // If the page is accessible by the user then also the previous directory need to have this flag
        if(flags & PTE_FLAG_US)
        {
            virtual_pdpr[pdprIndex] |= PTE_FLAG_US;
        }
    }
    else 
    {
        // else we get the addr and convert it using hhdm
        virtual_pd = hhdm_physToVirt((void *)(virtual_pdpr[pdprIndex] & VMM_PTE_ADDR_MASK));
    }

    // If the request is a huge page then we map it directly to the page directory
    if(isHugePage)
    {
        // We align the addresses to a 2MB boundary
        if(virt_addr % VMM_HUGE_PAGE_SIZE)
            virt_addr -= virt_addr % VMM_HUGE_PAGE_SIZE;

        if(phys_addr % VMM_HUGE_PAGE_SIZE)
            phys_addr -= phys_addr % VMM_HUGE_PAGE_SIZE;

        // Set the entry with the huge flag
        virtual_pd[pdIndex] = phys_addr | flags| PTE_FLAG_PRESENT | PTE_FLAG_PS;
    }
    else 
    {
        // ************************ PD -> PT ********************************
        uint64_t *virtual_pt;
        // If the pd entry doesn't exist we must create it
        if(!(virtual_pd[pdIndex] & PTE_FLAG_PRESENT))
        {
            // We allocate a new page for our new pt
            uint64_t phys_new_pt = pmm_alloc(VMM_PAGE_SIZE);
            if(!phys_new_pt)
            {
                log_logLine(LOG_ERROR, "%s: Error while allocating PT page", __FUNCTION__);
                hcf();
            }

            virtual_pt = hhdm_physToVirt((void *)phys_new_pt);

            // We have to set it to zero
            memset(virtual_pt, 0x00, VMM_PAGE_SIZE);

            // We set the directory entry as present, readable and writable by all 
            virtual_pd[pdIndex] = phys_new_pt | PTE_FLAG_PRESENT | PTE_FLAG_RW;
            
            // If the page is accessible by the user then also the previous directory need to have this flag
            if(flags & PTE_FLAG_US)
            {
                virtual_pd[pdIndex] |= PTE_FLAG_US;
            }
        }
        else 
        {
            // else we get the addr and convert it using hhdm
            virtual_pt = hhdm_physToVirt((void *)(virtual_pd[pdIndex] & VMM_PTE_ADDR_MASK));
        }

        // Here we write the corresponding page frame number to the page table
        virtual_pt[ptIndex] = phys_addr | flags | PTE_FLAG_PRESENT;
    }

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

    uint64_t k_start, k_end, k_phys;
    k_start = (uint64_t) &_KERNEL_START;
    k_end = (uint64_t) &_KERNEL_END;
    k_phys = executable_addr_request.response->physical_base;

    // Map the kernel using normal pages
    for(virtual = k_start, physical = k_phys; virtual < k_end; virtual += VMM_PAGE_SIZE, physical += VMM_PAGE_SIZE)
    {
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, physical, PTE_FLAG_RW | PTE_FLAG_GLOBAL, false);
    }

    log_logLine(LOG_DEBUG, "%s: Kernel mapped\n\tvirtual range: 0x%llx - 0x%llx\n\tphysical range: 0x%llx - 0x%llx", __FUNCTION__, k_start, k_end, k_phys, physical);

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