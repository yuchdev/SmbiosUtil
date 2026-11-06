#if defined(__APPLE__) || defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
#include <smbios/source_linux/unix_bios.h>
#include <smbios/source_interface/smbios_anchor.h>

#include <cassert>
#include <limits>
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
    // table_buffer_ is empty whenever neither sysfs, EFI, nor the
    // physical-memory fallback has populated it yet (e.g. unprivileged
    // host, no root access to /dev/mem). `table_buffer_[0]` on an empty
    // vector is a precondition violation (operator[] requires
    // pos < size()); on toolchains that assert this precondition (seen in
    // CI: GCC 15/libstdc++ hardened mode) it aborts the process outright,
    // and even where it doesn't assert it is still undefined behavior.
    // Mirror SMBios::get_table_base()'s own nullptr contract (see
    // smbios.cpp) so callers can detect "no table" instead of crashing.
    if (table_buffer_.empty()) {
        return nullptr;
    }
    // do not contain system-specific table information
    return &table_buffer_[0];
}


void SMBiosImpl::read_from_physical_memory(std::vector<uint8_t>& physical_memory_dump)
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
    return table_buffer_.size();
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
    // Not yet implemented - sysfs-based SMBIOS discovery is milestone 0005's Story 02.0.
    return false;
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

#endif //defined(__APPLE__) || defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
