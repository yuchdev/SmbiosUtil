#include <libsmbios/tables/management_controller_host_interface_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::ManagementControllerHostInterface at
// static-init time; see REGISTER_SMBIOS_ENTRY's doc comment
// (smbios_entry_factory.hpp) for why this lives here rather than in the
// header.
REGISTER_SMBIOS_ENTRY(SMBios::ManagementControllerHostInterface, ManagementControllerHostInterfaceEntry)

namespace {

constexpr size_t kInterfaceTypeOffset = 0x04;
constexpr size_t kInterfaceSpecificDataLengthOffset = 0x05;
constexpr size_t kInterfaceSpecificDataOffset = 0x06;

constexpr size_t kInterfaceTypeMinLength = 0x05;
constexpr size_t kInterfaceSpecificDataLengthMinLength = 0x06;

} // namespace

ManagementControllerHostInterfaceEntry::ManagementControllerHostInterfaceEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::ManagementControllerHostInterface) {
        std::stringstream err;
        err << "Wrong entry type, expected Management Controller Host Interface, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // 3.2+ shape only (R8, see this class's own header doc comment) - every
    // field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.43/7.43.1/7.43.2
    // (Table 122/123/124), including the Table 123 redacted-gap
    // resolution.
    init_string_values();
}

void ManagementControllerHostInterfaceEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 124. Only the named boundary values are mapped
    // here; get_interface_type_string() handles the MCTP range and the
    // "all others reserved" fallback separately since the MCTP range
    // (0x00-0x3F) is assigned by DSP0239, not individually enumerable here.
    interface_type_map_[InterfaceTypeNetworkHostInterface] = "Network Host Interface";
    interface_type_map_[InterfaceTypeOem] = "OEM-defined";
}

std::string ManagementControllerHostInterfaceEntry::get_type() const
{
    return "Management Controller Host Interface";
}

std::string ManagementControllerHostInterfaceEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Interface Type: " << get_interface_type_string() << '\n';
    description << "Interface Type Specific Data Length: " << static_cast<unsigned>(get_interface_specific_data_length()) << '\n';

    const uint8_t record_count = get_protocol_record_count();
    description << "Protocol Record Count: " << static_cast<unsigned>(record_count) << '\n';
    for (size_t index = 0; index < record_count; ++index) {
        ProtocolRecord record = get_protocol_record_at(index);
        description << "\tProtocol Type: " << static_cast<unsigned>(record.protocol_type)
                     << ", Data Length: " << record.data.size() << '\n';
    }

    return description.str();
}

ManagementControllerHostInterfaceEntry::InterfaceType ManagementControllerHostInterfaceEntry::get_interface_type() const
{
    if (get_entry_size() < kInterfaceTypeMinLength) return InterfaceTypeOutOfSpec;
    return static_cast<InterfaceType>(read_u8(raw_data(), kInterfaceTypeOffset));
}

std::string ManagementControllerHostInterfaceEntry::get_interface_type_string() const
{
    const InterfaceType type = get_interface_type();
    if (type == InterfaceTypeOutOfSpec) return "Reserved";
    if (type <= InterfaceTypeMctpRangeHigh) return "MCTP Host Interface";

    auto it = interface_type_map_.find(type);
    if (it != interface_type_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

uint8_t ManagementControllerHostInterfaceEntry::get_interface_specific_data_length() const
{
    if (get_entry_size() < kInterfaceSpecificDataLengthMinLength) return 0;
    return read_u8(raw_data(), kInterfaceSpecificDataLengthOffset);
}

std::vector<uint8_t> ManagementControllerHostInterfaceEntry::get_interface_specific_data() const
{
    const uint8_t n = get_interface_specific_data_length();
    std::vector<uint8_t> data;
    for (size_t i = 0; i < n; ++i) {
        const size_t offset = kInterfaceSpecificDataOffset + i;
        if (get_entry_size() < offset + 1) break;
        data.push_back(read_u8(raw_data(), offset));
    }
    return data;
}

uint8_t ManagementControllerHostInterfaceEntry::get_protocol_record_count() const
{
    const uint8_t n = get_interface_specific_data_length();
    const size_t offset = kInterfaceSpecificDataOffset + n;
    if (get_entry_size() < offset + 1) return 0;
    return read_u8(raw_data(), offset);
}

ManagementControllerHostInterfaceEntry::ProtocolRecord ManagementControllerHostInterfaceEntry::get_protocol_record_at(size_t index) const
{
    ProtocolRecord result;

    if (index >= get_protocol_record_count()) return result;

    // Walk the variable-length Protocol Records array from its own start
    // (0x07+N, per Table 122) record-by-record: each record's own Protocol
    // Type Specific Data Length field determines where the next record
    // begins, so there is no fixed stride to compute `index`'s offset
    // directly - see this class's own header doc comment.
    const uint8_t n = get_interface_specific_data_length();
    size_t offset = kInterfaceSpecificDataOffset + n + 1; // 0x07+N: start of Protocol Records.

    for (size_t current = 0; current <= index; ++current) {
        // Protocol Type byte.
        if (get_entry_size() < offset + 1) return ProtocolRecord();
        const uint8_t protocol_type = read_u8(raw_data(), offset);

        // Protocol Type Specific Data Length byte.
        if (get_entry_size() < offset + 2) return ProtocolRecord();
        const uint8_t data_length = read_u8(raw_data(), offset + 1);

        // Protocol Type Specific Data, data_length bytes.
        if (get_entry_size() < offset + 2 + data_length) return ProtocolRecord();

        if (current == index) {
            result.protocol_type = protocol_type;
            result.data.reserve(data_length);
            for (size_t i = 0; i < data_length; ++i) {
                result.data.push_back(read_u8(raw_data(), offset + 2 + i));
            }
            return result;
        }

        offset += 2 + data_length;
    }

    return result;
}
