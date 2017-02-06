#if defined(_WIN32) || defined(_WIN64)
#include <smbios/win_system_information.h>
#include <smbios/win_bios.h>

#define NOMINMAX
#include <limits>
#include <cassert>
#include <Windows.h>

using namespace smbios;

SMBiosImpl::SMBiosImpl() : native_system_information_(std::make_unique<smbios::NativeSystemInformation>())
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
    if (smbios_data_) {
        return &smbios_data_->smbios_table_data[0];
    }
    return nullptr;
}

size_t SMBiosImpl::get_major_version() const
{
    if (smbios_data_) {
        return smbios_data_->major_version;
    }
    return std::numeric_limits<size_t>::max();
}

size_t SMBiosImpl::get_minor_version() const
{
    if (smbios_data_) {
        return smbios_data_->minor_version;
    }
    return std::numeric_limits<size_t>::max();
}

size_t SMBiosImpl::get_table_size() const
{
    if (table_buffer_.empty()) {
        return 0;
    }
    if (smbios_data_) {
        return smbios_data_->length;
    }
    else {
        return table_buffer_.size();
    }        
}

void SMBiosImpl::read_from_physical_memory(std::vector<uint8_t>& physical_memory_dump)
{
    table_buffer_ = std::move(physical_memory_dump);
}

bool SMBiosImpl::is_ntdll_compatible() const
{
    FARPROC system_firmware_call = GetProcAddress(GetModuleHandle("kernel32.dll"), "GetSystemFirmwareTable");
    if (GetSystemFirmwareTable != reinterpret_cast<void*>(system_firmware_call)) {
        return false;
    }
    return true;
}

void SMBiosImpl::compose_native_smbios_table()
{
    bool is_upto_vista = (native_system_information_->major_version() >= 6);

    if (is_upto_vista && is_ntdll_compatible()) {

        size_t smbios_table_size = GetSystemFirmwareTable('RSMB', 0, nullptr, 0);
        if (0 == smbios_table_size) {
            return;
        }
        table_buffer_.resize(smbios_table_size);

        GetSystemFirmwareTable('RSMB', 0, &table_buffer_[0], smbios_table_size);
        smbios_data_ = reinterpret_cast<RawSMBIOSData*>(&table_buffer_[0]);
    }
}

#endif // defined(_WIN32) || defined(_WIN64)
