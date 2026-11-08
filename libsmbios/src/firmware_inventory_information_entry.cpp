#include <smbios/tables/firmware_inventory_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::FirmwareInventoryInformation at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::FirmwareInventoryInformation, FirmwareInventoryInformationEntry)

namespace {

constexpr size_t kComponentNameOffset = 0x04;
constexpr size_t kVersionOffset = 0x05;
constexpr size_t kVersionFormatOffset = 0x06;
constexpr size_t kFirmwareIdOffset = 0x07;
constexpr size_t kFirmwareIdFormatOffset = 0x08;
constexpr size_t kReleaseDateOffset = 0x09;
constexpr size_t kManufacturerOffset = 0x0A;
constexpr size_t kLowestSupportedVersionOffset = 0x0B;
constexpr size_t kImageSizeOffset = 0x0C;
constexpr size_t kCharacteristicsOffset = 0x14;
constexpr size_t kStateOffset = 0x16;
constexpr size_t kAssociatedComponentCountOffset = 0x17;
constexpr size_t kAssociatedComponentHandlesOffset = 0x18;

} // namespace

FirmwareInventoryInformationEntry::FirmwareInventoryInformationEntry(const DMIHeader& header,
                                                                      const SMBiosVersion& /*version*/,
                                                                      const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::FirmwareInventoryInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Firmware Inventory Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.46/7.46.1-7.46.4
    // (Table 133-137). See this header's own leading comment block for the
    // confirmed Associated Component Handles offset.
    init_string_values();
}

void FirmwareInventoryInformationEntry::init_string_values()
{
    // DSP0134 3.9.0 Table 134 (7.46.1).
    version_format_map_[VersionFormatFreeForm] = "Free-form";
    version_format_map_[VersionFormatMajorMinor] = "Major.Minor";
    version_format_map_[VersionFormatHex32] = "32-bit hexadecimal";
    version_format_map_[VersionFormatHex64] = "64-bit hexadecimal";

    // DSP0134 3.9.0 Table 135 (7.46.2).
    firmware_id_format_map_[FirmwareIdFormatFreeForm] = "Free-form";
    firmware_id_format_map_[FirmwareIdFormatUefiGuid] = "UEFI ESRT FwClass GUID / FMP ImageTypeId";

    // DSP0134 3.9.0 Table 136 (7.46.3).
    characteristics_map_[Updatable] = "Updatable";
    characteristics_map_[WriteProtect] = "Write-Protect";

    // DSP0134 3.9.0 Table 137 (7.46.4).
    state_map_[StateOther] = "Other";
    state_map_[StateUnknown] = "Unknown";
    state_map_[StateDisabled] = "Disabled";
    state_map_[StateEnabled] = "Enabled";
    state_map_[StateAbsent] = "Absent";
    state_map_[StateStandbyOffline] = "StandbyOffline";
    state_map_[StateStandbySpare] = "StandbySpare";
    state_map_[StateUnavailableOffline] = "UnavailableOffline";
}

std::string FirmwareInventoryInformationEntry::get_type() const
{
    return "Firmware Inventory Information";
}

std::string FirmwareInventoryInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Component Name: " << get_component_name_string() << '\n';
    description << "Version: " << get_version_string() << " (" << get_version_format_string() << ")\n";
    description << "Firmware ID: " << get_firmware_id_string() << " (" << get_firmware_id_format_string() << ")\n";
    description << "Release Date: " << get_release_date_string() << '\n';
    description << "Manufacturer: " << get_manufacturer_string() << '\n';
    description << "Lowest Supported Version: " << get_lowest_supported_version_string() << '\n';

    description << "Image Size: ";
    if (is_image_size_known()) {
        description << get_image_size() << " bytes\n";
    } else {
        description << "Unknown\n";
    }

    description << "Characteristics:\n" << get_characteristics_string();
    description << "State: " << get_state_string() << '\n';

    const uint8_t count = get_associated_component_count();
    description << "Associated Component Count: " << static_cast<unsigned>(count) << '\n';
    for (size_t index = 0; index < count; ++index) {
        description << "\tHandle: 0x" << std::hex << get_associated_component_handle_at(index) << std::dec << '\n';
    }

    return description.str();
}

std::string FirmwareInventoryInformationEntry::get_component_name_string() const
{
    constexpr size_t min_length = kComponentNameOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kComponentNameOffset));
}

std::string FirmwareInventoryInformationEntry::get_version_string() const
{
    constexpr size_t min_length = kVersionOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kVersionOffset));
}

std::string FirmwareInventoryInformationEntry::get_firmware_id_string() const
{
    constexpr size_t min_length = kFirmwareIdOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kFirmwareIdOffset));
}

std::string FirmwareInventoryInformationEntry::get_release_date_string() const
{
    constexpr size_t min_length = kReleaseDateOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kReleaseDateOffset));
}

std::string FirmwareInventoryInformationEntry::get_manufacturer_string() const
{
    constexpr size_t min_length = kManufacturerOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kManufacturerOffset));
}

std::string FirmwareInventoryInformationEntry::get_lowest_supported_version_string() const
{
    constexpr size_t min_length = kLowestSupportedVersionOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kLowestSupportedVersionOffset));
}

uint8_t FirmwareInventoryInformationEntry::get_version_format() const
{
    constexpr size_t min_length = kVersionFormatOffset + 1;
    if (get_entry_size() < min_length) return VersionFormatFreeForm;
    return read_u8(raw_data(), kVersionFormatOffset);
}

std::string FirmwareInventoryInformationEntry::get_version_format_string() const
{
    const uint8_t value = get_version_format();
    const auto found = version_format_map_.find(value);
    if (found != version_format_map_.end()) return found->second;

    // Table 134's own two reserved ranges, confirmed against the full
    // table - rendered distinctly rather than a flat "Unknown", same
    // rationale as StringPropertyEntry::get_property_id_string().
    if (value >= 0x04 && value <= 0x7F) return "Reserved (Future DSP0134 Assignment)";
    return "Firmware Vendor/OEM-specific"; // 0x80-0xFF
}

uint8_t FirmwareInventoryInformationEntry::get_firmware_id_format() const
{
    constexpr size_t min_length = kFirmwareIdFormatOffset + 1;
    if (get_entry_size() < min_length) return FirmwareIdFormatFreeForm;
    return read_u8(raw_data(), kFirmwareIdFormatOffset);
}

std::string FirmwareInventoryInformationEntry::get_firmware_id_format_string() const
{
    const uint8_t value = get_firmware_id_format();
    const auto found = firmware_id_format_map_.find(value);
    if (found != firmware_id_format_map_.end()) return found->second;

    // Table 135's own two reserved ranges, confirmed against the full
    // table.
    if (value >= 0x02 && value <= 0x7F) return "Reserved (Future DSP0134 Assignment)";
    return "Firmware Vendor/OEM-specific"; // 0x80-0xFF
}

uint64_t FirmwareInventoryInformationEntry::get_image_size() const
{
    constexpr size_t min_length = kImageSizeOffset + 8;
    if (get_entry_size() < min_length) return kImageSizeUnknown;
    return read_u64(raw_data(), kImageSizeOffset);
}

bool FirmwareInventoryInformationEntry::is_image_size_known() const
{
    return get_image_size() != kImageSizeUnknown;
}

uint16_t FirmwareInventoryInformationEntry::get_characteristics() const
{
    constexpr size_t min_length = kCharacteristicsOffset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), kCharacteristicsOffset);
}

std::string FirmwareInventoryInformationEntry::get_characteristics_string() const
{
    return bitset_to_properties<uint16_t>(get_characteristics(), characteristics_map_);
}

bool FirmwareInventoryInformationEntry::is_updatable() const
{
    return (get_characteristics() & Updatable) != 0;
}

bool FirmwareInventoryInformationEntry::is_write_protected() const
{
    return (get_characteristics() & WriteProtect) != 0;
}

uint8_t FirmwareInventoryInformationEntry::get_state() const
{
    constexpr size_t min_length = kStateOffset + 1;
    if (get_entry_size() < min_length) return StateOutOfSpec;
    return read_u8(raw_data(), kStateOffset);
}

std::string FirmwareInventoryInformationEntry::get_state_string() const
{
    const auto found = state_map_.find(get_state());
    if (found == state_map_.end()) return "Reserved";
    return found->second;
}

uint8_t FirmwareInventoryInformationEntry::get_associated_component_count() const
{
    // DSP0134 3.9.0, Table 133: the explicit Number of Associated
    // Components field. Deliberately NOT derived from
    // get_entry_size()/Length - see this class's own header doc comment.
    constexpr size_t min_length = kAssociatedComponentCountOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kAssociatedComponentCountOffset);
}

uint16_t FirmwareInventoryInformationEntry::get_associated_component_handle_at(size_t index) const
{
    if (index >= get_associated_component_count()) return 0;

    const size_t offset = kAssociatedComponentHandlesOffset + 2 * index;
    const size_t min_length = offset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}
