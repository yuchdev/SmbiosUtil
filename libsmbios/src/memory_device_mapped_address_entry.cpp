#include <libsmbios/tables/memory_device_mapped_address_entry.h>
#include <libsmbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <libsmbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>

using namespace smbios;

// Self-register against SMBios::MemoryDeviceMappedAddress at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::MemoryDeviceMappedAddress, MemoryDeviceMappedAddressEntry)

namespace {

/// Minimum structure length for every base (2.1+) field - matches the
/// full 2.1 structure length (0x13) named by DSP0134 3.9.0 Table 86,
/// rather than a per-field granular guard, matching
/// PhysicalMemoryArrayEntry's/MemoryArrayMappedAddressEntry's own
/// precedent of a single tier-length guard per version.
constexpr size_t kBaseMinLength = 0x13;

/// Minimum structure length for the 2.7+ extended address fields (0x13 +
/// 8-byte Extended Starting Address + 8-byte Extended Ending Address).
constexpr size_t kExtendedMinLength = 0x23;

constexpr uint32_t kLegacyAddressSentinel = 0xFFFFFFFF;

constexpr size_t kStartingAddressOffset = 0x04;
constexpr size_t kEndingAddressOffset = 0x08;
constexpr size_t kMemoryDeviceHandleOffset = 0x0C;
constexpr size_t kMemoryArrayMappedAddressHandleOffset = 0x0E;
constexpr size_t kPartitionRowPositionOffset = 0x10;
constexpr size_t kInterleavePositionOffset = 0x11;
constexpr size_t kExtendedStartingAddressOffset = 0x13;
constexpr size_t kExtendedEndingAddressOffset = 0x1B;

/// Legacy fields are in kibibytes; normalize to bytes so
/// get_starting_address()/get_ending_address() never expose the unit the
/// legacy storage happened to use.
constexpr uint64_t kibibytes_to_bytes(uint32_t kibibytes)
{
    return static_cast<uint64_t>(kibibytes) * 1024ULL;
}

} // namespace

MemoryDeviceMappedAddressEntry::MemoryDeviceMappedAddressEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::MemoryDeviceMappedAddress) {
        std::stringstream err;
        err << "Wrong entry type, expected Memory Device Mapped Address, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length (plus the
    // 0xFFFFFFFF sentinel check named in the class doc comment), confirmed
    // against DSP0134 3.9.0 Table 86 and c-versions/dmidecode_linux's own
    // dmi_decode() for type 20.
}

bool MemoryDeviceMappedAddressEntry::uses_extended_addressing() const
{
    if (get_entry_size() < kBaseMinLength) return false;
    return (read_u32(raw_data(), kStartingAddressOffset) == kLegacyAddressSentinel)
        && (get_entry_size() >= kExtendedMinLength);
}

std::string MemoryDeviceMappedAddressEntry::get_type() const
{
    return "Memory Device Mapped Address";
}

std::string MemoryDeviceMappedAddressEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Starting Address: " << get_starting_address_string() << '\n';
    description << "Ending Address: " << get_ending_address_string() << '\n';
    description << "Memory Device Handle: " << get_memory_device_handle_string() << '\n';
    description << "Memory Array Mapped Address Handle: " << get_memory_array_mapped_address_handle_string() << '\n';
    description << "Partition Row Position: " << get_partition_row_position_string() << '\n';
    description << "Interleave Position: " << get_interleave_position_string() << '\n';

    return description.str();
}

uint64_t MemoryDeviceMappedAddressEntry::get_starting_address() const
{
    if (get_entry_size() < kBaseMinLength) return 0;

    // 2.7+ widened field: see dmi_decode()'s own `DWORD(data + 0x04) ==
    // 0xFFFFFFFF` sentinel check for case 20.
    if (uses_extended_addressing()) {
        return read_u64(raw_data(), kExtendedStartingAddressOffset);
    }

    const uint32_t legacy_address = read_u32(raw_data(), kStartingAddressOffset);
    if (legacy_address == kLegacyAddressSentinel) {
        // Sentinel set but the structure is too short for the extended
        // field - genuinely unknown, not "fall back to the sentinel value
        // itself".
        return 0;
    }
    // Legacy value is in kibibytes - normalize to bytes.
    return kibibytes_to_bytes(legacy_address);
}

uint64_t MemoryDeviceMappedAddressEntry::get_ending_address() const
{
    if (get_entry_size() < kBaseMinLength) return 0;

    // The sentinel is checked against the STARTING address field only (see
    // uses_extended_addressing()) - the ending address never carries its
    // own independent sentinel check, per the reference's own logic.
    if (uses_extended_addressing()) {
        return read_u64(raw_data(), kExtendedEndingAddressOffset);
    }

    const uint32_t starting_address = read_u32(raw_data(), kStartingAddressOffset);
    if (starting_address == kLegacyAddressSentinel) {
        return 0;
    }
    // Legacy value is in kibibytes - normalize to bytes.
    return kibibytes_to_bytes(read_u32(raw_data(), kEndingAddressOffset));
}

uint16_t MemoryDeviceMappedAddressEntry::get_memory_device_handle() const
{
    if (get_entry_size() < kBaseMinLength) return 0xFFFF;
    return read_u16(raw_data(), kMemoryDeviceHandleOffset);
}

uint16_t MemoryDeviceMappedAddressEntry::get_memory_array_mapped_address_handle() const
{
    if (get_entry_size() < kBaseMinLength) return 0xFFFF;
    return read_u16(raw_data(), kMemoryArrayMappedAddressHandleOffset);
}

uint8_t MemoryDeviceMappedAddressEntry::get_partition_row_position() const
{
    if (get_entry_size() < kBaseMinLength) return PartitionRowPositionUnknown;
    return read_u8(raw_data(), kPartitionRowPositionOffset);
}

uint8_t MemoryDeviceMappedAddressEntry::get_interleave_position() const
{
    if (get_entry_size() < kBaseMinLength) return InterleavePositionUnknown;
    return read_u8(raw_data(), kInterleavePositionOffset);
}

std::string MemoryDeviceMappedAddressEntry::get_starting_address_string() const
{
    if (get_entry_size() < kBaseMinLength) return "Unknown";

    const uint32_t legacy_address = read_u32(raw_data(), kStartingAddressOffset);
    if (legacy_address == kLegacyAddressSentinel && !uses_extended_addressing()) {
        return "Unknown";
    }

    std::stringstream address;
    address << "0x" << std::hex << std::setfill('0') << std::setw(16)
            << get_starting_address() << std::dec << " bytes";
    return address.str();
}

std::string MemoryDeviceMappedAddressEntry::get_ending_address_string() const
{
    if (get_entry_size() < kBaseMinLength) return "Unknown";

    const uint32_t legacy_address = read_u32(raw_data(), kStartingAddressOffset);
    if (legacy_address == kLegacyAddressSentinel && !uses_extended_addressing()) {
        return "Unknown";
    }

    std::stringstream address;
    address << "0x" << std::hex << std::setfill('0') << std::setw(16)
            << get_ending_address() << std::dec << " bytes";
    return address.str();
}

std::string MemoryDeviceMappedAddressEntry::get_memory_device_handle_string() const
{
    std::stringstream handle_stream;
    handle_stream << "0x" << std::hex << std::setfill('0') << std::setw(4) << get_memory_device_handle() << std::dec;
    return handle_stream.str();
}

std::string MemoryDeviceMappedAddressEntry::get_memory_array_mapped_address_handle_string() const
{
    std::stringstream handle_stream;
    handle_stream << "0x" << std::hex << std::setfill('0') << std::setw(4) << get_memory_array_mapped_address_handle() << std::dec;
    return handle_stream.str();
}

std::string MemoryDeviceMappedAddressEntry::get_partition_row_position_string() const
{
    const uint8_t position = get_partition_row_position();
    // 0 is reserved (never a valid position) - distinct from 0xFF
    // ("unknown"), per the class doc comment.
    if (PartitionRowPositionReserved == position) {
        return "Reserved";
    }
    if (PartitionRowPositionUnknown == position) {
        return "Unknown";
    }
    return std::to_string(static_cast<unsigned>(position));
}

std::string MemoryDeviceMappedAddressEntry::get_interleave_position_string() const
{
    const uint8_t position = get_interleave_position();
    // 0 means "non-interleaved" here - a valid, meaningful value, unlike
    // Partition Row Position's reserved 0 - distinct from 0xFF
    // ("unknown"), per the class doc comment.
    if (InterleavePositionNonInterleaved == position) {
        return "Non-interleaved";
    }
    if (InterleavePositionUnknown == position) {
        return "Unknown";
    }
    return std::to_string(static_cast<unsigned>(position));
}
