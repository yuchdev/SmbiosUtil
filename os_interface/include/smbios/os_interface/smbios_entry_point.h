#pragma once
#include <cstdint>

namespace smbios {

// should be aligned to be mapped to the physical memory
#pragma pack(push, 1)

/// @brief SMBIOS entry point for 32-bit systems ("legacy" DMTF entry point,
/// detected by the `_SM_` anchor via `detect_smbios_anchor()`). Contains 2
/// checksums and 2 anchors so a scanner can validate it was not matched by
/// coincidence (see `legacy_memory_scan_source.cpp`'s `checksum32()`).
struct SMBIOSEntryPoint32 {
    ///< The 4-byte `"_SM_"` anchor string identifying this as a 32-bit
    ///< entry point; checked by `detect_smbios_anchor()`.
    uint8_t entry_point_anchor[4];

    ///< Checksum byte making the sum of all bytes from
    ///< `entry_point_anchor` through the end of this structure
    ///< (`entry_point_length` bytes) equal zero mod 256.
    uint8_t entry_point_checksum;

    ///< Length, in bytes, of this entry point structure (used as the
    ///< checksum range above, not necessarily `sizeof(SMBIOSEntryPoint32)`
    ///< on every BIOS).
    uint8_t entry_point_length;

    ///< SMBIOS specification major version this table was built against.
    uint8_t major_version;

    ///< SMBIOS specification minor version; see
    ///< `fixup_smbios_version()` for known malformed values some BIOSes
    ///< report here.
    uint8_t minor_version;

    ///< Size, in bytes, of the largest SMBIOS structure in the table,
    ///< including its string section.
    uint16_t max_structure_size;

    ///< SMBIOS BCD revision of the entry point structure itself (distinct
    ///< from `smbios_bcd_revision`, which is the structure table's own
    ///< revision).
    uint8_t entry_point_revision;

    ///< 5 bytes reserved for `entry_point_revision`-specific data; all
    ///< zero when `entry_point_revision` is 0.
    uint8_t formatted_area[5];

    ///< The 5-byte `"_DMI_"` intermediate anchor string, validated
    ///< separately from `entry_point_anchor` (see `checksum32()`).
    uint8_t intermediate_anchor[5];

    ///< Checksum byte making the sum of the 15 bytes starting at
    ///< `intermediate_anchor` (offset 0x10) equal zero mod 256.
    uint8_t intermediate_checksum;

    ///< Total length, in bytes, of the SMBIOS structure table pointed to
    ///< by `structure_table_address`.
    uint16_t structure_table_length;

    ///< Physical address of the first byte of the SMBIOS structure table.
    intptr_t structure_table_address;

    ///< Total number of SMBIOS structures present in the table, including
    ///< the terminating `EndOfTable` structure.
    uint16_t smbios_structures_number;

    ///< BCD revision of the SMBIOS structure table's own format (not the
    ///< entry point's).
    uint8_t smbios_bcd_revision;
};

/// @brief SMBIOS entry point for 64-bit systems ("SMBIOS3" entry point,
/// detected by the `_SM3_` anchor via `detect_smbios_anchor()`). Contains
/// 1 checksum and 1 anchor (no intermediate anchor, unlike the 32-bit
/// form). Size is not exact unlike the 32-bit version, but is guaranteed
/// large enough for every field this project reads.
struct SMBIOSEntryPoint64 {
    ///< The 5-byte `"_SM3_"` anchor string identifying this as a 64-bit
    ///< entry point; checked by `detect_smbios_anchor()`.
    uint8_t entry_point_anchor[5];

    ///< Checksum byte making the sum of all bytes from
    ///< `entry_point_anchor` through the end of this structure
    ///< (`entry_point_length` bytes) equal zero mod 256.
    uint8_t entry_point_checksum;

    ///< Length, in bytes, of this entry point structure.
    uint8_t entry_point_length;

    ///< SMBIOS specification major version this table was built against.
    uint8_t major_version;

    ///< SMBIOS specification minor version; see `fixup_smbios_version()`.
    uint8_t minor_version;

    ///< SMBIOS docrev: revision of the SMBIOS specification document
    ///< itself, distinct from major/minor version.
    uint8_t smbios_docrev;

    ///< Entry point revision; reserved/unused in the 64-bit form today.
    uint8_t reserved;

    ///< Maximum size, in bytes, of the SMBIOS structure table (an upper
    ///< bound to allocate against, not necessarily the exact table size).
    uint32_t max_structure_size;

    ///< Physical address of the first byte of the SMBIOS structure table.
    intptr_t structure_table_address;
};

#pragma pack(pop)

} // namespace smbios
