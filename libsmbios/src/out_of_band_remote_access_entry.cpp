#include <libsmbios/tables/out_of_band_remote_access_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::OutOfBandRemoteAccess at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::OutOfBandRemoteAccess, OutOfBandRemoteAccessEntry)

namespace {

constexpr size_t kManufacturerNameOffset = 0x04;
constexpr size_t kConnectionsOffset = 0x05;

constexpr size_t kManufacturerNameMinLength = 0x05;
constexpr size_t kConnectionsMinLength = 0x06;

/// Connections bit positions/masks, DSP0134 3.9.0 Table 103 (see
/// out_of_band_remote_access_entry.hpp's class doc comment for the full
/// confirmed layout).
constexpr uint8_t kInboundConnectionEnabledMask = 0x01; // Bit 0
constexpr uint8_t kOutboundConnectionEnabledMask = 0x02; // Bit 1

} // namespace

OutOfBandRemoteAccessEntry::OutOfBandRemoteAccessEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::OutOfBandRemoteAccess) {
        std::stringstream err;
        err << "Wrong entry type, expected Out-of-Band Remote Access, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.31 (Table 103).
}

std::string OutOfBandRemoteAccessEntry::get_type() const
{
    return "Out-of-Band Remote Access";
}

std::string OutOfBandRemoteAccessEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Manufacturer Name: " << get_manufacturer_name_string() << '\n';
    description << "Inbound Connection Enabled: " << (is_inbound_connection_enabled() ? "Yes" : "No") << '\n';
    description << "Outbound Connection Enabled: " << (is_outbound_connection_enabled() ? "Yes" : "No") << '\n';

    return description.str();
}

std::string OutOfBandRemoteAccessEntry::get_manufacturer_name_string() const
{
    if (get_entry_size() < kManufacturerNameMinLength) return std::string();
    return dmi_string(read_u8(raw_data(), kManufacturerNameOffset));
}

uint8_t OutOfBandRemoteAccessEntry::get_connections() const
{
    if (get_entry_size() < kConnectionsMinLength) return 0;
    return read_u8(raw_data(), kConnectionsOffset);
}

bool OutOfBandRemoteAccessEntry::is_inbound_connection_enabled() const
{
    return (get_connections() & kInboundConnectionEnabledMask) != 0;
}

bool OutOfBandRemoteAccessEntry::is_outbound_connection_enabled() const
{
    return (get_connections() & kOutboundConnectionEnabledMask) != 0;
}

std::vector<NamedField> OutOfBandRemoteAccessEntry::get_fields() const
{
    std::vector<NamedField> fields;
    fields.push_back({"Manufacturer Name", get_manufacturer_name_string()});
    fields.push_back({"Inbound Connection Enabled", is_inbound_connection_enabled()});
    fields.push_back({"Outbound Connection Enabled", is_outbound_connection_enabled()});
    return fields;
}
