#include <smbios_utility/unix_bios.h>
#include <smbios_utility/physical_memory.h>

#include <boost/iostreams/device/mapped_file.hpp>

#include <cassert>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <iterator>

namespace boost_io = boost::iostreams;
#if defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)

SMBiosImpl::SMBiosImpl()
{
    compose_native_smbios_table();
}

SMBiosImpl::~SMBiosImpl()
{

}

bool SMBiosImpl::smbios_read_success() const
{
    return !table_buffer_.empty() && smbios_data_;
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
    if(sysfs_table_exists()){
        reading_from_sysfs();
        return;
    }
    if(efi_table_exists()){
        reading_from_efi();
        return;
    }
    scan_devmem_table();
}

bool SMBiosImpl::sysfs_table_exists() const
{
    std::string smbios_filename("/sys/firmware/dmi/tables/smbios_entry_point");

    std::cout << "Read SMBIOS entry point\n";
    std::ifstream smbios_file(smbios_filename, std::ios::binary);
    if(!smbios_file.is_open()){
        return false;
    }

    std::istream_iterator<uint8_t> start(smbios_file), end;
    std::vector<uint8_t> smbios_array(start, end);
    std::cout << "Read " << smbios_array.size() << " bytes" << std::endl;

    return smbios_array.size() ? true : false;
}

bool SMBiosImpl::efi_table_exists() const
{
    // try these places for EFI entry point
    const std::string filename1("/sys/firmware/efi/systab");
    const std::string filename2("/proc/efi/systab");
    std::ifstream systab_file;

    systab_file.open(filename1);
    if(!systab_file.is_open()){
        systab_file.open(filename2);
    }

    if(!systab_file.is_open()){
        return false;
    }

    std::string systab_entry;
    while(getline(systab_file, systab_entry)){
        if(systab_entry.find("SMBIOS") != std::string::npos){
            return true;
        }
    }

    return false;
}

void SMBiosImpl::reading_from_efi()
{
}

void SMBiosImpl::reading_from_sysfs()
{
    uint8_t smbios_32_header[] = {'_','S','M','_'};
    uint8_t smbios_64_header[] = {'_','S','M','3','_'};
    uint8_t smbios_legacy_header[] = {'_','D','M','I','_'};

    std::string smbios_filename("/sys/firmware/dmi/tables/DMI");

    std::cout << "Read SMBIOS table\n";
    std::ifstream smbios_file(smbios_filename, std::ios::binary);

    std::istream_iterator<uint8_t> start(smbios_file), end;
    std::vector<uint8_t> smbios_array(start, end);

    bool is_64_header = std::equal(smbios_array.begin(), smbios_array.begin() + sizeof(smbios_64_header),
               std::begin(smbios_64_header), std::end(smbios_64_header));

    bool is_32_header = std::equal(smbios_array.begin(), smbios_array.begin() + sizeof(smbios_32_header),
               std::begin(smbios_32_header), std::end(smbios_32_header));

    bool is_legacy_header = std::equal(smbios_array.begin(), smbios_array.begin() + sizeof(smbios_legacy_header),
               std::begin(smbios_legacy_header), std::end(smbios_legacy_header));

    std::cout << "SMBIOS 32 header = " << is_32_header << '\n';
    std::cout << "SMBIOS 32 header = " << is_32_header << '\n';
    std::cout << "SMBIOS 64 header = " << is_64_header << '\n';
    std::cout << "SMBIOS legacy header = " << is_legacy_header << '\n';

    if(is_32_header){
        // TODO: create table_buffer_ with offset
    }
}

#endif //defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
