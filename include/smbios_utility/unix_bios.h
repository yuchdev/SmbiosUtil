#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>


#if defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)

class PhysicalMemory;

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

    /// @brief Actual table base (offset from header beginning)
    uint8_t* get_table_base()  const;

    /// @brief Actual table size from table beginning (without header)
    size_t get_table_size()  const;

    void read_from_physical_memory(const PhysicalMemory& physical_memory, size_t length);

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
};


#endif //defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) or defined(__OpenBSD__) || defined(__sun)
