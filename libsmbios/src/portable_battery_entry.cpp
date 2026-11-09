#include <libsmbios/tables/portable_battery_entry.h>
#include <libsmbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <libsmbios/smbios_entry_factory.h>

#include <sstream>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::PortableBattery at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::PortableBattery, PortableBatteryEntry)

namespace {

constexpr size_t kLocationIndexOffset = 0x04;
constexpr size_t kManufacturerIndexOffset = 0x05;
constexpr size_t kManufactureDateIndexOffset = 0x06;
constexpr size_t kSerialNumberIndexOffset = 0x07;
constexpr size_t kDeviceNameIndexOffset = 0x08;
constexpr size_t kDeviceChemistryOffset = 0x09;
constexpr size_t kDesignCapacityOffset = 0x0A;
constexpr size_t kDesignVoltageOffset = 0x0C;
constexpr size_t kSbdsVersionIndexOffset = 0x0E;
constexpr size_t kMaximumErrorOffset = 0x0F;
constexpr size_t kDesignCapacityMultiplierOffset = 0x15;

constexpr size_t kLocationIndexMinLength = 0x05;
constexpr size_t kManufacturerIndexMinLength = 0x06;
constexpr size_t kManufactureDateIndexMinLength = 0x07;
constexpr size_t kSerialNumberIndexMinLength = 0x08;
constexpr size_t kDeviceNameIndexMinLength = 0x09;
constexpr size_t kDeviceChemistryMinLength = 0x0A;
constexpr size_t kDesignCapacityMinLength = 0x0C;
constexpr size_t kDesignVoltageMinLength = 0x0E;
constexpr size_t kSbdsVersionIndexMinLength = 0x0F;
constexpr size_t kMaximumErrorMinLength = 0x10;
constexpr size_t kDesignCapacityMultiplierMinLength = 0x16;

/// Spec's own documented default - see the class doc comment's Design
/// Capacity Multiplier resolution note.
constexpr uint8_t kDefaultDesignCapacityMultiplier = 1;

} // namespace

PortableBatteryEntry::PortableBatteryEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::PortableBattery) {
        std::stringstream err;
        err << "Wrong entry type, expected Portable Battery, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.23/7.23.1 (Table 90/
    // Table 91).
    init_string_values();
}

void PortableBatteryEntry::init_string_values()
{
    // DSP0134 3.9.0, 7.23.1 Table 91.
    device_chemistry_map_[Other] = "Other";
    device_chemistry_map_[Unknown] = "Unknown";
    device_chemistry_map_[LeadAcid] = "Lead Acid";
    device_chemistry_map_[NickelCadmium] = "Nickel Cadmium";
    device_chemistry_map_[NickelMetalHydride] = "Nickel metal hydride";
    device_chemistry_map_[LithiumIon] = "Lithium-ion";
    device_chemistry_map_[ZincAir] = "Zinc air";
    device_chemistry_map_[LithiumPolymer] = "Lithium Polymer";
}

std::string PortableBatteryEntry::get_type() const
{
    return "Portable Battery";
}

std::string PortableBatteryEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Location: " << get_location_string() << '\n';
    description << "Manufacturer: " << get_manufacturer_string() << '\n';
    description << "Manufacture Date: "
                 << (uses_sbds_manufacture_date() ? "See SBDS Manufacture Date" : get_manufacture_date_string()) << '\n';
    description << "Serial Number: "
                 << (uses_sbds_serial_number() ? "See SBDS Serial Number" : get_serial_number_string()) << '\n';
    description << "Device Name: " << get_device_name_string() << '\n';
    description << "Device Chemistry: " << get_device_chemistry_string() << '\n';
    description << "Design Capacity: "
                 << (is_design_capacity_known() ? std::to_string(get_design_capacity()) + " mWh" : "Unknown") << '\n';
    description << "Design Voltage: "
                 << (is_design_voltage_known() ? std::to_string(get_design_voltage()) + " mV" : "Unknown") << '\n';
    description << "SBDS Version Number: " << get_sbds_version_string() << '\n';
    description << "Maximum Error in Battery Data: "
                 << (is_maximum_error_known() ? std::to_string(static_cast<unsigned>(get_maximum_error_percent())) + "%" : "Unknown") << '\n';

    return description.str();
}

uint8_t PortableBatteryEntry::get_device_chemistry() const
{
    if (get_entry_size() < kDeviceChemistryMinLength) return ChemistryOutOfSpec;

    const uint8_t chemistry = read_u8(raw_data(), kDeviceChemistryOffset);
    auto it = device_chemistry_map_.find(chemistry);
    if (it != device_chemistry_map_.end()) {
        return (*it).first;
    }
    return ChemistryOutOfSpec;
}

uint16_t PortableBatteryEntry::raw_design_capacity() const
{
    if (get_entry_size() < kDesignCapacityMinLength) return 0;
    return read_u16(raw_data(), kDesignCapacityOffset);
}

uint8_t PortableBatteryEntry::design_capacity_multiplier() const
{
    if (get_entry_size() < kDesignCapacityMultiplierMinLength) return kDefaultDesignCapacityMultiplier;

    const uint8_t multiplier = read_u8(raw_data(), kDesignCapacityMultiplierOffset);
    // Spec's own documented default is 1; defensively treat a literal 0 the
    // same way rather than silently zeroing an otherwise-known capacity.
    return (multiplier == 0) ? kDefaultDesignCapacityMultiplier : multiplier;
}

uint32_t PortableBatteryEntry::get_design_capacity() const
{
    const uint16_t raw_capacity = raw_design_capacity();
    if (raw_capacity == 0) return 0;
    return static_cast<uint32_t>(raw_capacity) * static_cast<uint32_t>(design_capacity_multiplier());
}

uint16_t PortableBatteryEntry::get_design_voltage() const
{
    if (get_entry_size() < kDesignVoltageMinLength) return 0;
    return read_u16(raw_data(), kDesignVoltageOffset);
}

uint8_t PortableBatteryEntry::get_maximum_error_percent() const
{
    if (get_entry_size() < kMaximumErrorMinLength) return MaximumErrorUnknown;
    return read_u8(raw_data(), kMaximumErrorOffset);
}

bool PortableBatteryEntry::is_design_capacity_known() const
{
    return raw_design_capacity() != 0;
}

bool PortableBatteryEntry::is_design_voltage_known() const
{
    return get_design_voltage() != 0;
}

bool PortableBatteryEntry::is_maximum_error_known() const
{
    return get_maximum_error_percent() != MaximumErrorUnknown;
}

bool PortableBatteryEntry::uses_sbds_manufacture_date() const
{
    if (get_entry_size() < kManufactureDateIndexMinLength) return true;
    return read_u8(raw_data(), kManufactureDateIndexOffset) == 0;
}

bool PortableBatteryEntry::uses_sbds_serial_number() const
{
    if (get_entry_size() < kSerialNumberIndexMinLength) return true;
    return read_u8(raw_data(), kSerialNumberIndexOffset) == 0;
}

std::string PortableBatteryEntry::get_location_string() const
{
    if (get_entry_size() < kLocationIndexMinLength) return std::string();
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kLocationIndexOffset));
}

std::string PortableBatteryEntry::get_manufacturer_string() const
{
    if (get_entry_size() < kManufacturerIndexMinLength) return std::string();
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kManufacturerIndexOffset));
}

std::string PortableBatteryEntry::get_manufacture_date_string() const
{
    if (uses_sbds_manufacture_date()) return std::string();
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kManufactureDateIndexOffset));
}

std::string PortableBatteryEntry::get_serial_number_string() const
{
    if (uses_sbds_serial_number()) return std::string();
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kSerialNumberIndexOffset));
}

std::string PortableBatteryEntry::get_device_name_string() const
{
    if (get_entry_size() < kDeviceNameIndexMinLength) return std::string();
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kDeviceNameIndexOffset));
}

std::string PortableBatteryEntry::get_device_chemistry_string() const
{
    assert(!device_chemistry_map_.empty());
    auto it = device_chemistry_map_.find(get_device_chemistry());
    if (it != device_chemistry_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string PortableBatteryEntry::get_sbds_version_string() const
{
    if (get_entry_size() < kSbdsVersionIndexMinLength) return std::string();
    return AbstractSMBiosEntry::dmi_string(read_u8(raw_data(), kSbdsVersionIndexOffset));
}
