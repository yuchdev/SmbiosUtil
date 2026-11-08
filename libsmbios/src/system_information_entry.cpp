#include <smbios/system_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::SystemInformation at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemInformation, SystemInformationEntry)

SystemInformationEntry::SystemInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end), version_(version)
{
    if (header.type != SMBios::SystemInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected System Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    init_string_values();
}

void SystemInformationEntry::init_string_values()
{
    wake_up_type_map_[Reserved] = "Reserved";
    wake_up_type_map_[Other] = "Other";
    wake_up_type_map_[Unknown] = "Unknown";
    wake_up_type_map_[APMTimer] = "APM Timer";
    wake_up_type_map_[ModemRing] = "Modem Ring";
    wake_up_type_map_[LANRemote] = "LAN Remote";
    wake_up_type_map_[PowerSwitch] = "Power Switch";
    wake_up_type_map_[PCIPME] = "PCI PME#";
    wake_up_type_map_[ACPowerRestored] = "AC Power Restored";
}

std::string SystemInformationEntry::get_type() const
{
    return "System Information";
}

std::string SystemInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Manufacturer: " << get_manufacturer_string() << '\n';
    description << "Product Name: " << get_product_name_string() << '\n';
    description << "Version: " << get_version_string() << '\n';
    description << "Serial Number: " << get_serial_number_string() << '\n';
    description << "UUID: " << get_uuid_string() << '\n';
    description << "Wake-up Type: " << get_wake_up_type_string() << '\n';
    description << "SKU Number: " << get_sku_number_string() << '\n';
    description << "Family: " << get_family_string() << '\n';

    return description.str();
}

uint8_t SystemInformationEntry::get_manufacturer_index() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x08; // manufacturer..serial_number gated together (dmidecode.c case 1)
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemInformationEntry::get_product_name_index() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x08;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemInformationEntry::get_version_index() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x08;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemInformationEntry::get_serial_number_index() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x08;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemInformationEntry::get_wake_up_type() const
{
    constexpr size_t offset = 0x18;
    constexpr size_t min_length = 0x19; // UUID+wake_up_type gated together (dmidecode.c case 1)
    if (get_entry_size() < min_length) return WakeUpType::Reserved;

    const uint8_t wake_up_type = read_u8(raw_data(), offset);
    auto it = wake_up_type_map_.find(wake_up_type);
    if (it != wake_up_type_map_.end()) {
        return (*it).first;
    }

    // should not be undefined values
    assert(false);
    return WakeUpType::Reserved;
}

uint8_t SystemInformationEntry::get_sku_number_index() const
{
    constexpr size_t offset = 0x19;
    constexpr size_t min_length = 0x1B; // sku_number+family gated together (dmidecode.c case 1)
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemInformationEntry::get_family_index() const
{
    constexpr size_t offset = 0x1A;
    constexpr size_t min_length = 0x1B;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

std::string SystemInformationEntry::get_manufacturer_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_manufacturer_index());
}

std::string SystemInformationEntry::get_product_name_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_product_name_index());
}

std::string SystemInformationEntry::get_version_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_version_index());
}

std::string SystemInformationEntry::get_serial_number_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_serial_number_index());
}

std::string SystemInformationEntry::get_uuid_string() const
{
    constexpr size_t offset = 0x08;
    constexpr size_t min_length = 0x19; // UUID+wake_up_type gated together (dmidecode.c case 1)
    if (get_entry_size() < min_length) {
        return "Not Specified";
    }

    return format_uuid(raw_data() + offset, version_);
}

std::string SystemInformationEntry::get_wake_up_type_string() const
{
    assert(!wake_up_type_map_.empty());
    return (*wake_up_type_map_.find(get_wake_up_type())).second;
}

std::string SystemInformationEntry::get_sku_number_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_sku_number_index());
}

std::string SystemInformationEntry::get_family_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_family_index());
}

std::string SystemInformationEntry::format_uuid(const uint8_t* uuid, const SMBiosVersion& version)
{
    bool only_0xff = true;
    bool only_0x00 = true;
    for (size_t i = 0; i < 16 && (only_0x00 || only_0xff); ++i) {
        if (uuid[i] != 0x00) only_0x00 = false;
        if (uuid[i] != 0xFF) only_0xff = false;
    }

    if (only_0xff) {
        return "Not Present";
    }
    if (only_0x00) {
        return "Not Settable";
    }

    std::stringstream formatted;
    formatted << std::hex << std::setfill('0');

    auto append_byte = [&formatted](uint8_t byte_value) {
        formatted << std::setw(2) << static_cast<unsigned>(byte_value);
    };

    // As of SMBIOS 2.6, the first 3 fields are little-endian multi-byte
    // integers and must be byte-swapped to print in conventional
    // big-endian hex order; versions before 2.6 print the 16 bytes
    // straight through, unswapped. See dmi_system_uuid() in the reference.
    if (!(version < SMBiosVersion{2, 6})) {
        append_byte(uuid[3]); append_byte(uuid[2]); append_byte(uuid[1]); append_byte(uuid[0]);
        formatted << '-';
        append_byte(uuid[5]); append_byte(uuid[4]);
        formatted << '-';
        append_byte(uuid[7]); append_byte(uuid[6]);
        formatted << '-';
        append_byte(uuid[8]); append_byte(uuid[9]);
        formatted << '-';
        append_byte(uuid[10]); append_byte(uuid[11]); append_byte(uuid[12]);
        append_byte(uuid[13]); append_byte(uuid[14]); append_byte(uuid[15]);
    }
    else {
        append_byte(uuid[0]); append_byte(uuid[1]); append_byte(uuid[2]); append_byte(uuid[3]);
        formatted << '-';
        append_byte(uuid[4]); append_byte(uuid[5]);
        formatted << '-';
        append_byte(uuid[6]); append_byte(uuid[7]);
        formatted << '-';
        append_byte(uuid[8]); append_byte(uuid[9]);
        formatted << '-';
        append_byte(uuid[10]); append_byte(uuid[11]); append_byte(uuid[12]);
        append_byte(uuid[13]); append_byte(uuid[14]); append_byte(uuid[15]);
    }

    return formatted.str();
}
