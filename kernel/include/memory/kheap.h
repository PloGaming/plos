#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define KHEAP_STARTING_SIZE 0x100000 ///< The starting size of our kernel heap (1MB)
#define KHEAP_MIN_SPLITTING_SIZE 16 ///< How much memory does a block need to be splitted
#define KHEAP_BLOCK_SIZE 16 ///< An alignment made to each size request
#define KHEAP_EXTENDING_AMOUNT 0x100000 ///< How much memory to extend our heap

/**
 * @brief This struct describes a single region of the kernel heap
 * The kernel heap is an ordered linked list of those nodes,
 * each node can be free or used. Different nodes describe
 * different non-overlapping memory regions
 */
struct kheap_node
{
    uint64_t size; ///< The size of the region EXCLUDING its header size
    bool isFree; ///< Is this region free?
    struct kheap_node *next; ///< A pointer to the next node 
};

void kheap_init(void);
bool kheap_extend(size_t size);
void* kmalloc(size_t size);
void kfree(void *ptr);
void kheap_print_nodes();

#endif // KHEAP_H