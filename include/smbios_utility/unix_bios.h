#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>


#if defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)

// should be aligned to be mapped to the physical memory
#pragma pack(push, 1)

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
    uint32_t structure_table_address[1];
    uint16_t smbios_structures_number;
    uint8_t smbios_bcd_revision;
};

struct SMBIOSEntryPoint64 {
    uint8_t entry_point_anchor[5];
    uint8_t entry_point_checksum;
    uint8_t entry_point_length;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t smbios_docrev;
    uint8_t reserved;
    uint32_t max_structure_size;
    uint64_t structure_table_address[1];
};

/// @brief SMBIOS header+table beginning
struct RawSMBIOSData {
    uint8_t calling_method;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t dmi_revision;
    uint32_t length;
    uint8_t smbios_table_data[1];
};
#pragma pack(pop)

/// @brief Class that owns memory allocated for SMBIOS table, offsets for table beginning
/// (without header) and table size. It works under Linux, and has since been reported to work
/// on FreeBSD, NetBSD, OpenBSD, BeOS and Solaris
class SMBiosImpl
{
public:

    /// @brief Read the SMBIOS table using /sys/firmware/dmi/tables
    SMBiosImpl();

    /// @brief Make compiler happy
    ~SMBiosImpl();

    /// @brief System-specific SMBIOS source was successful
    bool smbios_read_success() const;

    /// @brief Cast allocated memory to SMBIOS data (header and raw data)
    RawSMBIOSData* get_formatted_smbios_table() const;

    /// @brief Actual table base (offset from header beginning)
    uint8_t* get_table_base()  const;

    /// @brief Actual table size from table beginning (without header)
    size_t get_table_size()  const;

    /// @brief Major version (from header)
    size_t get_major_version() const;

    /// @brief Minor version (from header)
    size_t get_minor_version() const;

private:

    /// Looking for SMBIOS entry point in sysfs
    bool sysfs_table_exists() const;

    /// Looking for SMBIOS entry point in EFI, reading table from /dev/mem
    bool efi_table_exists() const;

    /// Looking for SMBIOS entry point directly in /dev/mem
    //bool scan_devmem_table();

    /// Found SMBIOS entry point in sysfs
    void reading_from_sysfs();

    /// Found SMBIOS entry point in EFI, reading table from /dev/mem
    void reading_from_efi();

    /// Reading from /dev/mem
    void reading_from_devmem();

    /// Implementation
    void compose_native_smbios_table();

    /// Save table with header here
    std::vector<uint8_t> table_buffer_;

    /// Apply to the table with header
    RawSMBIOSData* smbios_data_ = nullptr;
};


#endif //defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) or defined(__OpenBSD__) || defined(__sun)
