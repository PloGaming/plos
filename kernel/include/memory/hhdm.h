#ifndef HHDM_H
#define HHDM_H

#include <limine.h>

void* hhdm_physToVirt(void *physical_addr);
void* hhdm_virtToPhys(void *virtual_addr);

#endif // HHDM_H