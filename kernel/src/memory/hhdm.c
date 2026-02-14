#include <common/logging.h>
#include <memory/hhdm.h>
#include <limine.h>
#include <stdint.h>

extern struct limine_hhdm_request hhdm_request;

/**
 * @brief Converts a physical address to a virtual one in the hhdm region
 * Essentially the full RAM is direct mapped into the VAS (since it's so large).
 * So it's really easy to access the physical memory directly. 
 * The function simply adds to the physical address the offset the hhdm is mapped to
 * @param physical_addr The physical address we want to convert to virtual
 * @return void* The virtual address corresponding to the physical one
 */
void* hhdm_physToVirt(void *physical_addr)
{
    return (void *)((uint64_t )physical_addr + hhdm_request.response->offset);
}

/**
 * @brief Converts a hhdm virtual address to its relative physical 
 * 
 * @param virtual_addr a virtual address belonging to the hhdm region
 * @return void* the physical address corresponding to the physical one
 */
void* hhdm_virtToPhys(void *virtual_addr)
{
    return (void *)((uint64_t )virtual_addr - hhdm_request.response->offset);
}