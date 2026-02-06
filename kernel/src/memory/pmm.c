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

uint8_t *pmm_bitmap = NULL;
size_t pmm_bitmapSize = 0;

// Marks a single page in the bitmap as free or used
static void pmm_mark_page(uint64_t physAddr, uint8_t pageType)
{
    uint64_t bitPos = physAddr / PMM_PAGE_SIZE;

    uint64_t byteIndex = bitPos / 8;
    uint64_t bitIndex = bitPos % 8;

    if(!pmm_bitmap || byteIndex >= pmm_bitmapSize) return;
    
    if(pageType == PMM_PAGE_FREE)
    {
        pmm_bitmap[byteIndex] &= ~(1 << bitIndex);
    }
    else 
    {
        pmm_bitmap[byteIndex] |= (1 << bitIndex);
    }
}

// Mark the memory region in the bitmap
static void pmm_mark_region(uint64_t physStartAddr, uint64_t length, uint8_t regionType)
{
    uint64_t endPhysAddr;

    endPhysAddr = physStartAddr + length;

    // If the startAddr isn't aligned we must round up
    if(physStartAddr % PMM_PAGE_SIZE)
    {
        physStartAddr += PMM_PAGE_SIZE - (physStartAddr % PMM_PAGE_SIZE);
    }

    // If the endAddr isn't aligned we must round down
    if(endPhysAddr % PMM_PAGE_SIZE)
    {
        endPhysAddr -= endPhysAddr % PMM_PAGE_SIZE;
    }

    // Start freeing memory
    for(uint64_t i = physStartAddr; i < endPhysAddr; i += PMM_PAGE_SIZE)
    {
        pmm_mark_page(i, regionType);
    }

    log_logLine(LOG_DEBUG, "%s: Marked region: 0x%llx - 0x%llx as %s", __FUNCTION__, physStartAddr, endPhysAddr-1, 
        regionType == PMM_PAGE_FREE ? "FREE" : "OCCUPIED");
}

// Initialize the bitmap and its size
void pmm_initialize(struct limine_memmap_response *memmap)
{
    uint64_t highestAddr = 0;

    // Iterate the memory map
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE)
        {
            // Find the highest usable RAM address
            uint64_t newAddr = entry->base + entry->length;
            if(newAddr > highestAddr) highestAddr = newAddr;
        }
    }

    // The size of our bitmap, each bit will cointain info for one physical page
    size_t totalPages = highestAddr / PMM_PAGE_SIZE;
    pmm_bitmapSize = totalPages / 8;
    if(totalPages % 8) pmm_bitmapSize++;

    // Find the first usable slot to store our bitmap
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE && pmm_bitmap == NULL && entry->length >= pmm_bitmapSize)
        {
            // We convert the physical address to a virtual one
            pmm_bitmap = hhdm_physToVirt((void *)entry->base);
            
            // Reduce the region
            entry->base += pmm_bitmapSize;
            entry->length -= pmm_bitmapSize;
            
            // Initialize the bitmap as full
            memset(pmm_bitmap, 0xFF, pmm_bitmapSize);

            break;
        }
    }

    // If no region is found we abort
    if(!pmm_bitmap)
    {
        log_logLine(LOG_ERROR, "%s, Not enough memory for bitmap", __FUNCTION__);
        hcf();
    }

    // Fill the bitmap with usable regions
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE)
        {
            pmm_mark_region(entry->base, entry->length, PMM_PAGE_FREE);
        }
    }

    log_logLine(LOG_SUCCESS, "%s: PMM initialized: Bitmap size %lu bytes - Bitmap start virt addr 0x%lx", __FUNCTION__, pmm_bitmapSize, pmm_bitmap);
}

// Allocates new memory and returns a physical address to the start of it
uint64_t pmm_alloc(uint64_t size)
{
    uint64_t currentSize, startingBit, length;
    bool sequenceStarted;

    currentSize = startingBit = length = 0;
    sequenceStarted = false;

    for(size_t i = 0; i < pmm_bitmapSize; i++)
    {
        // We first check if the entire byte is occupied
        if(pmm_bitmap[i] == 0xFF)
        {
            currentSize = startingBit = length = 0;
            sequenceStarted = false;
            
            // If so we simply skip it
            continue;
        }

        for(size_t j = 0; j < 8; j++)
        {
            // If the j-th bit is set then the page is free
            if(!(pmm_bitmap[i] & (1 << j)))
            {
                if(!sequenceStarted)
                {
                    sequenceStarted = true;
                    startingBit = i * 8 + j;
                    length = 0;
                }
                length += PMM_PAGE_SIZE;
                currentSize += PMM_PAGE_SIZE;
            }
            else 
            {
                currentSize = startingBit = length = 0;
                sequenceStarted = false;
            }

            // We found a region
            if(currentSize >= size)
            {
                // We mark the region as occupied
                pmm_mark_region(startingBit * PMM_PAGE_SIZE, length, PMM_PAGE_OCCUPIED);

                return startingBit * PMM_PAGE_SIZE;
            }
        }
    }

    // If we can't find a big enough region we return NULL
    return (uint64_t) NULL;
}

// Free a memory region
void pmm_free(uint64_t physStartAddr, uint64_t length)
{
    pmm_mark_region(physStartAddr, length, PMM_PAGE_FREE);
}