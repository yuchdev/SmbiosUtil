#include <smbios_utility/win_bios.h>
#include <smbios_utility/win_system_information.h>

#if defined(_WIN32) || defined(_WIN64)

#include <cassert>
#include <iostream>
#include <Windows.h>

SMBiosImpl::SMBiosImpl() : native_system_information_(std::make_unique<NativeSystemInformation>())
{
    compose_native_smbios_table();
}

SMBiosImpl::~SMBiosImpl()
{

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
    bool is_upto_vista = (native_system_information_->major_version() >= 6);

    if (is_upto_vista) {
        FARPROC system_firmware_call = GetProcAddress(GetModuleHandle("kernel32.dll"), "GetSystemFirmwareTable");
        if (GetSystemFirmwareTable != reinterpret_cast<void*>(system_firmware_call)) {
            // TODO: incompatible
        }
    }

    size_t smbios_table_size = GetSystemFirmwareTable('RSMB', 0, nullptr, 0);
    table_buffer_.resize(smbios_table_size);
    GetSystemFirmwareTable('RSMB', 0, &table_buffer_[0], smbios_table_size);
    smbios_data_ = reinterpret_cast<RawSMBIOSData*>(&table_buffer_[0]);
}

#endif // defined(_WIN32) || defined(_WIN64)
