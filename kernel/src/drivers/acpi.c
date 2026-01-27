#include <drivers/acpi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <libk/string.h>

static bool useXSDT = false;

static bool checksum_RSDP(uint8_t *byte_array, size_t size) {
    uint32_t sum = 0;
    for(size_t i = 0; i < size; i++) {
        sum += byte_array[i];
    }
    return (sum & 0xFF) == 0;
}

// Returns true if the rdsp is initialized correctly
bool acpi_verify_rsdp(void *rsdp_addr)
{
    struct RSDPDescriptorV2 *rsdp = rsdp_addr;
    if(!rsdp)
    {
        return false;
    }

    // Verify the signature
    for(uint32_t i = 0; i < strlen(RSDP_SIGNATURE); i++)
    {
        if(rsdp->Signature[i] != RSDP_SIGNATURE[i])
        {
            return false;
        }
    }

    useXSDT = rsdp->Revision == 2;

    return checksum_RSDP((uint8_t *)rsdp, useXSDT ? sizeof(struct RSDPDescriptorV2) :
                                                    sizeof(struct RSDPDescriptorV1));
}