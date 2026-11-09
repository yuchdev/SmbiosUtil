#include <libsmbios/tables/cooling_device_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::CoolingDevice at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::CoolingDevice, CoolingDeviceEntry)

namespace {

constexpr size_t kTemperatureProbeHandleOffset = 0x04;
constexpr size_t kDeviceTypeAndStatusOffset = 0x06;
constexpr size_t kCoolingUnitGroupOffset = 0x07;
constexpr size_t kOemDefinedOffset = 0x08;
constexpr size_t kNominalSpeedOffset = 0x0C;
constexpr size_t kDescriptionOffset = 0x0E;

constexpr size_t kTemperatureProbeHandleMinLength = 0x06;
constexpr size_t kDeviceTypeAndStatusMinLength = 0x07;
constexpr size_t kCoolingUnitGroupMinLength = 0x08;
constexpr size_t kOemDefinedMinLength = 0x0C;

/// See CoolingDeviceEntry::has_nominal_speed()'s own doc comment for why
/// this is 0x0E (the full 2-byte field), not the literal spec text's 0x0D.
constexpr size_t kNominalSpeedMinLength = 0x0E;

/// Description is a single-byte STRING index; the literal spec gate
/// ("0Fh or larger") needs no off-by-one adjustment.
constexpr size_t kDescriptionMinLength = 0x0F;

/// Device Type and Status bit positions/masks, DSP0134 3.9.0 Table 98
/// (see cooling_device_entry.h's class doc comment for the Status
/// cross-subsection confirmation note).
constexpr uint8_t kStatusShift = 5; // Bits 7:5
constexpr uint8_t kStatusMask = 0x07;
constexpr uint8_t kDeviceTypeShift = 0; // Bits 4:0
constexpr uint8_t kDeviceTypeMask = 0x1F;

/// Shared "is this WORD the 0x8000 unknown sentinel" check, implemented
/// once per this .cpp per this story's own Key constraints note.
bool is_word_value_unknown(uint16_t value)
{
    return value == CoolingDeviceEntry::NominalSpeedUnknown;
}

} // namespace

CoolingDeviceEntry::CoolingDeviceEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::CoolingDevice) {
        std::stringstream err;
        err << "Wrong entry type, expected Cooling Device, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.28 (Table 97/98).
    init_string_values();
}

void CoolingDeviceEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 98.
    status_map_[StatusOutOfSpec] = "Reserved";
    status_map_[StatusOther] = "Other";
    status_map_[StatusUnknown] = "Unknown";
    status_map_[StatusOk] = "OK";
    status_map_[StatusNonCritical] = "Non-critical";
    status_map_[StatusCritical] = "Critical";
    status_map_[StatusNonRecoverable] = "Non-recoverable";

    device_type_map_[DeviceTypeOutOfSpec] = "Reserved";
    device_type_map_[DeviceTypeOther] = "Other";
    device_type_map_[DeviceTypeUnknown] = "Unknown";
    device_type_map_[DeviceTypeFan] = "Fan";
    device_type_map_[DeviceTypeCentrifugalBlower] = "Centrifugal Blower";
    device_type_map_[DeviceTypeChipFan] = "Chip Fan";
    device_type_map_[DeviceTypeCabinetFan] = "Cabinet Fan";
    device_type_map_[DeviceTypePowerSupplyFan] = "Power Supply Fan";
    device_type_map_[DeviceTypeHeatPipe] = "Heat Pipe";
    device_type_map_[DeviceTypeIntegratedRefrigeration] = "Integrated Refrigeration";
    device_type_map_[DeviceTypeActiveCooling] = "Active Cooling";
    device_type_map_[DeviceTypePassiveCooling] = "Passive Cooling";
}

std::string CoolingDeviceEntry::get_type() const
{
    return "Cooling Device";
}

std::string CoolingDeviceEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Temperature Probe Handle: 0x" << std::hex << get_temperature_probe_handle() << std::dec << '\n';
    description << "Device Type: " << get_device_type_string() << '\n';
    description << "Status: " << get_status_string() << '\n';
    description << "Cooling Unit Group: " << static_cast<unsigned>(get_cooling_unit_group())
                 << (is_redundant() ? " (redundant)" : " (not redundant)") << '\n';
    if (has_nominal_speed()) {
        description << "Nominal Speed: "
                     << (is_nominal_speed_known() ? std::to_string(get_nominal_speed()) + " rpm" : "Unknown") << '\n';
    }
    if (has_description()) {
        description << "Description: " << get_description_string() << '\n';
    }

    return description.str();
}

uint16_t CoolingDeviceEntry::get_temperature_probe_handle() const
{
    if (get_entry_size() < kTemperatureProbeHandleMinLength) return 0xFFFF;
    return read_u16(raw_data(), kTemperatureProbeHandleOffset);
}

uint8_t CoolingDeviceEntry::get_device_type_and_status() const
{
    if (get_entry_size() < kDeviceTypeAndStatusMinLength) return 0;
    return read_u8(raw_data(), kDeviceTypeAndStatusOffset);
}

uint8_t CoolingDeviceEntry::get_cooling_unit_group() const
{
    if (get_entry_size() < kCoolingUnitGroupMinLength) return 0;
    return read_u8(raw_data(), kCoolingUnitGroupOffset);
}

uint32_t CoolingDeviceEntry::get_oem_defined() const
{
    if (get_entry_size() < kOemDefinedMinLength) return 0;
    return read_u32(raw_data(), kOemDefinedOffset);
}

uint16_t CoolingDeviceEntry::get_nominal_speed() const
{
    if (!has_nominal_speed()) return NominalSpeedUnknown;
    return read_u16(raw_data(), kNominalSpeedOffset);
}

std::string CoolingDeviceEntry::get_description_string() const
{
    if (!has_description()) return std::string();
    return dmi_string(read_u8(raw_data(), kDescriptionOffset));
}

CoolingDeviceEntry::Status CoolingDeviceEntry::get_status() const
{
    return static_cast<Status>((get_device_type_and_status() >> kStatusShift) & kStatusMask);
}

std::string CoolingDeviceEntry::get_status_string() const
{
    auto it = status_map_.find(get_status());
    if (it != status_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

CoolingDeviceEntry::DeviceType CoolingDeviceEntry::get_device_type() const
{
    return static_cast<DeviceType>((get_device_type_and_status() >> kDeviceTypeShift) & kDeviceTypeMask);
}

std::string CoolingDeviceEntry::get_device_type_string() const
{
    auto it = device_type_map_.find(get_device_type());
    if (it != device_type_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

bool CoolingDeviceEntry::is_redundant() const
{
    return get_cooling_unit_group() != 0;
}

bool CoolingDeviceEntry::has_nominal_speed() const
{
    return get_entry_size() >= kNominalSpeedMinLength;
}

bool CoolingDeviceEntry::is_nominal_speed_known() const
{
    return has_nominal_speed() && !is_word_value_unknown(get_nominal_speed());
}

bool CoolingDeviceEntry::has_description() const
{
    return get_entry_size() >= kDescriptionMinLength;
}
