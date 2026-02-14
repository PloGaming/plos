#ifndef VMM_H
#define VMM_H

#include <interrupts/isr.h>
#include <stdint.h>

/**
 * @name VMM start/end for user/kernel 
 * Describes the locations for the 2 virtual regions
 * @{
 */
#define VMM_USER_START 0x1000 ///< Userspace starts at 4Kb because the 1st page is reserved for invalid pointers
#define VMM_USER_END   0x7FFFFFFFFFFF ///< Userspace ends to 2^48 - 1

#define VMM_KERNEL_START 0xFFFFC00000000000 ///< Define a big virtual memory area for virtual mappings in the kernel 
#define VMM_KERNEL_END (0xFFFFFFFF80000000 - 1)
/** @} */

/**
 * @name VMM Flags
 * Flags for each VAS area
 * @{
 */
#define VMM_FLAGS_NONE      0               ///< No permissions
#define VMM_FLAGS_READ      (1ull << 0)     ///< Readable page
#define VMM_FLAGS_WRITE     (1ull << 1)     ///< Writable page
#define VMM_FLAGS_EXEC      (1ull << 2)     ///< Executable page
#define VMM_FLAGS_USER      (1ull << 3)     ///< User accessible page
#define VMM_FLAGS_ANON      (1ull << 4)     ///< Anonymous page
#define VMM_FLAGS_MMIO      (1ull << 5)     ///< Memory mapped I/O in this page
/** @} */

/**
 * @brief A contiguos region of virtual memory
 * Used mainly to guide the page fault handler into
 * his decision making
 */
struct vm_area {
    uint64_t base; ///< The starting virtual address 
    uint64_t size; ///< The length of the region
    uint64_t flags; ///< Flags that describe the type of this region
    struct vm_area *next; ///< Pointer to the next region
};

/**
 * @brief Represents a single address space
 * It's the container of all the regions
 */
struct vm_address_space {
    uint64_t *pml4_phys; ///< The pointer to the pml4 table for this VAS
    struct vm_area *region_list; ///< List of the regions
};

void vmm_init(void);

struct vm_address_space *vmm_new_address_space(void);
void vmm_destroy_address_space(struct vm_address_space *);
void vmm_switch_address_space(struct vm_address_space *space);

void *vmm_alloc(struct vm_address_space *space, uint64_t size, uint64_t flags, uint64_t arg);
void vmm_free(struct vm_address_space *space, uint64_t addr);

struct vm_address_space* vmm_get_kernel_vas(void);
uint64_t vmm_generic_to_x86_flags(uint64_t genericFlags);

void vmm_page_fault_handler(struct isr_context *context);

#endif // VMM_H