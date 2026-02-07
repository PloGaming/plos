#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>

// The starting size of our kernel heap (1MB)
#define KHEAP_STARTING_SIZE 0x100000

void kheap_init(void);
void* kmalloc(size_t size);
void kfree(void *ptr);

#endif // KHEAP_H