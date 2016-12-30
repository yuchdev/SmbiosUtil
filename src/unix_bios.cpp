#include <smbios_utility/unix_bios.h>

#include <cassert>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <iterator>

#if defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) or defined(__OpenBSD__) || defined(__sun)

SMBiosImpl::SMBiosImpl()
{
    compose_native_smbios_table();
}

RawSMBIOSData* SMBiosImpl::get_formatted_smbios_table() const
{
    assert(table_buffer_.size());
    assert(smbios_data_);
    return smbios_data_;
}

uint8_t* SMBiosImpl::get_table_base() const
{
    assert(table_buffer_.size());
    assert(smbios_data_);
    return &smbios_data_->smbios_table_data[0];
}

size_t SMBiosImpl::get_major_version() const
{
    assert(table_buffer_.size());
    assert(smbios_data_);
    return smbios_data_->major_version;
}

size_t SMBiosImpl::get_minor_version() const
{
    assert(table_buffer_.size());
    assert(smbios_data_);
    return smbios_data_->minor_version;
}

size_t SMBiosImpl::get_table_size() const
{
    assert(table_buffer_.size());
    assert(smbios_data_);
    return smbios_data_->length;
}

void SMBiosImpl::compose_native_smbios_table()
{
    std::string smbios_filename("/sys/firmware/dmi/tables/smbios_entry_point");

    std::cout << "Read SMBIOS table\n";
    std::ifstream smbios_file(smbios_filename, std::ios::binary);

    std::istream_iterator<uint8_t> start(smbios_file), end;
    std::vector<uint8_t> smbios_array(start, end);
    std::cout << "Read " << smbios_array.size() << " bytes" << std::endl;
}

#endif //defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) or defined(__OpenBSD__) || defined(__sun)
