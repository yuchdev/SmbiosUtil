#include <libsmbios/tables/memory_error_information_64bit_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>
#include <iomanip>

using namespace smbios;

// Self-register against SMBios::MemoryErrorInformation64Bit at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::MemoryErrorInformation64Bit, MemoryErrorInformation64BitEntry)

namespace {
    // DSP0134 3.9.0, 7.34 Table 106: Vendor Syndrome's "unknown" marker is
    // all-zero - a different convention than the address/resolution
    // sentinels below.
    constexpr uint32_t kVendorSyndromeUnknown = 0x00000000;

    // QWORD sentinel for the two widened (64-bit) address fields.
    constexpr uint64_t kAddressUnknown64 = 0x8000000000000000ULL;

    // DWORD sentinel for Error Resolution - unchanged in width and value
    // from the 32-bit structure's own 0x13 field; see this class's header
    // comment on Table 106's confirmed width asymmetry.
    constexpr uint32_t kResolutionUnknown32 = 0x80000000;
}

MemoryErrorInformation64BitEntry::MemoryErrorInformation64BitEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::MemoryErrorInformation64Bit) {
        std::stringstream err;
        err << "Wrong entry type, expected 64-Bit Memory Error Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    init_string_values();
}

void MemoryErrorInformation64BitEntry::init_string_values()
{
    // DSP0134 3.9.0, 7.19.1 Table 82 - Memory Error: Error Type field
    // (shared with the 32-bit structure, duplicated verbatim per this
    // class's own header comment).
    error_type_names_[0x01] = "Other";
    error_type_names_[0x02] = "Unknown";
    error_type_names_[0x03] = "OK";
    error_type_names_[0x04] = "Bad read";
    error_type_names_[0x05] = "Parity error";
    error_type_names_[0x06] = "Single-bit error";
    error_type_names_[0x07] = "Double-bit error";
    error_type_names_[0x08] = "Multi-bit error";
    error_type_names_[0x09] = "Nibble error";
    error_type_names_[0x0A] = "Checksum error";
    error_type_names_[0x0B] = "CRC error";
    error_type_names_[0x0C] = "Corrected single-bit error";
    error_type_names_[0x0D] = "Corrected error";
    error_type_names_[0x0E] = "Uncorrectable error";

    // DSP0134 3.9.0, 7.19.2 Table 83 - Memory Error: Error Granularity field.
    error_granularity_names_[0x01] = "Other";
    error_granularity_names_[0x02] = "Unknown";
    error_granularity_names_[0x03] = "Device level";
    error_granularity_names_[0x04] = "Memory partition level";

    // DSP0134 3.9.0, 7.19.3 Table 84 - Memory Error: Error Operation field.
    error_operation_names_[0x01] = "Other";
    error_operation_names_[0x02] = "Unknown";
    error_operation_names_[0x03] = "Read";
    error_operation_names_[0x04] = "Write";
    error_operation_names_[0x05] = "Partial write";
}

std::string MemoryErrorInformation64BitEntry::get_type() const
{
    return "64-Bit Memory Error Information";
}

std::string MemoryErrorInformation64BitEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Error Type: " << get_error_type_string() << '\n';
    description << "Error Granularity: " << get_error_granularity_string() << '\n';
    description << "Error Operation: " << get_error_operation_string() << '\n';

    description << "Vendor Syndrome: ";
    if (is_vendor_syndrome_known()) {
        description << "0x" << std::hex << std::setfill('0') << std::setw(8) << get_vendor_syndrome() << std::dec;
    } else {
        description << "Unknown";
    }
    description << '\n';

    description << "Memory Array Error Address: ";
    if (is_memory_array_error_address_known()) {
        description << "0x" << std::hex << std::setfill('0') << std::setw(16) << get_memory_array_error_address() << std::dec;
    } else {
        description << "Unknown";
    }
    description << '\n';

    description << "Device Error Address: ";
    if (is_device_error_address_known()) {
        description << "0x" << std::hex << std::setfill('0') << std::setw(16) << get_device_error_address() << std::dec;
    } else {
        description << "Unknown";
    }
    description << '\n';

    description << "Error Resolution: ";
    if (is_error_resolution_known()) {
        description << "0x" << std::hex << std::setfill('0') << std::setw(8) << get_error_resolution() << std::dec;
    } else {
        description << "Unknown";
    }
    description << '\n';

    return description.str();
}

uint8_t MemoryErrorInformation64BitEntry::get_error_type() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x05;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t MemoryErrorInformation64BitEntry::get_error_granularity() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x06;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t MemoryErrorInformation64BitEntry::get_error_operation() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x07;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint32_t MemoryErrorInformation64BitEntry::get_vendor_syndrome() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x0B;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), offset);
}

bool MemoryErrorInformation64BitEntry::is_vendor_syndrome_known() const
{
    constexpr size_t min_length = 0x0B;
    if (get_entry_size() < min_length) return false;
    return get_vendor_syndrome() != kVendorSyndromeUnknown;
}

uint64_t MemoryErrorInformation64BitEntry::get_memory_array_error_address() const
{
    constexpr size_t offset = 0x0B;
    constexpr size_t min_length = 0x13;
    if (get_entry_size() < min_length) return 0;
    return read_u64(raw_data(), offset);
}

bool MemoryErrorInformation64BitEntry::is_memory_array_error_address_known() const
{
    constexpr size_t min_length = 0x13;
    if (get_entry_size() < min_length) return false;
    return get_memory_array_error_address() != kAddressUnknown64;
}

uint64_t MemoryErrorInformation64BitEntry::get_device_error_address() const
{
    constexpr size_t offset = 0x13;
    constexpr size_t min_length = 0x1B;
    if (get_entry_size() < min_length) return 0;
    return read_u64(raw_data(), offset);
}

bool MemoryErrorInformation64BitEntry::is_device_error_address_known() const
{
    constexpr size_t min_length = 0x1B;
    if (get_entry_size() < min_length) return false;
    return get_device_error_address() != kAddressUnknown64;
}

uint32_t MemoryErrorInformation64BitEntry::get_error_resolution() const
{
    // NOT widened to QWORD - confirmed against Table 106 directly; see this
    // class's header comment.
    constexpr size_t offset = 0x1B;
    constexpr size_t min_length = 0x1F;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), offset);
}

bool MemoryErrorInformation64BitEntry::is_error_resolution_known() const
{
    constexpr size_t min_length = 0x1F;
    if (get_entry_size() < min_length) return false;
    return get_error_resolution() != kResolutionUnknown32;
}

std::string MemoryErrorInformation64BitEntry::get_error_type_string() const
{
    auto it = error_type_names_.find(get_error_type());
    return it != error_type_names_.end() ? (*it).second : "Unknown";
}

std::string MemoryErrorInformation64BitEntry::get_error_granularity_string() const
{
    auto it = error_granularity_names_.find(get_error_granularity());
    return it != error_granularity_names_.end() ? (*it).second : "Unknown";
}

std::string MemoryErrorInformation64BitEntry::get_error_operation_string() const
{
    auto it = error_operation_names_.find(get_error_operation());
    return it != error_operation_names_.end() ? (*it).second : "Unknown";
}
