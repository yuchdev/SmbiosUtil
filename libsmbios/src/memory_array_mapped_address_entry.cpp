#include <libsmbios/tables/memory_array_mapped_address_entry.h>
#include <libsmbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <libsmbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>

using namespace smbios;

// Self-register against SMBios::MemoryArrayMappedAddress at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::MemoryArrayMappedAddress, MemoryArrayMappedAddressEntry)

MemoryArrayMappedAddressEntry::MemoryArrayMappedAddressEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::MemoryArrayMappedAddress) {
        std::stringstream err;
        err << "Wrong entry type, expected Memory Array Mapped Address, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length (plus the
    // 0xFFFFFFFF sentinel check named in the class doc comment), confirmed
    // against c-versions/dmidecode_linux's own dmi_decode() for type 19.
}

bool MemoryArrayMappedAddressEntry::uses_extended_addressing() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x0F;
    constexpr size_t extended_min_length = 0x1F;
    constexpr uint32_t sentinel = 0xFFFFFFFF;

    if (get_entry_size() < min_length) return false;
    return (read_u32(raw_data(), offset) == sentinel) && (get_entry_size() >= extended_min_length);
}

std::string MemoryArrayMappedAddressEntry::get_type() const
{
    return "Memory Array Mapped Address";
}

std::string MemoryArrayMappedAddressEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Starting Address: " << get_starting_address_string() << '\n';
    description << "Ending Address: " << get_ending_address_string() << '\n';
    description << "Physical Array Handle: " << get_memory_array_handle_string() << '\n';
    description << "Partition Width: " << get_partition_width_string() << '\n';

    return description.str();
}

uint64_t MemoryArrayMappedAddressEntry::get_starting_address() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;

    // 2.7+ widened field: see dmi_decode()'s own `DWORD(data + 0x04) ==
    // 0xFFFFFFFF` sentinel check for case 19 (a DIFFERENT sentinel value
    // than PhysicalMemoryArrayEntry's 0x80000000).
    if (uses_extended_addressing()) {
        constexpr size_t extended_offset = 0x0F;
        return read_u64(raw_data(), extended_offset);
    }

    const uint32_t legacy_address = read_u32(raw_data(), offset);
    if (legacy_address == 0xFFFFFFFF) {
        // Sentinel set but the structure is too short for the extended
        // field - genuinely unknown, not "fall back to the sentinel value
        // itself".
        return 0;
    }
    return static_cast<uint64_t>(legacy_address);
}

uint64_t MemoryArrayMappedAddressEntry::get_ending_address() const
{
    constexpr size_t offset = 0x08;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;

    // The sentinel is checked against the STARTING address field only (see
    // uses_extended_addressing()) - the ending address never carries its
    // own independent sentinel check, per the reference's own logic.
    if (uses_extended_addressing()) {
        constexpr size_t extended_offset = 0x17;
        return read_u64(raw_data(), extended_offset);
    }

    constexpr size_t starting_offset = 0x04;
    const uint32_t starting_address = read_u32(raw_data(), starting_offset);
    if (starting_address == 0xFFFFFFFF) {
        return 0;
    }
    return static_cast<uint64_t>(read_u32(raw_data(), offset));
}

uint16_t MemoryArrayMappedAddressEntry::get_memory_array_handle() const
{
    constexpr size_t offset = 0x0C;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0xFFFF;
    return read_u16(raw_data(), offset);
}

uint8_t MemoryArrayMappedAddressEntry::get_partition_width() const
{
    constexpr size_t offset = 0x0E;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

std::string MemoryArrayMappedAddressEntry::get_starting_address_string() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return "Unknown";

    const uint32_t legacy_address = read_u32(raw_data(), offset);
    if (legacy_address == 0xFFFFFFFF && !uses_extended_addressing()) {
        return "Unknown";
    }

    const bool extended = uses_extended_addressing();
    std::stringstream address;
    address << "0x" << std::hex << std::setfill('0') << std::setw(extended ? 16 : 8)
            << get_starting_address() << std::dec << (extended ? " bytes" : " kB");
    return address.str();
}

std::string MemoryArrayMappedAddressEntry::get_ending_address_string() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return "Unknown";

    const uint32_t legacy_address = read_u32(raw_data(), offset);
    if (legacy_address == 0xFFFFFFFF && !uses_extended_addressing()) {
        return "Unknown";
    }

    const bool extended = uses_extended_addressing();
    std::stringstream address;
    address << "0x" << std::hex << std::setfill('0') << std::setw(extended ? 16 : 8)
            << get_ending_address() << std::dec << (extended ? " bytes" : " kB");
    return address.str();
}

std::string MemoryArrayMappedAddressEntry::get_memory_array_handle_string() const
{
    std::stringstream handle_stream;
    handle_stream << "0x" << std::hex << std::setfill('0') << std::setw(4) << get_memory_array_handle() << std::dec;
    return handle_stream.str();
}

std::string MemoryArrayMappedAddressEntry::get_partition_width_string() const
{
    return std::to_string(static_cast<unsigned>(get_partition_width()));
}
