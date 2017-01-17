#pragma once
#include <vector>
#include <cstdint>
#include <memory>

#if defined(_WIN32) || defined(_WIN64)

class NativeSystemInformation;

/// @brief SMBIOS header+table beginning
struct RawSMBIOSData {
    uint8_t calling_method;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t dmi_revision;
    uint32_t length;
    uint8_t smbios_table_data[1];
};

/// @brief Class that owns memory allocated for SMBIOS table, offsets for table beginning
/// (without header) and table size
class SMBiosImpl
{
public:

    /// @brief Read the SMBIOS table using GetSystemFirmwareTable() 
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

    /// Find ntdll entry point
    bool is_ntdll_compatible() const;

    /// Implementation
    void compose_native_smbios_table();

    /// Save table with header here
    std::vector<uint8_t> table_buffer_;

    std::unique_ptr<NativeSystemInformation> native_system_information_;

    /// Apply to the table with header
    RawSMBIOSData* smbios_data_ = nullptr;
};

#endif // defined(_WIN32) || defined(_WIN64)
