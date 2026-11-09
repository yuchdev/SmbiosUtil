#include <libsmbios/tables/management_device_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::ManagementDevice at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::ManagementDevice, ManagementDeviceEntry)

namespace {

constexpr size_t kDescriptionOffset = 0x04;
constexpr size_t kDeviceTypeOffset = 0x05;
constexpr size_t kAddressOffset = 0x06;
constexpr size_t kAddressTypeOffset = 0x0A;

constexpr size_t kDescriptionMinLength = 0x05;
constexpr size_t kDeviceTypeMinLength = 0x06;
constexpr size_t kAddressMinLength = 0x0A;
constexpr size_t kAddressTypeMinLength = 0x0B;

} // namespace

ManagementDeviceEntry::ManagementDeviceEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::ManagementDevice) {
        std::stringstream err;
        err << "Wrong entry type, expected Management Device, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.35 (Table 107/108/109).
    init_string_values();
}

void ManagementDeviceEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 108.
    device_type_map_[DeviceTypeOutOfSpec] = "Reserved";
    device_type_map_[DeviceTypeOther] = "Other";
    device_type_map_[DeviceTypeUnknown] = "Unknown";
    device_type_map_[DeviceTypeNationalSemiconductorLm75] = "National Semiconductor LM75";
    device_type_map_[DeviceTypeNationalSemiconductorLm78] = "National Semiconductor LM78";
    device_type_map_[DeviceTypeNationalSemiconductorLm79] = "National Semiconductor LM79";
    device_type_map_[DeviceTypeNationalSemiconductorLm80] = "National Semiconductor LM80";
    device_type_map_[DeviceTypeNationalSemiconductorLm81] = "National Semiconductor LM81";
    device_type_map_[DeviceTypeAnalogDevicesAdm9240] = "Analog Devices ADM9240";
    device_type_map_[DeviceTypeDallasSemiconductorDs1780] = "Dallas Semiconductor DS1780";
    device_type_map_[DeviceTypeMaxim1617] = "Maxim 1617";
    device_type_map_[DeviceTypeGenesysGl518sm] = "Genesys GL518SM";
    device_type_map_[DeviceTypeWinbondW83781d] = "Winbond W83781D";
    device_type_map_[DeviceTypeHoltekHt82h791] = "Holtek HT82H791";

    // DSP0134 3.9.0, Table 109.
    address_type_map_[AddressTypeOutOfSpec] = "Reserved";
    address_type_map_[AddressTypeOther] = "Other";
    address_type_map_[AddressTypeUnknown] = "Unknown";
    address_type_map_[AddressTypeIoPort] = "I/O Port";
    address_type_map_[AddressTypeMemory] = "Memory";
    address_type_map_[AddressTypeSmBus] = "SM Bus";
}

std::string ManagementDeviceEntry::get_type() const
{
    return "Management Device";
}

std::string ManagementDeviceEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Description: " << get_description_string() << '\n';
    description << "Device Type: " << get_device_type_string() << '\n';
    description << "Address: " << get_address() << '\n';
    description << "Address Type: " << get_address_type_string() << '\n';

    return description.str();
}

std::string ManagementDeviceEntry::get_description_string() const
{
    if (get_entry_size() < kDescriptionMinLength) return std::string();
    return dmi_string(read_u8(raw_data(), kDescriptionOffset));
}

ManagementDeviceEntry::DeviceType ManagementDeviceEntry::get_device_type() const
{
    if (get_entry_size() < kDeviceTypeMinLength) return DeviceTypeOutOfSpec;
    return static_cast<DeviceType>(read_u8(raw_data(), kDeviceTypeOffset));
}

std::string ManagementDeviceEntry::get_device_type_string() const
{
    auto it = device_type_map_.find(get_device_type());
    if (it != device_type_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

uint32_t ManagementDeviceEntry::get_address() const
{
    if (get_entry_size() < kAddressMinLength) return 0;
    return read_u32(raw_data(), kAddressOffset);
}

ManagementDeviceEntry::AddressType ManagementDeviceEntry::get_address_type() const
{
    if (get_entry_size() < kAddressTypeMinLength) return AddressTypeOutOfSpec;
    return static_cast<AddressType>(read_u8(raw_data(), kAddressTypeOffset));
}

std::string ManagementDeviceEntry::get_address_type_string() const
{
    auto it = address_type_map_.find(get_address_type());
    if (it != address_type_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}
