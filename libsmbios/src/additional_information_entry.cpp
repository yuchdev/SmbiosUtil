#include <libsmbios/tables/additional_information_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::AdditionalInformation at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::AdditionalInformation, AdditionalInformationEntry)

namespace {

constexpr size_t kEntryCountOffset = 0x04;
constexpr size_t kEntryCountMinLength = 0x05;
constexpr size_t kEntriesArrayOffset = 0x05;

/// Offsets within a single Additional Information Entry (relative to that
/// entry's own start), DSP0134 3.9.0 Table 119 (7.41.1).
constexpr size_t kEntryLengthOffset = 0x00;
constexpr size_t kReferencedHandleOffset = 0x01;
constexpr size_t kReferencedOffsetOffset = 0x03;
constexpr size_t kStringIndexOffset = 0x04;
constexpr size_t kValueOffset = 0x05;

/// Minimum Entry Length per spec (Table 119's own "a minimum of 6").
constexpr uint8_t kMinEntryLength = 6;

} // namespace

AdditionalInformationEntry::AdditionalInformationEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::AdditionalInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Additional Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.41/7.41.1 (Table
    // 118/119).
    cache_entry_offsets();
}

void AdditionalInformationEntry::cache_entry_offsets()
{
    const uint8_t declared_count = get_entry_count();
    size_t offset = kEntriesArrayOffset;

    for (uint8_t current = 0; current < declared_count; ++current) {
        // Entry Length byte (+0x00 of this entry) must itself be in bounds.
        if (get_entry_size() < offset + kEntryLengthOffset + 1) break;
        const uint8_t entry_length = read_u8(raw_data(), offset + kEntryLengthOffset);

        // A spec-conformant entry is at least kMinEntryLength bytes; a
        // shorter declared Entry Length cannot be walked safely (the next
        // entry's start would be ambiguous/overlapping), so stop here
        // rather than guess.
        if (entry_length < kMinEntryLength) break;

        // The entire entry (Entry Length bytes, starting at offset) must
        // fit within the structure.
        if (get_entry_size() < offset + entry_length) break;

        entry_offsets_.push_back(offset);
        offset += entry_length;
    }
}

std::string AdditionalInformationEntry::get_type() const
{
    return "Additional Information";
}

std::string AdditionalInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Number of Additional Information entries: " << static_cast<unsigned>(get_entry_count()) << '\n';

    const size_t count = get_entry_offsets_count();
    for (size_t index = 0; index < count; ++index) {
        description << "\tEntry Length: " << static_cast<unsigned>(get_entry_length_at(index)) << '\n';
        description << "\tReferenced Handle: 0x" << std::hex << get_referenced_handle_at(index) << std::dec << '\n';
        description << "\tReferenced Offset: 0x" << std::hex << static_cast<unsigned>(get_referenced_offset_at(index)) << std::dec << '\n';
        description << "\tString: " << get_string_at(index) << '\n';

        const std::vector<uint8_t> value = get_value_bytes_at(index);
        description << "\tValue (" << value.size() << " bytes): ";
        for (uint8_t b : value) {
            description << std::hex << static_cast<unsigned>(b) << ' ' << std::dec;
        }
        description << '\n';
    }

    return description.str();
}

uint8_t AdditionalInformationEntry::get_entry_count() const
{
    if (get_entry_size() < kEntryCountMinLength) return 0;
    return read_u8(raw_data(), kEntryCountOffset);
}

size_t AdditionalInformationEntry::get_entry_offsets_count() const
{
    return entry_offsets_.size();
}

uint8_t AdditionalInformationEntry::get_entry_length_at(size_t index) const
{
    if (index >= entry_offsets_.size()) return 0;
    return read_u8(raw_data(), entry_offsets_[index] + kEntryLengthOffset);
}

uint16_t AdditionalInformationEntry::get_referenced_handle_at(size_t index) const
{
    if (index >= entry_offsets_.size()) return 0;
    return read_u16(raw_data(), entry_offsets_[index] + kReferencedHandleOffset);
}

uint8_t AdditionalInformationEntry::get_referenced_offset_at(size_t index) const
{
    if (index >= entry_offsets_.size()) return 0;
    return read_u8(raw_data(), entry_offsets_[index] + kReferencedOffsetOffset);
}

uint8_t AdditionalInformationEntry::get_string_index_at(size_t index) const
{
    if (index >= entry_offsets_.size()) return 0;
    return read_u8(raw_data(), entry_offsets_[index] + kStringIndexOffset);
}

std::string AdditionalInformationEntry::get_string_at(size_t index) const
{
    if (index >= entry_offsets_.size()) return dmi_string(0);
    return dmi_string(get_string_index_at(index));
}

std::vector<uint8_t> AdditionalInformationEntry::get_value_bytes_at(size_t index) const
{
    std::vector<uint8_t> value;
    if (index >= entry_offsets_.size()) return value;

    // cache_entry_offsets() already confirmed this entry's full Entry
    // Length fits within get_entry_size(), so the Value field (Entry
    // Length - 5 bytes starting at +0x05) is guaranteed in-bounds here -
    // no further guard needed beyond the index check above.
    const size_t entry_start = entry_offsets_[index];
    const uint8_t entry_length = read_u8(raw_data(), entry_start + kEntryLengthOffset);
    const size_t value_length = static_cast<size_t>(entry_length) - kValueOffset;

    value.reserve(value_length);
    for (size_t i = 0; i < value_length; ++i) {
        value.push_back(read_u8(raw_data(), entry_start + kValueOffset + i));
    }
    return value;
}
