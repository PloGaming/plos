#include <common/logging.h>
#include <cpu.h>
#include <memory/hhdm.h>
#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/paging.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// This is a basic implementation of a linked list allocator 

static uint64_t kheap_start, kheap_end;
static struct kheap_node *kheap_head;

/**
 * @brief This function will initialize the kernel heap
 * The kernel heap is placed after the kernel, using the remainig space on the VAS
 */
void kheap_init(void)
{
    extern char _KERNEL_END;

    // We place the kernel heap 1MB after the end of our kernel
    kheap_start = (uint64_t)&_KERNEL_END + 0x100000;
    
    // Align it to the next virtual page
    if(kheap_start % PAGING_PAGE_SIZE)
    {
        kheap_start += PAGING_PAGE_SIZE - (kheap_start % PAGING_PAGE_SIZE);
    }

    // We set the end at the start since it's empty
    kheap_end = kheap_start;

    // We map the initial memory region of the heap
    kheap_extend(KHEAP_STARTING_SIZE);

    log_log_line(LOG_SUCCESS, "%s: Kernel heap initialized\n\tVirtual range: 0x%llx - 0x%llx", __FUNCTION__, kheap_start, kheap_end);
}

/**
 * @brief Extends our kernel heap
 * 
 * @param size The amount of bytes our kernel heap grows
 * @return true if the heap extension went successfully
 * @return false if the heap extension failed
 */
bool kheap_extend(size_t size)
{
    // Calculate the number of pages
    uint64_t numPages = size / PAGING_PAGE_SIZE;
    if(size % PAGING_PAGE_SIZE) numPages++;

    uint64_t *kernel_pml4_phys = paging_getKernelRoot();
    
    // Start the mapping
    for(uint64_t virtual = kheap_end; virtual < kheap_end + (numPages * PAGING_PAGE_SIZE); virtual += PAGING_PAGE_SIZE)
    {
        // Allocate a new page
        uint64_t newPage = pmm_alloc(PAGING_PAGE_SIZE);
        if(!newPage)
        {
            // No more space in the pmm
            return false;
        }
        // Map the page
        paging_map_page(hhdm_physToVirt(kernel_pml4_phys), virtual, newPage, PTE_FLAG_RW | PTE_FLAG_GLOBAL, false);
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
        newNode->size = numPages * PAGING_PAGE_SIZE - sizeof(struct kheap_node);
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
        current->size += numPages * PAGING_PAGE_SIZE;
    }

    kheap_end += numPages * PAGING_PAGE_SIZE;

    log_log_line(LOG_DEBUG, "%s: The heap has been expanded by %llu bytes; new kheap_end = %llx",__FUNCTION__, numPages * PAGING_PAGE_SIZE, kheap_end);
    return true;
}

/**
 * @brief kernel heap allocating function
 * It allocates a virtually contiguos memory region 
 * above the mapping of the kernel
 * not guaranteed to be physically contiguos
 * @param size The minimum bytes that need to be reserved
 * @return void* A virtual address pointing to the reserved region
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

                    // Set both sizes accordingly
                    newNode->size = currentNode->size - size - sizeof(struct kheap_node);
                    currentNode->size = size;
                }

                currentNode->isFree = false;
                return (void *)(currentNode + 1);
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

/**
 * @brief Prints all the nodes in the kernel heap
 * It's a debug function, for understanding the current kernel heap structure 
 */
void kheap_print_nodes()
{
    log_log_line(LOG_DEBUG, "%s: Kernel heap current nodes:", __FUNCTION__);
    struct kheap_node *current = kheap_head;
    while(current)
    {
        log_log_line(LOG_DEBUG, "Region: 0x%llx - 0x%llx; isFree: %s; size: %lld bytes", 
            (uint64_t)current + sizeof(struct kheap_node), 
            (uint64_t)current + sizeof(struct kheap_node) + current->size,
            current->isFree ? "true" : "false", 
            current->size);
        current = current->next;
    }
}

/**
 * @brief Tries to coalesce near free blocks
 * useful to combat external fragmentation
 */
static void kheap_coalesce()
{
    struct kheap_node *current = kheap_head;
    while(current != NULL && current->next != NULL)
    {
        // If 2 consecutive blocks are free then we merge them
        if(current->isFree && current->next->isFree)
        {
            // Increment the size of the previous node
            current->size += current->next->size + sizeof(struct kheap_node);

            // We remove the 2nd node from the list
            current->next = current->next->next;
        }
        else 
        {
            // We advance only if we didn't merge anything
            current = current->next;
        }
    }
}

/**
 * @brief Our kernel heap deallocator function
 * 
 * @param ptr A pointer to a valid kernel heap region
 */
void kfree(void *ptr)
{
    if(!ptr) return;

    // Get the header
    struct kheap_node *node_to_free = (struct kheap_node *)ptr - 1;
    
    // Check for double free
    if(node_to_free->isFree)
    {
        log_log_line(LOG_WARN, "%s: Kernel heap double free detected; virtual addr: 0x%llx", __FUNCTION__, ptr);
        return;
    }

    // Set the node as free
    node_to_free->isFree = true;

    // Coalesce the blocks to reduce external fragmentation
    kheap_coalesce();
}