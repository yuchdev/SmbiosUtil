#include <libsmbios/tables/oem_strings_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::OemStrings at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::OemStrings, OemStringsEntry)

OemStringsEntry::OemStringsEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::OemStrings) {
        std::stringstream err;
        err << "Wrong entry type, expected OEM Strings, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }
}

std::string OemStringsEntry::get_type() const
{
    return "OEM Strings";
}

std::string OemStringsEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    const uint8_t count = get_string_count();
    description << "String Count: " << static_cast<unsigned>(count) << '\n';
    for (unsigned index = 1; index <= count; ++index) {
        description << "String " << static_cast<unsigned>(index) << ": " << get_string_at(index) << '\n';
    }

    return description.str();
}

uint8_t OemStringsEntry::get_string_count() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x05;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

std::string OemStringsEntry::get_string_at(size_t index) const
{
    return AbstractSMBiosEntry::dmi_string(index);
}

std::vector<NamedField> OemStringsEntry::get_fields() const
{
    std::vector<NamedField> fields;
    const uint8_t count = get_string_count();
    fields.push_back({"String Count", static_cast<uint64_t>(count)});
    for (unsigned index = 1; index <= count; ++index) {
        fields.push_back({"String " + std::to_string(static_cast<unsigned>(index)), get_string_at(index)});
    }
    return fields;
}
