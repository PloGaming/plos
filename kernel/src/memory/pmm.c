#include <cpu.h>
#include <libk/stdio.h>
#include <memory/gdt/gdt.h>
#include <memory/pmm.h>
#include <limine.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <memory/hhdm.h>
#include <libk/string.h>
#include <common/logging.h>
#include <common/dll.h>

extern struct limine_memmap_request memmap_request;

// Array of all pages
static struct pmm_page *buddy_memmap = NULL;
static uint64_t buddy_memmap_size = 0;

// Array of free lists of each order
static struct free_area free_areas[PMM_MAX_ORDER];

// Used for statistics
static uint64_t used_pages, totalPages;

// Highest usable RAM Addr
static uint64_t highestAddr = 0;

/********************** UTILITY FUNCTIONS FOR BUDDY ***********************/

static inline struct pmm_page *pfn_to_page(uint64_t pfn)
{
    if(pfn >= totalPages) return NULL;
    return &buddy_memmap[pfn];
}

// Uses pointer arithmetic
static inline uint64_t page_to_pfn(struct pmm_page *page) { return page - buddy_memmap; }

static inline uint64_t page_to_phys(struct pmm_page *page) { return page_to_pfn(page) * PMM_PAGE_SIZE; }

static inline struct pmm_page *phys_to_page(uint64_t phys) { return pfn_to_page(phys/PMM_PAGE_SIZE); }

static inline bool is_page_free(struct pmm_page *page) { return !(page->flags & PMM_FLAG_USED); }

/*************************************************************************/

// Frees an entire block of pages of order x: 0 = 4KB
void pmm_free_pages(uint64_t phys, uint32_t order)
{
    if(phys % PMM_PAGE_SIZE)
    {
        log_log_line(LOG_WARN, "%s: Warning freeing unaligned address %llx", __FUNCTION__, phys);
        return;
    }

    // Get the page we're referring to
    uint64_t pfn = phys / PMM_PAGE_SIZE;
    struct pmm_page *page = pfn_to_page(pfn);
    if(!page) return;

    // Coalescing buddys
    while(order < PMM_MAX_ORDER - 1)
    {
        // Get the buddy page
        uint64_t buddy_pfn = pfn ^ (1 << order);
        struct pmm_page *buddy_page = pfn_to_page(buddy_pfn);
        
        // If the buddy doesn't exist or it's outside our RAM, stop
        if(!buddy_page || buddy_pfn >= totalPages) break;

        // We can merge if and only if:
        // 1) Buddy is free
        // 2) Buddy has the same order
        if(!is_page_free(buddy_page) || buddy_page->order != order) break;

        // We found a valid buddy to coalesce with

        // Remove the buddy from his free list
        dll_delete(&buddy_page->link);
        free_areas[order].nr_free--;

        // Cleanup
        buddy_page->order = 0;

        // If buddy comes before us we set it to be the new "manager"
        if(buddy_pfn < pfn)
        {
            pfn = buddy_pfn;
            page = buddy_page;
        }

        order++;
    }

    // We set the newly coalesced page as free
    page->order = order;
    page->flags = PMM_FLAG_FREE;
    page->ref_count = 0;

    // Add it to our free areas list
    dll_add_after(&free_areas[order].head, &page->link);
    free_areas[order].nr_free++;
}

// Allocates 2^order page. Returns the physical address
uint64_t pmm_alloc_pages(uint32_t order)
{
    if(order >= PMM_MAX_ORDER) return 0;

    // Search for a page >= order we search for
    uint32_t current_order;
    bool page_found = false;
    for(current_order = order; current_order < PMM_MAX_ORDER; current_order++)
    {
        // Has this free list at least one block?
        if(!dll_empty(&free_areas[current_order].head))
        {
            page_found = true;
            break;
        }
    }

    if(!page_found) return 0;

    // Delete the node since it's not free anymore
    struct double_ll_node *node = free_areas[current_order].head.next;
    dll_delete(node);
    free_areas[current_order].nr_free--;
    
    // Get the page struct
    struct pmm_page *page = (struct pmm_page *)((uint8_t *)node - offsetof(struct pmm_page, link));

    // Splitting
    // If our order is bigger we simply split it until it's of the correct size
    while(current_order > order)
    {
        current_order--;

        // We find our buddy
        uint64_t buddy_pfn = page_to_pfn(page) ^ (1 << current_order);
        struct pmm_page *buddy_page = pfn_to_page(buddy_pfn);
        
        // We initialize the new page
        buddy_page->order = current_order;
        buddy_page->flags = PMM_FLAG_FREE;
        buddy_page->ref_count = 0;

        // We add it to our list of free pages
        dll_add_after(&free_areas[current_order].head, &buddy_page->link);
        free_areas[current_order].nr_free++;
    }

    page->flags = PMM_FLAG_USED;
    page->ref_count = 1;
    page->order = order;

    return page_to_phys(page);
}

// Convert a size (bytes) into log2(bytes)
static uint32_t pmm_get_order_from_size(uint64_t size)
{
    uint64_t pages_needed = size / PMM_PAGE_SIZE;
    if(size % PMM_PAGE_SIZE) pages_needed++;

    // We search the power of 2 
    uint32_t order = 0;
    while((1ULL << order) < pages_needed) order++;
    return order;
}

// Our main function for allocating physical memory
uint64_t pmm_alloc(uint64_t size)
{
    uint32_t order = pmm_get_order_from_size(size);
    if(order >= PMM_MAX_ORDER) return 0; // Too big

    uint64_t phys = pmm_alloc_pages(order);
    if(phys != 0) used_pages += (1ULL << order);

    return phys;
}

// Our main function for deallocating physical memory
void pmm_free(uint64_t physAddr, uint64_t length)
{
    uint32_t order = pmm_get_order_from_size(length);

    pmm_free_pages(physAddr, order);

    used_pages -= (1ULL << order);
}

// Initialize the buddy allocator
void pmm_init()
{
    struct limine_memmap_response *memmap = memmap_request.response;

    // Find the highest usable RAM address
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        switch(entry->type)
        {
            case LIMINE_MEMMAP_USABLE:
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            case LIMINE_MEMMAP_FRAMEBUFFER:
            case LIMINE_MEMMAP_ACPI_TABLES:
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            case LIMINE_MEMMAP_ACPI_NVS:            
                highestAddr = entry->base + entry->length;
                break;
        }
    }

    // The number of pages of usable RAM
    totalPages = highestAddr / PMM_PAGE_SIZE;
    if(highestAddr % PMM_PAGE_SIZE) totalPages++;

    // Calculate the size needed to host our structs
    buddy_memmap_size = totalPages * sizeof(struct pmm_page);

    log_log_line(LOG_DEBUG, "%s: highest addr: 0x%llx; Total pages: 0x%llu; buddy_memmap_size: 0x%llx bytes", __FUNCTION__,highestAddr, totalPages, buddy_memmap_size);

    // Find the first usable region to store our memmap
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= buddy_memmap_size)
        {
            // We convert the physical address to a virtual one
            buddy_memmap = hhdm_physToVirt((void *)entry->base);
            
            // Reduce the region
            entry->base += buddy_memmap_size;
            entry->length -= buddy_memmap_size;
            
            // Zero the memmap
            memset(buddy_memmap, 0, buddy_memmap_size);
            break;
        }
    }

    // If no region is found we abort
    if(!buddy_memmap)
    {
        log_log_line(LOG_ERROR, "%s, Not enough memory for buddy allocator structures", __FUNCTION__);
        hcf();
    }

    // Initialize the free lists
    for(size_t i = 0; i < PMM_MAX_ORDER; i++)
    {
        // Initializes the double linked lists as empty
        dll_init(&free_areas[i].head);
        free_areas[i].nr_free = 0;
    }

    // Fill the memmap as used
    // Note that we memset'd to 0 the entire memmap before so order and link = 0
    used_pages = totalPages;
    for(uint64_t i = 0; i < totalPages; i++)
    {
        buddy_memmap[i].flags |= PMM_FLAG_USED;
        buddy_memmap[i].ref_count = 1;
    }

    // Populate the buddy structs with valid entries
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t start = entry->base;
            uint64_t end = start + entry->length;
            
            // Round up to the next page
            if(start % PMM_PAGE_SIZE) start += PMM_PAGE_SIZE - (start % PMM_PAGE_SIZE);
            
            // Round down to the previous page
            if(end % PMM_PAGE_SIZE) end -= end % PMM_PAGE_SIZE;

            // We sacrifice page 0 as we use that to mark invalid allocations
            if(start == 0)
            {
                start += PMM_PAGE_SIZE;
            }

            uint64_t current_phys = start;
            
            while(current_phys < end)
            {
                uint32_t order = PMM_MAX_ORDER - 1;
                
                // Find the biggest order to free blocks
                while(order > 0)
                {
                    uint64_t size = PMM_PAGE_SIZE * (1ULL << order);
                    
                    // Is the address aligned ?
                    // Is the block small enough to fit in our range?
                    if ((current_phys & (size - 1)) == 0 && (current_phys + size) <= end)
                    {
                        break; // Then we found a big enough block
                    }
                    order--; // Otherwise try it again with a lower order 
                }
                
                // Free the block
                pmm_free_pages(current_phys, order);
                
                used_pages -= (1ULL << order);
                current_phys += (PMM_PAGE_SIZE * (1ULL << order));
            }
        }
    }

    log_log_line(LOG_SUCCESS, "%s: PMM initialized:\n\tBuddy allocator structures size %lu bytes\n\tBuddy allocator start virt addr 0x%lx\n\tManaging %llu pages", __FUNCTION__, buddy_memmap_size, buddy_memmap, totalPages);
}

// Increment the reference count on the page
void pmm_page_inc_ref(uint64_t phys)
{
    struct pmm_page *page = phys_to_page(phys);
    
    if(page && (page->flags & PMM_FLAG_USED))
        page->ref_count++;
}

// Decrements the reference count on the page, if it reaches zero it also frees it
void pmm_page_dec_ref(uint64_t phys)
{
    struct pmm_page *page = phys_to_page(phys);
    if(page && (page->flags & PMM_FLAG_USED))
    {
        page->ref_count--;
        if(page->ref_count == 0)
        {
            pmm_free_pages(phys, page->order);
            used_pages--;
        }
    }
}

// Prints the state of our buddy allocator, nicely formatted
void pmm_dump_state(void)
{
    log_log_line(LOG_DEBUG, "--- BUDDY ALLOCATOR STATE ---");

    for (int i = 0; i < PMM_MAX_ORDER; i++)
    {
        if (free_areas[i].nr_free > 0)
        {
            uint64_t block_size = (1ULL << i) * PMM_PAGE_SIZE;
            
            log_log_line(LOG_DEBUG, "Order %d (%llu KB): %llu blocks free", i, block_size / 1024, free_areas[i].nr_free);
            
        }
    }

    log_log_line(LOG_DEBUG, "-----------------------------");
    log_log_line(LOG_DEBUG, "Total Memory: %llu MB", (totalPages * PMM_PAGE_SIZE) / 1024 / 1024);
    log_log_line(LOG_DEBUG, "Used Memory:  %llu MB", (used_pages * PMM_PAGE_SIZE) / 1024 / 1024);
    log_log_line(LOG_DEBUG, "Free Memory:  %llu MB", ((totalPages - used_pages) * PMM_PAGE_SIZE) / 1024 / 1024);
    log_log_line(LOG_DEBUG, "-----------------------------");
}

// Print the usable physical regions
void pmm_printUsableRegions()
{
    struct limine_memmap_response *memmap = memmap_request.response;

    log_log_line(LOG_DEBUG, "%s: Printing system memory map: ", __FUNCTION__);
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE)
        {
            log_log_line(LOG_DEBUG, "%d) Usable region; Phys range: 0x%llx - 0x%llx; Length: %llu bytes", i+1, entry->base, entry->base + entry->length, entry->length);
        }
    }
}

// Returns the highest RAM address
uint64_t pmm_getHighestAddr(void)
{
    return highestAddr;
}
