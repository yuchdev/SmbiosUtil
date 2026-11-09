#include <libsmbios/tables/system_event_log_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>
#include <iomanip>

using namespace smbios;

// Self-register against SMBios::SystemEventLog at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemEventLog, SystemEventLogEntry)

namespace {
    // DSP0134 3.9.0, 7.16 Table 60: version 2.0 implementations stop at
    // offset 0x14 (Length == 0x14h); every field from 0x14 onward requires
    // SMBiosVersion >= {2, 1}.
    const SMBiosVersion kVersion21{2, 1};
}

SystemEventLogEntry::SystemEventLogEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end), version_(version)
{
    if (header.type != SMBios::SystemEventLog) {
        std::stringstream err;
        err << "Wrong entry type, expected System Event Log, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    init_string_values();
}

void SystemEventLogEntry::init_string_values()
{
    // DSP0134 3.9.0, 7.16 Table 60, Access Method row - confirmed directly
    // against the spec file; see this class's header for the "redacted row"
    // note.
    access_method_names_[OneIndexOneDataPort8Bit] = "Indexed I/O: 1 8-bit index port, 1 8-bit data port";
    access_method_names_[TwoIndexOneDataPort8Bit] = "Indexed I/O: 2 8-bit index ports, 1 8-bit data port";
    access_method_names_[OneIndexOneDataPort16Bit] = "Indexed I/O: 1 16-bit index port, 1 8-bit data port";
    access_method_names_[MemoryMappedPhysical32Bit] = "Memory-mapped physical 32-bit address";
    access_method_names_[GeneralPurposeNonVolatileData] = "General-Purpose NonVolatile Data functions";
}

std::string SystemEventLogEntry::get_type() const
{
    return "System Event Log";
}

std::string SystemEventLogEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Log Area Length: " << get_log_area_length() << '\n';
    description << "Log Header Start Offset: " << get_log_header_start_offset() << '\n';
    description << "Log Data Start Offset: " << get_log_data_start_offset() << '\n';
    description << "Access Method: " << get_access_method_string() << '\n';
    description << "Log Status: Valid = " << (is_log_area_valid() ? "Yes" : "No")
                 << ", Full = " << (is_log_area_full() ? "Yes" : "No") << '\n';
    description << "Log Change Token: " << get_log_change_token() << '\n';
    description << "Access Method Address: " << get_access_method_address() << '\n';

    if (!(version_ < kVersion21) && get_entry_size() >= 0x17) {
        description << "Log Header Format: " << static_cast<unsigned>(get_log_header_format()) << '\n';
        const uint8_t count = get_supported_descriptor_count();
        description << "Supported Log Type Descriptors: " << static_cast<unsigned>(count) << '\n';
        for (uint8_t index = 0; index < count; ++index) {
            description << "\tDescriptor " << static_cast<unsigned>(index) << ": 0x"
                         << std::hex << get_log_type_descriptor_at(index) << std::dec << '\n';
        }
    }

    return description.str();
}

uint16_t SystemEventLogEntry::get_log_area_length() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x06;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint16_t SystemEventLogEntry::get_log_header_start_offset() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x08;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint16_t SystemEventLogEntry::get_log_data_start_offset() const
{
    constexpr size_t offset = 0x08;
    constexpr size_t min_length = 0x0A;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint8_t SystemEventLogEntry::get_access_method() const
{
    constexpr size_t offset = 0x0A;
    constexpr size_t min_length = 0x0B;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEventLogEntry::get_log_status() const
{
    constexpr size_t offset = 0x0B;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

bool SystemEventLogEntry::is_log_area_valid() const
{
    return (get_log_status() & LogStatus::LogAreaValid) != 0;
}

bool SystemEventLogEntry::is_log_area_full() const
{
    return (get_log_status() & LogStatus::LogAreaFull) != 0;
}

uint32_t SystemEventLogEntry::get_log_change_token() const
{
    constexpr size_t offset = 0x0C;
    constexpr size_t min_length = 0x10;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), offset);
}

uint32_t SystemEventLogEntry::get_access_method_address() const
{
    constexpr size_t offset = 0x10;
    constexpr size_t min_length = 0x14;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), offset);
}

uint8_t SystemEventLogEntry::get_log_header_format() const
{
    constexpr size_t offset = 0x14;
    constexpr size_t min_length = 0x15;

    // DSP0134 3.9.0, 7.16 Table 60: this field did not exist before 2.1 -
    // guarded by BOTH the SMBiosVersion and get_entry_size(), same
    // double-guard discipline FirmwareLanguageInformationEntry::get_flags()
    // established for its own 2.1+ field (R8/version-gating note in this
    // class's header).
    if (version_ < kVersion21) return 0;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEventLogEntry::get_supported_descriptor_count() const
{
    constexpr size_t offset = 0x15;
    constexpr size_t min_length = 0x16;
    if (version_ < kVersion21) return 0;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEventLogEntry::get_descriptor_length() const
{
    constexpr size_t offset = 0x16;
    constexpr size_t min_length = 0x17;

    // Spec's own note: "currently always 2" - but read it, never hardcode
    // 2 into the trailing-list offset arithmetic below (get_descriptor_
    // length() is the single source of truth for "y").
    if (version_ < kVersion21) return 0;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint32_t SystemEventLogEntry::get_log_type_descriptor_at(size_t index) const
{
    if (version_ < kVersion21) return 0;

    const uint8_t descriptor_length = get_descriptor_length();
    if (descriptor_length == 0) return 0;
    if (descriptor_length > sizeof(uint32_t)) return 0;

    const size_t offset = 0x17 + index * static_cast<size_t>(descriptor_length);
    const size_t min_length = offset + descriptor_length;

    // Independently guarded against get_entry_size() directly (not merely
    // against get_supported_descriptor_count()) - a malformed x/y pair can
    // never cause an out-of-bounds read (see class doc comment).
    if (get_entry_size() < min_length) return 0;

    uint32_t value = 0;
    for (uint8_t byte_index = 0; byte_index < descriptor_length; ++byte_index) {
        value |= static_cast<uint32_t>(read_u8(raw_data(), offset + byte_index)) << (8 * byte_index);
    }
    return value;
}

std::string SystemEventLogEntry::get_access_method_string() const
{
    const uint8_t access_method = get_access_method();

    auto it = access_method_names_.find(access_method);
    if (it != access_method_names_.end()) {
        return (*it).second;
    }

    if (access_method >= 0x05 && access_method <= 0x7F) {
        return "Reserved";
    }

    std::stringstream oem;
    oem << "OEM-specific (0x" << std::hex << static_cast<unsigned>(access_method) << std::dec << ")";
    return oem.str();
}
