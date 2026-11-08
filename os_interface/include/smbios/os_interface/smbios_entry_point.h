#pragma once
#include <cstdint>

namespace smbios {

// should be aligned to be mapped to the physical memory
#pragma pack(push, 1)

/// @brief SMBIOS entry point for 32-bit systems
/// Contains 2 checksum, 2 anchors so that not to misuse
struct SMBIOSEntryPoint32 {
    uint8_t entry_point_anchor[4];
    uint8_t entry_point_checksum;
    uint8_t entry_point_length;
    uint8_t major_version;
    uint8_t minor_version;
    uint16_t max_structure_size;
    uint8_t entry_point_revision;
    uint8_t formatted_area[5];
    uint8_t intermediate_anchor[5];
    uint8_t intermediate_checksum;
    uint16_t structure_table_length;
    intptr_t structure_table_address;
    uint16_t smbios_structures_number;
    uint8_t smbios_bcd_revision;
};

/// @brief SMBIOS entry point for 64-bit systems
/// Contains 1 checksum, 1 anchor
/// Size is not exact unlike for 32 version but it is guaranteed enough
struct SMBIOSEntryPoint64 {
    uint8_t entry_point_anchor[5];
    uint8_t entry_point_checksum;
    uint8_t entry_point_length;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t smbios_docrev;
    uint8_t reserved;
    uint32_t max_structure_size;
    intptr_t structure_table_address;
};

#pragma pack(pop)

} // namespace smbios
