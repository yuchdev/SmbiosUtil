#include <libsmbios/tables/string_property_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::StringProperty at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::StringProperty, StringPropertyEntry)

namespace {

constexpr size_t kPropertyIdOffset = 0x04;
constexpr size_t kValueOffset = 0x06;
constexpr size_t kParentHandleOffset = 0x07;

} // namespace

StringPropertyEntry::StringPropertyEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::StringProperty) {
        std::stringstream err;
        err << "Wrong entry type, expected String Property, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.47/7.47.1 (Table
    // 138/139).
}

std::string StringPropertyEntry::get_type() const
{
    return "String Property";
}

std::string StringPropertyEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Property ID: 0x" << std::hex << get_property_id() << std::dec
                 << " (" << get_property_id_string() << ")\n";
    description << "Value: " << get_value_string() << '\n';
    description << "Parent Handle: 0x" << std::hex << get_parent_handle() << std::dec << '\n';

    return description.str();
}

uint16_t StringPropertyEntry::get_property_id() const
{
    constexpr size_t min_length = kPropertyIdOffset + 2;
    if (get_entry_size() < min_length) return Reserved;
    return read_u16(raw_data(), kPropertyIdOffset);
}

std::string StringPropertyEntry::get_property_id_string() const
{
    const uint16_t value = get_property_id();

    if (value == Reserved) return "Reserved";
    if (value == UefiDevicePath) return "UEFI device path";

    // DSP0134 3.9.0 Table 139 (7.47.1): three distinct reserved *ranges*,
    // rendered distinctly - see this accessor's own header doc comment for
    // why a flat "Unknown" would lose information the spec conveys.
    if (value >= 2 && value <= 32767) return "Reserved (DMTF)";
    if (value >= 32768 && value <= 49151) return "Reserved (Firmware Vendor)";
    return "Reserved (OEM)"; // 49152-65535
}

std::string StringPropertyEntry::get_value_string() const
{
    constexpr size_t min_length = kValueOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kValueOffset));
}

uint16_t StringPropertyEntry::get_parent_handle() const
{
    constexpr size_t min_length = kParentHandleOffset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), kParentHandleOffset);
}
