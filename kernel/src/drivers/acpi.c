#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <drivers/acpi.h>
#include <memory/hhdm.h>
#include <libk/string.h>

static bool useXSDT = false;
static void* currentRSDT = NULL;

static bool checksum_RSDP(uint8_t *byte_array, size_t size) {
    uint32_t sum = 0;
    for(size_t i = 0; i < size; i++) {
        sum += byte_array[i];
    }
    return (sum & 0xFF) == 0;
}

// Returns true if the rdsp is initialized correctly
// Side effect: Sets the useXSDT global variable to the correct value
bool acpi_set_correct_RSDT(void *rsdp_addr)
{
    struct RSDPDescriptorV2 *rsdp = rsdp_addr;
    if(!rsdp)
    {
        return false;
    }

    // Verify the signature
    if(strncmp(rsdp->Signature, RSDP_SIGNATURE, 8) != 0)
    {
        return false;
    }

    useXSDT = rsdp->Revision == 2;
    currentRSDT = hhdm_physToVirt(useXSDT ? (void *)rsdp->XSDTAddress : 
                    (void *)(uint64_t)rsdp->RsdtAddress);

    return checksum_RSDP((uint8_t *)rsdp, useXSDT ? sizeof(struct RSDPDescriptorV2) :
                    sizeof(struct RSDPDescriptorV1));
}

// Returns true if the RSDP revision is 2
bool acpi_isXSDT(void)
{
    return useXSDT;
}

// Returns the local table
void *acpi_getCurrent_RSDT(void)
{
    return currentRSDT; 
}

// Find a specific sdt table (es. "MADT")
void *acpi_find_table(const char *signature)
{
    struct RSDT *rsdt = currentRSDT;
    struct XSDT *xsdt = currentRSDT;

    size_t numEntries = useXSDT ? (xsdt->sdtHeader.Length - sizeof(xsdt->sdtHeader)) / sizeof(xsdt->sdtAddresses[0])
                            : (rsdt->sdtHeader.Length - sizeof(rsdt->sdtHeader)) / sizeof(rsdt->sdtAddresses[0]);
    
    for(size_t i = 0; i < numEntries; i++)
    {
        uint64_t physAddr = (useXSDT ? xsdt->sdtAddresses[i] : (uint64_t)rsdt->sdtAddresses[i]);
        
        struct ACPISDTHeader *currentTable = hhdm_physToVirt((void *)physAddr);

        if(strncmp(currentTable->Signature, signature, 4) == 0)
        {
            return (void *)currentTable;
        }
    }

    return NULL;
}