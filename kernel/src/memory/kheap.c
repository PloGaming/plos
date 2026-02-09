#include <common/logging.h>
#include <cpu.h>
#include <memory/hhdm.h>
#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// This is a basic implementation of a free list allocator

static uint64_t kheap_start, kheap_end;
struct kheap_node *kheap_head;

// This function will initialize the kernel heap
void kheap_init(void)
{
    extern char _KERNEL_END;

    // We place the kernel heap 1MB after the end of our kernel
    kheap_start = (uint64_t)&_KERNEL_END + 0x100000;
    
    // Align it to the next virtual page
    if(kheap_start % VMM_PAGE_SIZE)
    {
        kheap_start += VMM_PAGE_SIZE - (kheap_start % VMM_PAGE_SIZE);
    }

    // We set the end at the start since it's empty
    kheap_end = kheap_start;

    // We map the initial memory region of the heap
    kheap_extend(KHEAP_STARTING_SIZE);

    log_logLine(LOG_SUCCESS, "%s: Kernel heap initialized\n\tVirtual range: 0x%llx - 0x%llx", __FUNCTION__, kheap_start, kheap_end);
}

// Extends our kernel heap
bool kheap_extend(size_t size)
{
    // Calculate the number of pages
    uint64_t numPages = size / VMM_PAGE_SIZE;
    if(size % VMM_PAGE_SIZE) numPages++;

    uint64_t *kernel_pml4_phys = vmm_getKernelRoot();
    
    // Start the mapping
    for(uint64_t virtual = kheap_end; virtual < kheap_end + (numPages * VMM_PAGE_SIZE); virtual += VMM_PAGE_SIZE)
    {
        // Allocate a new page
        uint64_t newPage = pmm_alloc(VMM_PAGE_SIZE);
        if(!newPage)
        {
            // No more space in the pmm
            return false;
        }
        // Map the page
        vmm_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, newPage, PTE_FLAG_RW | PTE_FLAG_GLOBAL, false);
    }

    // Get the last node
    struct kheap_node *current = kheap_head;
    while(current != NULL && current->next != NULL)
    {
        current = current->next;
    }

    // We have no nodes or the last one is occupied
    if(current == NULL || current->isFree == false)
    {
        struct kheap_node *newNode = (struct kheap_node *) kheap_end;
        newNode->isFree = true;
        newNode->size = numPages * VMM_PAGE_SIZE - sizeof(struct kheap_node);
        newNode->next = NULL;

        if(current == NULL)
        {
            // If it's the first node then we set the head
            kheap_head = newNode;
        }
        else 
        {
            // Otherwise we attach it to the last one
            current->next = newNode;
        }
    }
    else // The last node is free so we simply augment it's size
    {
        current->size += numPages * VMM_PAGE_SIZE;
    }

    kheap_end += numPages * VMM_PAGE_SIZE;

    log_logLine(LOG_DEBUG, "%s: The heap has been expanded by %llu bytes; new kheap_end = %llx",__FUNCTION__, numPages * VMM_PAGE_SIZE, kheap_end);
    return true;
}

/* 
    Our kernel heap allocating function
    It allocates a virtually contiguos memory region 
    above the mapping of the kernel
*/
void* kmalloc(size_t size)
{
    // Align the size to 16 bytes
    if(size % KHEAP_BLOCK_SIZE) size += KHEAP_BLOCK_SIZE - (size % KHEAP_BLOCK_SIZE);

    // We iterate through the nodes
    struct kheap_node *currentNode;
    while(true)
    {
        currentNode = kheap_head;

        // Iterate the list of free nodes
        while(currentNode != NULL)
        {
            // We found a free and big enough block
            if(currentNode->isFree && currentNode->size >= size)
            {
                // If the remaining size is enough for another big enough block we split it
                if(currentNode->size - size >= sizeof(struct kheap_node) + KHEAP_MIN_SPLITTING_SIZE)
                {
                    struct kheap_node *newNode = (struct kheap_node *)((uint8_t *)currentNode + sizeof(struct kheap_node) + size);
                
                    // Change the next node for both nodes
                    newNode->next = currentNode->next;
                    currentNode->next = newNode;

                    // Set the new node as free and the old one as occupied
                    newNode->isFree = true;
                    currentNode->isFree = false;

                    // Set both sizes accordingly
                    newNode->size = currentNode->size - size - sizeof(struct kheap_node);
                    currentNode->size = size;

                    // Return the usable address
                    return (void *)(currentNode + 1); 
                }
                else 
                {
                    // Else we just allocate it 
                    currentNode->isFree = false;
                    return (void *)(currentNode + 1);
                }
            }

            // We go to the next node
            currentNode = currentNode->next;
        }

        // If we're here it's because we didn't find a big enough block
        if(!kheap_extend(KHEAP_EXTENDING_AMOUNT))
        {
            // The heap expansion has failed
            return NULL;
        }
    }
}

void kfree(void *ptr)
{
    
}