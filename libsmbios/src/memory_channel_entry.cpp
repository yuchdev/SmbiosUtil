#include <libsmbios/tables/memory_channel_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::MemoryChannel at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::MemoryChannel, MemoryChannelEntry)

namespace {

constexpr size_t kChannelTypeOffset = 0x04;
constexpr size_t kMaximumChannelLoadOffset = 0x05;
constexpr size_t kDeviceCountOffset = 0x06;
constexpr size_t kDeviceArrayOffset = 0x07;

constexpr size_t kChannelTypeMinLength = 0x05;
constexpr size_t kMaximumChannelLoadMinLength = 0x06;
constexpr size_t kDeviceCountMinLength = 0x07;

} // namespace

MemoryChannelEntry::MemoryChannelEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::MemoryChannel) {
        std::stringstream err;
        err << "Wrong entry type, expected Memory Channel, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.38/7.38.1 (Table 112/
    // 113).
    init_string_values();
}

void MemoryChannelEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 113.
    channel_type_map_[ChannelTypeOutOfSpec] = "Reserved";
    channel_type_map_[ChannelTypeOther] = "Other";
    channel_type_map_[ChannelTypeUnknown] = "Unknown";
    channel_type_map_[ChannelTypeRambus] = "Rambus";
    channel_type_map_[ChannelTypeSyncLink] = "SyncLink";
}

std::string MemoryChannelEntry::get_type() const
{
    return "Memory Channel";
}

std::string MemoryChannelEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Channel Type: " << get_channel_type_string() << '\n';
    description << "Maximum Channel Load: " << static_cast<unsigned>(get_maximum_channel_load()) << '\n';

    const uint8_t count = get_device_count();
    description << "Memory Device Count: " << static_cast<unsigned>(count) << '\n';
    for (size_t index = 0; index < count; ++index) {
        description << "\tLoad: " << static_cast<unsigned>(get_device_load_at(index))
                     << ", Handle: " << get_device_handle_at(index) << '\n';
    }

    return description.str();
}

MemoryChannelEntry::ChannelType MemoryChannelEntry::get_channel_type() const
{
    if (get_entry_size() < kChannelTypeMinLength) return ChannelTypeOutOfSpec;
    return static_cast<ChannelType>(read_u8(raw_data(), kChannelTypeOffset));
}

std::string MemoryChannelEntry::get_channel_type_string() const
{
    auto it = channel_type_map_.find(get_channel_type());
    if (it != channel_type_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

uint8_t MemoryChannelEntry::get_maximum_channel_load() const
{
    if (get_entry_size() < kMaximumChannelLoadMinLength) return 0;
    return read_u8(raw_data(), kMaximumChannelLoadOffset);
}

uint8_t MemoryChannelEntry::get_device_count() const
{
    // DSP0134 3.9.0, Table 112: the explicit Memory Device Count field.
    // Deliberately NOT derived from get_entry_size()/Length - see this
    // class's own header doc comment for why Length must not be used here,
    // the opposite convention from GroupAssociationsEntry.
    if (get_entry_size() < kDeviceCountMinLength) return 0;
    return read_u8(raw_data(), kDeviceCountOffset);
}

uint8_t MemoryChannelEntry::get_device_load_at(size_t index) const
{
    if (index >= get_device_count()) return 0;

    const size_t offset = kDeviceArrayOffset + 3 * index;
    const size_t min_length = offset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t MemoryChannelEntry::get_device_handle_at(size_t index) const
{
    if (index >= get_device_count()) return 0;

    const size_t offset = kDeviceArrayOffset + 1 + 3 * index;
    const size_t min_length = offset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}
