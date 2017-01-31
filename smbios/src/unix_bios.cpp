#if defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
#include <smbios/unix_bios.h>
#include <smbios/smbios_anchor.h>

#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iterator>

using namespace smbios;

SMBiosImpl::SMBiosImpl()
{
    compose_native_smbios_table();
}

SMBiosImpl::~SMBiosImpl()
{

}

bool SMBiosImpl::smbios_read_success() const
{
    return !table_buffer_.empty();
}

const uint8_t* SMBiosImpl::get_table_base() const
{
    // do not contain system-specific table information
    return &table_buffer_[0];
}


void SMBiosImpl::read_from_physical_memory(std::vector<uint8_t>& physical_memory_dump, size_t length)
{
    table_buffer_ = std::move(physical_memory_dump);
}

size_t SMBiosImpl::get_major_version() const
{
    return std::numeric_limits<size_t>::max();
}

size_t SMBiosImpl::get_minor_version() const
{
    return std::numeric_limits<size_t>::max();
}

size_t SMBiosImpl::get_table_size() const
{
    // do not contain system-specific table information
    table_buffer_.size();
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
}

bool SMBiosImpl::sysfs_table_exists() const
{
#if 1
    return false;
#else
    // TODO: implement on Linux with sysfs-based SMBIOS
    std::string smbios_filename("/sys/firmware/dmi/tables/smbios_entry_point");

    std::cout << "Read SMBIOS entry point\n";
    std::ifstream smbios_file(smbios_filename, std::ios::binary);
    if(!smbios_file.is_open()){
        return false;
    }

    std::istream_iterator<uint8_t> start(smbios_file), end;
    std::vector<uint8_t> smbios_anchor(start, end);
    std::cout << "Read " << smbios_anchor.size() << " bytes" << std::endl;
#endif
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
    // TODO: implement on Mac
}

void SMBiosImpl::reading_from_sysfs()
{
    // TODO: implement on Linux with sysfs-based SMBIOS
}

#endif //defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
