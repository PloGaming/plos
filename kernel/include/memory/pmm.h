#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stdbool.h>
#include <common/dll.h>

#define PMM_PAGE_SIZE 4096 //< The initial size of each page

#define PMM_MAX_ORDER       11 // Maximum buddy size 2^22 = 4MB

/**
 * @name PMM page type
 * @{
 */
#define PMM_FLAG_FREE       0
#define PMM_FLAG_USED       1
/** @} */

/**
 * @brief A node that describes a physical memory region 
 * This region is 2^(12 + order) bytes long, it has a reference count
 * because multiple things can reference this page at once.
 * When the ref_count drops to zero we can safely free the region
 */
struct pmm_page {
    uint32_t flags; ///< The attributes of the page (free, occupied, etc..)
    uint32_t ref_count; ///< The number of references to the page (once it hits zero we can free it)
    uint32_t order; ///< The dimension of the page size
    struct double_ll_node link; ///< The link to our free areas list
};

/**
 * @brief A list of physical memory region of order x
 * It provides an head to the first node and how many there are
 */
struct free_area {
    struct double_ll_node head; ///< List of free pages of x order
    uint64_t nr_free; ///< How many free blocks do we have
};

void pmm_init();
uint64_t pmm_alloc(uint64_t size);
void pmm_free(uint64_t physAddr, uint64_t length);
uint64_t pmm_getHighestAddr(void);
void pmm_page_inc_ref(uint64_t phys);
void pmm_page_dec_ref(uint64_t phys);
void pmm_dump_state(void);
void pmm_printUsableRegions();

#endif // PMM_H