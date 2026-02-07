#ifndef PMM_H
#define PMM_H

#include <limine.h>

#define PMM_PAGE_SIZE     4096
#define PMM_PAGE_OCCUPIED 1
#define PMM_PAGE_FREE     0

void pmm_initialize(struct limine_memmap_response *memmap);
void pmm_free(uint64_t physStartAddr, uint64_t length);
uint64_t pmm_alloc(uint64_t size);
uint64_t pmm_getHighestAddr(void);

#endif // PMM_H