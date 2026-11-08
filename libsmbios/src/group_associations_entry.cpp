#include <smbios/tables/group_associations_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::GroupAssociations at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::GroupAssociations, GroupAssociationsEntry)

GroupAssociationsEntry::GroupAssociationsEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::GroupAssociations) {
        std::stringstream err;
        err << "Wrong entry type, expected Group Associations, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone (R2),
    // confirmed against docs/standard/DSP0134_3.9.0.md's own 7.15 (Table 59)
    // and its own note/worked examples - this structure has no
    // version-gated fields.
    init_string_values();
}

void GroupAssociationsEntry::init_string_values()
{
    // DSP0134 3.9.0, 7.15: Item Type values are SMBios::SMBiosHandler
    // values naming the *other* structure type an item refers to. Only the
    // subset SMBios::SMBiosHandler itself knows a name for is listed here;
    // get_item_type_string_at() falls back to the raw numeric value for
    // anything else.
    item_type_names_[SMBios::BIOSInformation] = "BIOS Information";
    item_type_names_[SMBios::SystemInformation] = "System Information";
    item_type_names_[SMBios::BaseboardInformation] = "Baseboard Information";
    item_type_names_[SMBios::SystemEnclosure] = "System Enclosure";
    item_type_names_[SMBios::ProcessorInformation] = "Processor Information";
    item_type_names_[SMBios::CacheInformation] = "Cache Information";
    item_type_names_[SMBios::PortConnection] = "Port Connection";
    item_type_names_[SMBios::SystemSlots] = "System Slots";
    item_type_names_[SMBios::OemStrings] = "OEM Strings";
    item_type_names_[SMBios::SystemConfigurationOptions] = "System Configuration Options";
    item_type_names_[SMBios::FirmwareLanguageInformation] = "Firmware Language Information";
    item_type_names_[SMBios::GroupAssociations] = "Group Associations";
    item_type_names_[SMBios::PhysicalMemoryArray] = "Physical Memory Array";
    item_type_names_[SMBios::MemoryDevice] = "Memory Device";
    item_type_names_[SMBios::MemoryArrayMappedAddress] = "Memory Array Mapped Address";
    item_type_names_[SMBios::SystemBootInformation] = "System Boot Information";
}

std::string GroupAssociationsEntry::get_type() const
{
    return "Group Associations";
}

std::string GroupAssociationsEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Group Name: " << get_group_name_string() << '\n';

    const size_t count = get_item_count();
    description << "Items: " << count << '\n';
    for (size_t index = 0; index < count; ++index) {
        description << "\tItem Type: " << get_item_type_string_at(index)
                     << ", Item Handle: " << get_item_handle_at(index) << '\n';
    }

    return description.str();
}

uint8_t GroupAssociationsEntry::get_group_name_index() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x05;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

size_t GroupAssociationsEntry::get_item_count() const
{
    // DSP0134 3.9.0, 7.15 Table 59: "The user of this structure determines
    // the number of items as (Length - 5) / 3." There is no count field to
    // read - this is purely derived from get_entry_size() (the structure's
    // Length). Integer division truncates for a malformed Length that does
    // not divide evenly by 3 after subtracting 5; that truncation is
    // harmless here, but get_item_type_at()/get_item_handle_at() still
    // guard independently against get_entry_size() since this formula alone
    // does not prove every index it allows is actually in-bounds.
    constexpr size_t min_length = 5;
    if (get_entry_size() < min_length) return 0;
    return (get_entry_size() - min_length) / 3;
}

uint8_t GroupAssociationsEntry::get_item_type_at(size_t index) const
{
    const size_t offset = 0x05 + 3 * index;
    const size_t min_length = offset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t GroupAssociationsEntry::get_item_handle_at(size_t index) const
{
    const size_t offset = 0x06 + 3 * index;
    const size_t min_length = offset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

std::string GroupAssociationsEntry::get_group_name_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_group_name_index());
}

std::string GroupAssociationsEntry::get_item_type_string_at(size_t index) const
{
    const size_t offset = 0x05 + 3 * index;
    const size_t min_length = offset + 1;
    if (get_entry_size() < min_length) return "Unknown (0)";

    const uint8_t item_type = get_item_type_at(index);
    auto it = item_type_names_.find(item_type);
    if (it != item_type_names_.end()) {
        return (*it).second;
    }

    std::stringstream unknown;
    unknown << "Unknown (" << static_cast<unsigned>(item_type) << ")";
    return unknown.str();
}
