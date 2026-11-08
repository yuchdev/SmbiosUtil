#include <smbios/tables/baseboard_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::BaseboardInformation at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::BaseboardInformation, BaseboardInformationEntry)

BaseboardInformationEntry::BaseboardInformationEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::BaseboardInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Baseboard Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone (R2),
    // confirmed against docs/standard/DSP0134_3.9.0.md's own 7.3/7.3.1/7.3.2
    // and cross-checked against c-versions/dmidecode_linux's own
    // dmi_decode() case 2 ("Base Board Information").
    init_string_values();
}

void BaseboardInformationEntry::init_string_values()
{
    // DSP0134 3.9.0, 7.3.1 Table 14 - bits 5:7 are reserved (set to 000b).
    feature_flags_map_[HostingBoard] = "Board is a hosting board";
    feature_flags_map_[RequiresDaughterBoard] = "Board requires at least one daughter board";
    feature_flags_map_[Removable] = "Board is removable";
    feature_flags_map_[Replaceable] = "Board is replaceable";
    feature_flags_map_[HotSwappable] = "Board is hot swappable";

    // DSP0134 3.9.0, 7.3.2 Table 15.
    board_type_map_[Unknown] = "Unknown";
    board_type_map_[Other] = "Other";
    board_type_map_[ServerBlade] = "Server Blade";
    board_type_map_[ConnectivitySwitch] = "Connectivity Switch";
    board_type_map_[SystemManagementModule] = "System Management Module";
    board_type_map_[ProcessorModule] = "Processor Module";
    board_type_map_[IOModule] = "I/O Module";
    board_type_map_[MemoryModule] = "Memory Module";
    board_type_map_[DaughterBoard] = "Daughter Board";
    board_type_map_[Motherboard] = "Motherboard (includes processor, memory, and I/O)";
    board_type_map_[ProcessorMemoryModule] = "Processor/Memory Module";
    board_type_map_[ProcessorIOModule] = "Processor/IO Module";
    board_type_map_[InterconnectBoard] = "Interconnect Board";
}

std::string BaseboardInformationEntry::get_type() const
{
    return "Baseboard Information";
}

std::string BaseboardInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Manufacturer: " << get_manufacturer_string() << '\n';
    description << "Product Name: " << get_product_string() << '\n';
    description << "Version: " << get_version_string() << '\n';
    description << "Serial Number: " << get_serial_number_string() << '\n';
    description << "Asset Tag: " << get_asset_tag_string() << '\n';
    description << "Features: " << '\n' << get_feature_flags_string();
    description << "Location In Chassis: " << get_location_in_chassis_string() << '\n';
    description << "Chassis Handle: " << get_chassis_handle() << '\n';
    description << "Type: " << get_board_type_string() << '\n';
    description << "Contained Object Handles: " << static_cast<unsigned>(get_contained_object_handle_count()) << '\n';

    return description.str();
}

uint8_t BaseboardInformationEntry::get_manufacturer_index() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x05;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t BaseboardInformationEntry::get_product_index() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x06;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t BaseboardInformationEntry::get_version_index() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x07;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t BaseboardInformationEntry::get_serial_number_index() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x08;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t BaseboardInformationEntry::get_asset_tag_index() const
{
    constexpr size_t offset = 0x08;
    constexpr size_t min_length = 0x09;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t BaseboardInformationEntry::get_feature_flags() const
{
    constexpr size_t offset = 0x09;
    constexpr size_t min_length = 0x0A;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t BaseboardInformationEntry::get_location_in_chassis_index() const
{
    constexpr size_t offset = 0x0A;
    constexpr size_t min_length = 0x0B;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t BaseboardInformationEntry::get_chassis_handle() const
{
    constexpr size_t offset = 0x0B;
    constexpr size_t min_length = 0x0D;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint8_t BaseboardInformationEntry::get_board_type() const
{
    constexpr size_t offset = 0x0D;
    constexpr size_t min_length = 0x0E;
    if (get_entry_size() < min_length) return BoardType::BoardTypeOutOfSpec;

    const uint8_t board_type = read_u8(raw_data(), offset);
    auto it = board_type_map_.find(board_type);
    if (it != board_type_map_.end()) {
        return (*it).first;
    }
    return BoardType::BoardTypeOutOfSpec;
}

uint8_t BaseboardInformationEntry::get_contained_object_handle_count() const
{
    constexpr size_t offset = 0x0E;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t BaseboardInformationEntry::get_contained_object_handle_at(size_t index) const
{
    if (index >= get_contained_object_handle_count()) return 0;

    const size_t offset = 0x0F + index * 2;
    const size_t min_length = offset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

std::string BaseboardInformationEntry::get_manufacturer_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_manufacturer_index());
}

std::string BaseboardInformationEntry::get_product_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_product_index());
}

std::string BaseboardInformationEntry::get_version_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_version_index());
}

std::string BaseboardInformationEntry::get_serial_number_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_serial_number_index());
}

std::string BaseboardInformationEntry::get_asset_tag_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_asset_tag_index());
}

std::string BaseboardInformationEntry::get_feature_flags_string() const
{
    // DSP0134 3.9.0, 7.3.1 Table 14, footnote 1: the feature flags are only
    // meaningful ("present") when the structure carries the Number of
    // Contained Object Handles field (length >= 0x0E) AND that count is
    // non-zero - distinct from whether the raw byte itself is readable
    // (see get_feature_flags()). Mirrors SystemSlotsEntry::
    // get_characteristics_string()'s own "Unknown" vs "None" distinction,
    // here "Not Present" vs "None".
    if (get_entry_size() < 0x0E || get_contained_object_handle_count() == 0) {
        return "\tNot Present\n";
    }

    const uint8_t flags = get_feature_flags();
    if (flags == 0) {
        return "\tNone\n";
    }

    return AbstractSMBiosEntry::bitset_to_properties<uint8_t>(flags, feature_flags_map_);
}

std::string BaseboardInformationEntry::get_location_in_chassis_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_location_in_chassis_index());
}

std::string BaseboardInformationEntry::get_board_type_string() const
{
    assert(!board_type_map_.empty());
    auto it = board_type_map_.find(get_board_type());
    if (it != board_type_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}
