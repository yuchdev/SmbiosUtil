#pragma once
#include <vector>
#include <cstdint>


struct RawSMBIOSData {
    uint8_t calling_method;;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t dmi_revision;
    uint32_t length;
    uint8_t smbios_table_data[1];
};

class SMBiosImpl
{
public:

    SMBiosImpl();

    RawSMBIOSData* get_formatted_smbios_table() const;

    uint8_t* get_table_base()  const;

    size_t get_table_size()  const;

    size_t get_major_version() const;

    size_t get_minor_version() const;

private:

    void compose_native_smbios_table();

    std::vector<uint8_t> table_buffer_;

    RawSMBIOSData* smbios_data_ = nullptr;
};
