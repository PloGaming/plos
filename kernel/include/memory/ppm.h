#ifndef PPM_H
#define PPM_H

#include <limine.h>

#define PPM_PAGE_SIZE 4096

void ppm_initialize(struct limine_memmap_response *memmap);

#endif // PPM_H