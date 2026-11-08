#include <smbios/tables/firmware_language_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::FirmwareLanguageInformation at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::FirmwareLanguageInformation, FirmwareLanguageInformationEntry)

FirmwareLanguageInformationEntry::FirmwareLanguageInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end), version_(version)
{
    if (header.type != SMBios::FirmwareLanguageInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Firmware Language Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }
}

std::string FirmwareLanguageInformationEntry::get_type() const
{
    return "Firmware Language Information";
}

std::string FirmwareLanguageInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Installable Languages: " << static_cast<unsigned>(get_installable_language_count()) << '\n';
    description << "Abbreviated Format: " << (is_abbreviated_format() ? "Yes" : "No") << '\n';
    description << "Current Language: " << get_current_language_string() << '\n';

    const uint8_t count = get_installable_language_count();
    for (uint8_t index = 0; index < count; ++index) {
        description << "Language " << static_cast<unsigned>(index) << ": " << get_language_at(index) << '\n';
    }

    return description.str();
}

uint8_t FirmwareLanguageInformationEntry::get_installable_language_count() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x05;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t FirmwareLanguageInformationEntry::get_flags() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x06;

    // DSP0134 3.9.0, 7.14 Table 58: the Flags field itself did not exist
    // before SMBIOS 2.1 - reads as 0 regardless of get_entry_size() for a
    // pre-2.1 table, same "not applicable, not garbage" default
    // SystemInformationEntry::format_uuid()'s own 2.6 gate established for
    // this codebase.
    if (version_ < SMBiosVersion{2, 1}) return 0;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

bool FirmwareLanguageInformationEntry::is_abbreviated_format() const
{
    return (get_flags() & Flags::AbbreviatedFormat) != 0;
}

uint8_t FirmwareLanguageInformationEntry::get_current_language_index() const
{
    constexpr size_t offset = 0x15;
    constexpr size_t min_length = 0x16;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

std::string FirmwareLanguageInformationEntry::get_current_language_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_current_language_index());
}

std::string FirmwareLanguageInformationEntry::get_language_at(size_t index) const
{
    // Per 7.14's own Example 1/Example 2 (see class doc comment's
    // "Discrepancy found" note): the Installable-Languages strings occupy
    // DMI string indices 1..Installable Languages directly, and Current
    // Language (0x15) is just a one-based index into that same list, not an
    // anchor the list starts "after."
    if (index >= get_installable_language_count()) return AbstractSMBiosEntry::dmi_string(0);

    const size_t string_index = index + 1;
    return AbstractSMBiosEntry::dmi_string(string_index);
}
