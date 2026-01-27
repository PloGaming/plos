#ifndef ACPI_H
#define ACPI_H

#include <stdbool.h>
#include <stdint.h>

#define RSDP_SIGNATURE "RSD PTR "

struct RSDPDescriptorV1 {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__ ((packed));

struct RSDPDescriptorV2 {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;

    // Valid fields only if Revision == 2
    uint32_t Length;
    uint64_t XSDTAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} __attribute__((packed));

bool acpi_verify_rsdp(void *rsdp);

#endif // ACPI_H