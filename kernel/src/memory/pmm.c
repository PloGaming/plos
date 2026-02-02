#include <cpu.h>
#include <libk/stdio.h>
#include <memory/pmm.h>
#include <limine.h>
#include <stddef.h>
#include <stdint.h>
#include <memory/hhdm.h>
#include <libk/string.h>

uint8_t *pmm_bitmap = NULL;
size_t pmm_bitmapSize = 0;

static void pmm_mark(uint64_t physAddr, uint8_t pageType)
{
    uint64_t bitPos = physAddr / PMM_PAGE_SIZE;

    uint64_t byteIndex = bitPos / 8;
    uint64_t bitIndex = bitPos % 8;

    if(byteIndex > pmm_bitmapSize) return;
    
    if(pageType == PMM_PAGE_FREE)
    {
        pmm_bitmap[byteIndex] &= ~(1 << bitIndex);
        return;
    }
    
    pmm_bitmap[byteIndex] |= (1 << bitIndex);
}

// Free the memory region in the bitmap in the region
void pmm_free(uint64_t physStartAddr, uint64_t length)
{
    uint64_t endPhysAddr;

    endPhysAddr = physStartAddr + length;
;
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
        pmm_mark(i, PMM_PAGE_FREE);
    }
}

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
            highestAddr = entry->base + entry->length;
        }
    }

    // The size of our bitmap, each bit will cointain info for one physical page
    size_t totalPages = (highestAddr / PMM_PAGE_SIZE);
    pmm_bitmapSize = totalPages / 8;
    if(totalPages % 8) pmm_bitmapSize++;

    // Find the first usable slot to store our bitmap
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE && pmm_bitmap == NULL && entry->length >= pmm_bitmapSize)
        {
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
        printf("[ERROR] Not enough memory for bitmap\n");
        hcf();
    }

    // Fill the bitmap with usable regions
    for(size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE)
        {
            pmm_free(entry->base, entry->length);
        }
    }

    printf("[PMM] PMM initialized: Bitmap size %lu bytes - Bitmap start virt addr 0x%lx\n", pmm_bitmapSize, pmm_bitmap);
}

// Allocates new memory and returns a pointer to the start of it
void *pmm_alloc(size_t size)
{

}