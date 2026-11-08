#include <smbios/tables/system_power_supply_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::SystemPowerSupply at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemPowerSupply, SystemPowerSupplyEntry)

namespace {

constexpr size_t kPowerUnitGroupOffset = 0x04;
constexpr size_t kLocationOffset = 0x05;
constexpr size_t kDeviceNameOffset = 0x06;
constexpr size_t kManufacturerOffset = 0x07;
constexpr size_t kSerialNumberOffset = 0x08;
constexpr size_t kAssetTagOffset = 0x09;
constexpr size_t kModelPartNumberOffset = 0x0A;
constexpr size_t kRevisionLevelOffset = 0x0B;
constexpr size_t kMaxPowerCapacityOffset = 0x0C;
constexpr size_t kCharacteristicsOffset = 0x0E;
constexpr size_t kInputVoltageProbeHandleOffset = 0x10;
constexpr size_t kCoolingDeviceHandleOffset = 0x12;
constexpr size_t kInputCurrentProbeHandleOffset = 0x14;

constexpr size_t kPowerUnitGroupMinLength = 0x05;
constexpr size_t kLocationMinLength = 0x06;
constexpr size_t kDeviceNameMinLength = 0x07;
constexpr size_t kManufacturerMinLength = 0x08;
constexpr size_t kSerialNumberMinLength = 0x09;
constexpr size_t kAssetTagMinLength = 0x0A;
constexpr size_t kModelPartNumberMinLength = 0x0B;
constexpr size_t kRevisionLevelMinLength = 0x0C;
constexpr size_t kMaxPowerCapacityMinLength = 0x0E;
constexpr size_t kCharacteristicsMinLength = 0x10;
constexpr size_t kInputVoltageProbeHandleMinLength = 0x12;
constexpr size_t kCoolingDeviceHandleMinLength = 0x14;
constexpr size_t kInputCurrentProbeHandleMinLength = 0x16;

/// Power Supply Characteristics bit positions/masks, DSP0134 3.9.0 Table
/// 117 (see system_power_supply_entry.h's leading comment block for the
/// confirmed layout citation).
constexpr uint16_t kPowerSupplyTypeShift = 10; // Bits 13:10
constexpr uint16_t kPowerSupplyTypeMask = 0x0F;
constexpr uint16_t kPowerSupplyStatusShift = 7; // Bits 9:7
constexpr uint16_t kPowerSupplyStatusMask = 0x07;
constexpr uint16_t kVoltageSwitchingShift = 3; // Bits 6:3
constexpr uint16_t kVoltageSwitchingMask = 0x0F;
constexpr uint16_t kUnpluggedBit = 0x0004; // Bit 2
constexpr uint16_t kPresentBit = 0x0002;   // Bit 1
constexpr uint16_t kHotReplaceableBit = 0x0001; // Bit 0

} // namespace

SystemPowerSupplyEntry::SystemPowerSupplyEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::SystemPowerSupply) {
        std::stringstream err;
        err << "Wrong entry type, expected System Power Supply, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.40/7.40.1 (Table
    // 116/117).
    init_string_values();
}

void SystemPowerSupplyEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 117.
    power_supply_type_map_[PowerSupplyTypeOutOfSpec] = "Reserved";
    power_supply_type_map_[PowerSupplyTypeOther] = "Other";
    power_supply_type_map_[PowerSupplyTypeUnknown] = "Unknown";
    power_supply_type_map_[PowerSupplyTypeLinear] = "Linear";
    power_supply_type_map_[PowerSupplyTypeSwitching] = "Switching";
    power_supply_type_map_[PowerSupplyTypeBattery] = "Battery";
    power_supply_type_map_[PowerSupplyTypeUps] = "UPS";
    power_supply_type_map_[PowerSupplyTypeConverter] = "Converter";
    power_supply_type_map_[PowerSupplyTypeRegulator] = "Regulator";

    power_supply_status_map_[PowerSupplyStatusOutOfSpec] = "Reserved";
    power_supply_status_map_[PowerSupplyStatusOther] = "Other";
    power_supply_status_map_[PowerSupplyStatusUnknown] = "Unknown";
    power_supply_status_map_[PowerSupplyStatusOk] = "OK";
    power_supply_status_map_[PowerSupplyStatusNonCritical] = "Non-critical";
    power_supply_status_map_[PowerSupplyStatusCritical] = "Critical";

    voltage_switching_map_[VoltageSwitchingOutOfSpec] = "Reserved";
    voltage_switching_map_[VoltageSwitchingOther] = "Other";
    voltage_switching_map_[VoltageSwitchingUnknown] = "Unknown";
    voltage_switching_map_[VoltageSwitchingManual] = "Manual";
    voltage_switching_map_[VoltageSwitchingAutoSwitch] = "Auto-switch";
    voltage_switching_map_[VoltageSwitchingWideRange] = "Wide range";
    voltage_switching_map_[VoltageSwitchingNotApplicable] = "Not applicable";
}

std::string SystemPowerSupplyEntry::get_type() const
{
    return "System Power Supply";
}

std::string SystemPowerSupplyEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Location: " << get_location_string() << '\n';
    description << "Device Name: " << get_device_name_string() << '\n';
    description << "Manufacturer: " << get_manufacturer_string() << '\n';
    description << "Serial Number: " << get_serial_number_string() << '\n';
    description << "Asset Tag: " << get_asset_tag_string() << '\n';
    description << "Model Part Number: " << get_model_part_number_string() << '\n';
    description << "Revision Level: " << get_revision_level_string() << '\n';
    description << "Power Unit Group: " << static_cast<unsigned>(get_power_unit_group())
                 << (is_redundant() ? " (redundant)" : " (not redundant)") << '\n';
    description << "Max Power Capacity: "
                 << (is_max_power_capacity_known() ? std::to_string(get_max_power_capacity()) + " W" : "Unknown") << '\n';
    description << "Power Supply Type: " << get_power_supply_type_string() << '\n';
    description << "Status: " << get_power_supply_status_string() << '\n';
    description << "Input Voltage Range Switching: " << get_voltage_switching_string() << '\n';
    description << "Unplugged: " << (is_unplugged() ? "yes" : "no") << '\n';
    description << "Present: " << (is_present() ? "yes" : "no") << '\n';
    description << "Hot-replaceable: " << (is_hot_replaceable() ? "yes" : "no") << '\n';
    if (has_input_voltage_probe()) {
        description << "Input Voltage Probe Handle: 0x" << std::hex << get_input_voltage_probe_handle() << std::dec << '\n';
    }
    if (has_cooling_device()) {
        description << "Cooling Device Handle: 0x" << std::hex << get_cooling_device_handle() << std::dec << '\n';
    }
    if (has_input_current_probe()) {
        description << "Input Current Probe Handle: 0x" << std::hex << get_input_current_probe_handle() << std::dec << '\n';
    }

    return description.str();
}

uint8_t SystemPowerSupplyEntry::get_power_unit_group() const
{
    if (get_entry_size() < kPowerUnitGroupMinLength) return 0;
    return read_u8(raw_data(), kPowerUnitGroupOffset);
}

uint16_t SystemPowerSupplyEntry::get_max_power_capacity() const
{
    if (get_entry_size() < kMaxPowerCapacityMinLength) return MaxPowerCapacityUnknown;
    return read_u16(raw_data(), kMaxPowerCapacityOffset);
}

uint16_t SystemPowerSupplyEntry::get_characteristics() const
{
    if (get_entry_size() < kCharacteristicsMinLength) return 0;
    return read_u16(raw_data(), kCharacteristicsOffset);
}

uint16_t SystemPowerSupplyEntry::get_input_voltage_probe_handle() const
{
    if (get_entry_size() < kInputVoltageProbeHandleMinLength) return HandleAbsent;
    return read_u16(raw_data(), kInputVoltageProbeHandleOffset);
}

uint16_t SystemPowerSupplyEntry::get_cooling_device_handle() const
{
    if (get_entry_size() < kCoolingDeviceHandleMinLength) return HandleAbsent;
    return read_u16(raw_data(), kCoolingDeviceHandleOffset);
}

uint16_t SystemPowerSupplyEntry::get_input_current_probe_handle() const
{
    if (get_entry_size() < kInputCurrentProbeHandleMinLength) return HandleAbsent;
    return read_u16(raw_data(), kInputCurrentProbeHandleOffset);
}

std::string SystemPowerSupplyEntry::get_location_string() const
{
    if (get_entry_size() < kLocationMinLength) return dmi_string(0);
    return dmi_string(read_u8(raw_data(), kLocationOffset));
}

std::string SystemPowerSupplyEntry::get_device_name_string() const
{
    if (get_entry_size() < kDeviceNameMinLength) return dmi_string(0);
    return dmi_string(read_u8(raw_data(), kDeviceNameOffset));
}

std::string SystemPowerSupplyEntry::get_manufacturer_string() const
{
    if (get_entry_size() < kManufacturerMinLength) return dmi_string(0);
    return dmi_string(read_u8(raw_data(), kManufacturerOffset));
}

std::string SystemPowerSupplyEntry::get_serial_number_string() const
{
    if (get_entry_size() < kSerialNumberMinLength) return dmi_string(0);
    return dmi_string(read_u8(raw_data(), kSerialNumberOffset));
}

std::string SystemPowerSupplyEntry::get_asset_tag_string() const
{
    if (get_entry_size() < kAssetTagMinLength) return dmi_string(0);
    return dmi_string(read_u8(raw_data(), kAssetTagOffset));
}

std::string SystemPowerSupplyEntry::get_model_part_number_string() const
{
    if (get_entry_size() < kModelPartNumberMinLength) return dmi_string(0);
    return dmi_string(read_u8(raw_data(), kModelPartNumberOffset));
}

std::string SystemPowerSupplyEntry::get_revision_level_string() const
{
    if (get_entry_size() < kRevisionLevelMinLength) return dmi_string(0);
    return dmi_string(read_u8(raw_data(), kRevisionLevelOffset));
}

SystemPowerSupplyEntry::PowerSupplyType SystemPowerSupplyEntry::get_power_supply_type() const
{
    return static_cast<PowerSupplyType>((get_characteristics() >> kPowerSupplyTypeShift) & kPowerSupplyTypeMask);
}

std::string SystemPowerSupplyEntry::get_power_supply_type_string() const
{
    auto it = power_supply_type_map_.find(get_power_supply_type());
    if (it != power_supply_type_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

SystemPowerSupplyEntry::PowerSupplyStatus SystemPowerSupplyEntry::get_power_supply_status() const
{
    return static_cast<PowerSupplyStatus>((get_characteristics() >> kPowerSupplyStatusShift) & kPowerSupplyStatusMask);
}

std::string SystemPowerSupplyEntry::get_power_supply_status_string() const
{
    auto it = power_supply_status_map_.find(get_power_supply_status());
    if (it != power_supply_status_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

SystemPowerSupplyEntry::VoltageSwitching SystemPowerSupplyEntry::get_voltage_switching() const
{
    return static_cast<VoltageSwitching>((get_characteristics() >> kVoltageSwitchingShift) & kVoltageSwitchingMask);
}

std::string SystemPowerSupplyEntry::get_voltage_switching_string() const
{
    auto it = voltage_switching_map_.find(get_voltage_switching());
    if (it != voltage_switching_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

bool SystemPowerSupplyEntry::is_unplugged() const
{
    return (get_characteristics() & kUnpluggedBit) != 0;
}

bool SystemPowerSupplyEntry::is_present() const
{
    return (get_characteristics() & kPresentBit) != 0;
}

bool SystemPowerSupplyEntry::is_hot_replaceable() const
{
    return (get_characteristics() & kHotReplaceableBit) != 0;
}

bool SystemPowerSupplyEntry::is_redundant() const
{
    return get_power_unit_group() != 0;
}

bool SystemPowerSupplyEntry::is_max_power_capacity_known() const
{
    return get_max_power_capacity() != MaxPowerCapacityUnknown;
}

bool SystemPowerSupplyEntry::has_input_voltage_probe() const
{
    return get_entry_size() >= kInputVoltageProbeHandleMinLength && get_input_voltage_probe_handle() != HandleAbsent;
}

bool SystemPowerSupplyEntry::has_cooling_device() const
{
    return get_entry_size() >= kCoolingDeviceHandleMinLength && get_cooling_device_handle() != HandleAbsent;
}

bool SystemPowerSupplyEntry::has_input_current_probe() const
{
    return get_entry_size() >= kInputCurrentProbeHandleMinLength && get_input_current_probe_handle() != HandleAbsent;
}
