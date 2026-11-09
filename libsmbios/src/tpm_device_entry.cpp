#include <libsmbios/tables/tpm_device_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::TpmDevice at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::TpmDevice, TpmDeviceEntry)

namespace {

constexpr size_t kVendorIdOffset = 0x04;
constexpr size_t kVendorIdLength = 4;
constexpr size_t kMajorSpecVersionOffset = 0x08;
constexpr size_t kMinorSpecVersionOffset = 0x09;
constexpr size_t kFirmwareVersion1Offset = 0x0A;
constexpr size_t kFirmwareVersion2Offset = 0x0E;
constexpr size_t kDescriptionOffset = 0x12;
constexpr size_t kCharacteristicsOffset = 0x13;
constexpr size_t kOemDefinedOffset = 0x1B;

} // namespace

TpmDeviceEntry::TpmDeviceEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::TpmDevice) {
        std::stringstream err;
        err << "Wrong entry type, expected TPM Device, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.44/7.44.1 (Table
    // 126/127).
    init_string_values();
}

void TpmDeviceEntry::init_string_values()
{
    // DSP0134 3.9.0 Table 127 (7.44.1). Bits 0:1 and 6:63 are explicitly
    // Reserved (no entry here); only bits 2:5 carry a decoded meaning.
    characteristics_map_[CharacteristicsNotSupported] = "TPM Device Characteristics are not supported";
    characteristics_map_[FamilyConfigurableViaFirmwareUpdate] = "Family configurable via firmware update";
    characteristics_map_[FamilyConfigurableViaPlatformSoftware] = "Family configurable via platform software support";
    characteristics_map_[FamilyConfigurableViaOemProprietaryMechanism] = "Family configurable via OEM proprietary mechanism";
}

std::string TpmDeviceEntry::get_type() const
{
    return "TPM Device";
}

std::string TpmDeviceEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Vendor ID: " << get_vendor_id_string() << '\n';
    description << "Spec Version: " << static_cast<unsigned>(get_major_spec_version())
                 << '.' << static_cast<unsigned>(get_minor_spec_version()) << '\n';
    description << "Firmware Version 1: " << get_firmware_version_1() << '\n';
    description << "Firmware Version 2: " << get_firmware_version_2() << '\n';
    description << "Description: " << get_description_string() << '\n';
    description << "Characteristics:\n" << get_characteristics_string();
    description << "OEM-defined: " << get_oem_defined() << '\n';

    return description.str();
}

std::string TpmDeviceEntry::get_vendor_id_string() const
{
    constexpr size_t min_length = kVendorIdOffset + kVendorIdLength;
    if (get_entry_size() < min_length) return "";

    // Raw ASCII bytes, read directly - deliberately NOT dmi_string(). See
    // this header's own leading comment block and this accessor's own doc
    // comment for why this field is independent of this entry's DMI string
    // table.
    char vendor_id[kVendorIdLength + 1] = { 0 };
    for (size_t i = 0; i < kVendorIdLength; ++i) {
        vendor_id[i] = static_cast<char>(read_u8(raw_data(), kVendorIdOffset + i));
    }

    // Trim exactly one trailing 0x00 byte (the 3-character "ABC" case, per
    // Table 126's own example: 41h 42h 43h 00h). A genuine 4-character
    // vendor ID with no null terminator ("ABCD" = 41h 42h 43h 44h) is
    // returned in full - std::string's own null-terminated construction
    // already stops at the first embedded '\0' either way, so no
    // additional trimming logic beyond relying on that constructor is
    // needed; a 4th byte of 0x00 simply becomes the string's own natural
    // terminator, while a non-zero 4th byte is included.
    return std::string(vendor_id);
}

uint8_t TpmDeviceEntry::get_major_spec_version() const
{
    constexpr size_t min_length = kMajorSpecVersionOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kMajorSpecVersionOffset);
}

uint8_t TpmDeviceEntry::get_minor_spec_version() const
{
    constexpr size_t min_length = kMinorSpecVersionOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kMinorSpecVersionOffset);
}

uint32_t TpmDeviceEntry::get_firmware_version_1() const
{
    constexpr size_t min_length = kFirmwareVersion1Offset + 4;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), kFirmwareVersion1Offset);
}

uint32_t TpmDeviceEntry::get_firmware_version_2() const
{
    constexpr size_t min_length = kFirmwareVersion2Offset + 4;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), kFirmwareVersion2Offset);
}

uint64_t TpmDeviceEntry::get_characteristics() const
{
    constexpr size_t min_length = kCharacteristicsOffset + 8;
    if (get_entry_size() < min_length) return 0;
    return read_u64(raw_data(), kCharacteristicsOffset);
}

uint32_t TpmDeviceEntry::get_oem_defined() const
{
    constexpr size_t min_length = kOemDefinedOffset + 4;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), kOemDefinedOffset);
}

std::string TpmDeviceEntry::get_description_string() const
{
    constexpr size_t min_length = kDescriptionOffset + 1;
    if (get_entry_size() < min_length) return AbstractSMBiosEntry::dmi_string(0);
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kDescriptionOffset));
}

std::string TpmDeviceEntry::get_characteristics_string() const
{
    return bitset_to_properties<uint64_t>(get_characteristics(), characteristics_map_);
}
