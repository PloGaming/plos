#ifndef PMM_H
#define PMM_H

#include <limine.h>
#include <stddef.h>

#define PMM_PAGE_SIZE     4096
#define PMM_PAGE_OCCUPIED 1
#define PMM_PAGE_FREE     0

void pmm_initialize(struct limine_memmap_response *memmap);
void *pmm_alloc(size_t size);

#endif // PMM_H