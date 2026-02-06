#include <common/logging.h>
#include <memory/vmm.h>
#include <memory/pmm.h>
#include <memory/hhdm.h>
#include <libk/stdio.h>
#include <cpu.h>
#include <stdint.h>
#include <stdbool.h>
#include <libk/string.h>

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

    log_logLine(LOG_SUCCESS, "%s: Page mapped successfully.\n\tvirt: %llx -> phys: %llx", virt_addr, phys_addr);
}

// This function should be called at the start to initialize the vmm
void vmm_init()
{

}