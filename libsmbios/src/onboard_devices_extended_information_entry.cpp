#include <libsmbios/tables/onboard_devices_extended_information_entry.h>
#include <libsmbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <libsmbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>

using namespace smbios;

// Self-register against SMBios::OnboardDevicesExtendedInformation at
// static-init time; see REGISTER_SMBIOS_ENTRY's doc comment
// (smbios_entry_factory.h) for why this lives here rather than in the
// header.
REGISTER_SMBIOS_ENTRY(SMBios::OnboardDevicesExtendedInformation, OnboardDevicesExtendedInformationEntry)

namespace {

constexpr size_t kReferenceDesignationOffset = 0x04;
constexpr size_t kDeviceTypeOffset = 0x05;
constexpr size_t kDeviceTypeInstanceOffset = 0x06;
constexpr size_t kSegmentGroupNumberOffset = 0x07;
constexpr size_t kBusNumberOffset = 0x09;
constexpr size_t kDeviceFunctionNumberOffset = 0x0A;

/// Device Type byte (0x05) masks, DSP0134 3.9.0 Table 120: bit 7 is the
/// enabled/disabled flag, bits 6:0 are the device type enum. Two
/// independent masks - see this entry's own header doc comment for why a
/// single combined accessor is deliberately not provided.
constexpr uint8_t kDeviceEnabledMask = 0x80;
constexpr uint8_t kDeviceTypeMask = 0x7F;

/// Minimum structure length (Table 120's own "0Bh").
constexpr size_t kMinLength = 0x0B;

} // namespace

OnboardDevicesExtendedInformationEntry::OnboardDevicesExtendedInformationEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::OnboardDevicesExtendedInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Onboard Devices Extended Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.42/7.42.1-7.42.4
    // (Table 120).
    init_string_values();
}

void OnboardDevicesExtendedInformationEntry::init_string_values()
{
    // DSP0134 3.9.0 Table 121 (7.42.2).
    device_type_map_[DeviceTypeOther] = "Other";
    device_type_map_[DeviceTypeUnknown] = "Unknown";
    device_type_map_[DeviceTypeVideo] = "Video";
    device_type_map_[DeviceTypeScsiController] = "SCSI Controller";
    device_type_map_[DeviceTypeEthernet] = "Ethernet";
    device_type_map_[DeviceTypeTokenRing] = "Token Ring";
    device_type_map_[DeviceTypeSound] = "Sound";
    device_type_map_[DeviceTypePataController] = "PATA Controller";
    device_type_map_[DeviceTypeSataController] = "SATA Controller";
    device_type_map_[DeviceTypeSasController] = "SAS Controller";
    device_type_map_[DeviceTypeWirelessLan] = "Wireless LAN";
    device_type_map_[DeviceTypeBluetooth] = "Bluetooth";
    device_type_map_[DeviceTypeWwan] = "WWAN";
    device_type_map_[DeviceTypeEmmc] = "eMMC (embedded Multi-Media Controller)";
    device_type_map_[DeviceTypeNvmeController] = "NVMe Controller";
    device_type_map_[DeviceTypeUfsController] = "UFS Controller";
}

std::string OnboardDevicesExtendedInformationEntry::get_type() const
{
    return "Onboard Devices Extended Information";
}

std::string OnboardDevicesExtendedInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Reference Designation: " << get_reference_designation_string() << '\n';
    description << "Status: " << (is_device_enabled() ? "Enabled" : "Disabled") << '\n';
    description << "Type: " << get_device_type_string() << '\n';
    description << "Type Instance: " << static_cast<unsigned>(get_device_type_instance()) << '\n';
    description << "Bus Address: " << get_bus_address_string() << '\n';

    return description.str();
}

uint8_t OnboardDevicesExtendedInformationEntry::get_reference_designation_index() const
{
    constexpr size_t min_length = kReferenceDesignationOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kReferenceDesignationOffset);
}

uint8_t OnboardDevicesExtendedInformationEntry::get_device_type_byte() const
{
    constexpr size_t min_length = kDeviceTypeOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kDeviceTypeOffset);
}

uint8_t OnboardDevicesExtendedInformationEntry::get_device_type_instance() const
{
    constexpr size_t min_length = kDeviceTypeInstanceOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kDeviceTypeInstanceOffset);
}

uint16_t OnboardDevicesExtendedInformationEntry::get_segment_group_number() const
{
    constexpr size_t min_length = kSegmentGroupNumberOffset + 2;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), kSegmentGroupNumberOffset);
}

uint8_t OnboardDevicesExtendedInformationEntry::get_bus_number() const
{
    constexpr size_t min_length = kBusNumberOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kBusNumberOffset);
}

uint8_t OnboardDevicesExtendedInformationEntry::get_device_function_number() const
{
    constexpr size_t min_length = kDeviceFunctionNumberOffset + 1;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), kDeviceFunctionNumberOffset);
}

uint8_t OnboardDevicesExtendedInformationEntry::get_device_number() const
{
    return static_cast<uint8_t>(get_device_function_number() >> 3);
}

uint8_t OnboardDevicesExtendedInformationEntry::get_function_number() const
{
    return static_cast<uint8_t>(get_device_function_number() & 0x07);
}

bool OnboardDevicesExtendedInformationEntry::is_device_enabled() const
{
    // Independent mask - bit 7 only, never combined with the device-type
    // mask below. See this class's own header doc comment for why this
    // split is a hard constraint, not a style choice.
    return (get_device_type_byte() & kDeviceEnabledMask) != 0;
}

OnboardDevicesExtendedInformationEntry::DeviceType OnboardDevicesExtendedInformationEntry::get_device_type() const
{
    // Independent mask - bits 6:0 only, never combined with the
    // enabled/disabled mask above. See this class's own header doc comment
    // for why this split is a hard constraint, not a style choice.
    return static_cast<DeviceType>(get_device_type_byte() & kDeviceTypeMask);
}

std::string OnboardDevicesExtendedInformationEntry::get_device_type_string() const
{
    auto it = device_type_map_.find(static_cast<uint8_t>(get_device_type()));
    if (it != device_type_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string OnboardDevicesExtendedInformationEntry::get_reference_designation_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_reference_designation_index());
}

std::string OnboardDevicesExtendedInformationEntry::get_bus_address_string() const
{
    if (get_entry_size() < kMinLength) return "";

    const uint16_t segment = get_segment_group_number();
    const uint8_t bus = get_bus_number();
    const uint8_t device_function = get_device_function_number();

    // See this header's own leading comment block: 7.42.4 confirms the
    // same all-FF/all-FFFF "not applicable" sentinel convention as
    // SystemSlotsEntry's own 7.10.8.
    if (segment == 0xFFFF && bus == 0xFF && device_function == 0xFF) {
        return "";
    }

    std::stringstream address;
    address << std::hex << std::setfill('0')
            << std::setw(4) << segment << ':'
            << std::setw(2) << static_cast<unsigned>(bus) << ':'
            << std::setw(2) << static_cast<unsigned>(get_device_number()) << '.'
            << std::dec << static_cast<unsigned>(get_function_number());
    return address.str();
}
