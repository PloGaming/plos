#ifndef ACPI_H
#define ACPI_H

#include <stdbool.h>
#include <stdint.h>

#define RSDP_SIGNATURE "RSD PTR "

/**
 * @brief The RSDP structure v1.0 with a 32 bit address for RSDT
 * 
 */
struct RSDPDescriptorV1 {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__ ((packed));

/**
 * @brief The RSDP structure v2.0 with a 64 bit address for XSDT
 * 
 */
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

struct ACPISDTHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed));

struct RSDT {
    struct ACPISDTHeader sdtHeader; //signature "RSDT"
    uint32_t sdtAddresses[];
} __attribute__((packed));

struct XSDT {
    struct ACPISDTHeader sdtHeader; //signature "XSDT"
    uint64_t sdtAddresses[];
} __attribute__((packed));

bool acpi_set_correct_RSDT();
bool acpi_isXSDT(void);
void *acpi_get_current_RSDT(void);
void *acpi_find_table(const char *signature);

#endif // ACPI_H