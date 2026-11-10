#include <libsmbios/tables/electrical_current_probe_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::ElectricalCurrentProbe at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::ElectricalCurrentProbe, ElectricalCurrentProbeEntry)

namespace {

constexpr size_t kDescriptionOffset = 0x04;
constexpr size_t kLocationAndStatusOffset = 0x05;
constexpr size_t kMaximumValueOffset = 0x06;
constexpr size_t kMinimumValueOffset = 0x08;
constexpr size_t kResolutionOffset = 0x0A;
constexpr size_t kToleranceOffset = 0x0C;
constexpr size_t kAccuracyOffset = 0x0E;
constexpr size_t kOemDefinedOffset = 0x10;
constexpr size_t kNominalValueOffset = 0x14;

constexpr size_t kDescriptionMinLength = 0x05;
constexpr size_t kLocationAndStatusMinLength = 0x06;
constexpr size_t kMaximumValueMinLength = 0x08;
constexpr size_t kMinimumValueMinLength = 0x0A;
constexpr size_t kResolutionMinLength = 0x0C;
constexpr size_t kToleranceMinLength = 0x0E;
constexpr size_t kAccuracyMinLength = 0x10;
constexpr size_t kOemDefinedMinLength = 0x14;

/// See ElectricalCurrentProbeEntry::has_nominal_value()'s own doc comment
/// for why this is 0x16 (the full 2-byte field), not the literal spec
/// text's 0x15.
constexpr size_t kNominalValueMinLength = 0x16;

/// Location and Status bit positions/masks, DSP0134 3.9.0 Table 102 (see
/// electrical_current_probe_entry.hpp's class doc comment for the
/// cross-subsection confirmation note).
constexpr uint8_t kStatusShift = 5; // Bits 7:5
constexpr uint8_t kStatusMask = 0x07;
constexpr uint8_t kLocationShift = 0; // Bits 4:0
constexpr uint8_t kLocationMask = 0x1F;

/// Shared "is this WORD the 0x8000 unknown sentinel" check, implemented
/// once per this .cpp per this story's own Key constraints note, rather
/// than inlining the comparison at each of this class's 5 WORD-field call
/// sites.
bool is_word_value_unknown(uint16_t value)
{
    return value == ElectricalCurrentProbeEntry::ValueUnknown;
}

} // namespace

ElectricalCurrentProbeEntry::ElectricalCurrentProbeEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::ElectricalCurrentProbe) {
        std::stringstream err;
        err << "Wrong entry type, expected Electrical Current Probe, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.30 (Table 101/102).
    init_string_values();
}

void ElectricalCurrentProbeEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 102.
    status_map_[StatusOutOfSpec] = "Reserved";
    status_map_[StatusOther] = "Other";
    status_map_[StatusUnknown] = "Unknown";
    status_map_[StatusOk] = "OK";
    status_map_[StatusNonCritical] = "Non-critical";
    status_map_[StatusCritical] = "Critical";
    status_map_[StatusNonRecoverable] = "Non-recoverable";

    location_map_[LocationOutOfSpec] = "Reserved";
    location_map_[LocationOther] = "Other";
    location_map_[LocationUnknown] = "Unknown";
    location_map_[LocationProcessor] = "Processor";
    location_map_[LocationDisk] = "Disk";
    location_map_[LocationPeripheralBay] = "Peripheral Bay";
    location_map_[LocationSystemManagementModule] = "System Management Module";
    location_map_[LocationMotherboard] = "Motherboard";
    location_map_[LocationMemoryModule] = "Memory Module";
    location_map_[LocationProcessorModule] = "Processor Module";
    location_map_[LocationPowerUnit] = "Power Unit";
    location_map_[LocationAddInCard] = "Add-in Card";
}

std::string ElectricalCurrentProbeEntry::get_type() const
{
    return "Electrical Current Probe";
}

std::string ElectricalCurrentProbeEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Description: " << get_description_string() << '\n';
    description << "Location: " << get_location_string() << '\n';
    description << "Status: " << get_status_string() << '\n';
    description << "Maximum Value: "
                 << (is_maximum_value_known() ? std::to_string(get_maximum_value()) + " mA" : "Unknown") << '\n';
    description << "Minimum Value: "
                 << (is_minimum_value_known() ? std::to_string(get_minimum_value()) + " mA" : "Unknown") << '\n';
    description << "Resolution: "
                 << (is_resolution_known() ? std::to_string(get_resolution()) + " (tenths of mA)" : "Unknown") << '\n';
    description << "Tolerance: "
                 << (is_tolerance_known() ? std::to_string(get_tolerance()) + " mA" : "Unknown") << '\n';
    description << "Accuracy: "
                 << (is_accuracy_known() ? std::to_string(get_accuracy()) + " (1/100th %)" : "Unknown") << '\n';
    if (has_nominal_value()) {
        description << "Nominal Value: "
                     << (is_nominal_value_known() ? std::to_string(get_nominal_value()) + " mA" : "Unknown") << '\n';
    }

    return description.str();
}

std::string ElectricalCurrentProbeEntry::get_description_string() const
{
    if (get_entry_size() < kDescriptionMinLength) return std::string();
    return dmi_string(read_u8(raw_data(), kDescriptionOffset));
}

uint8_t ElectricalCurrentProbeEntry::get_location_and_status() const
{
    if (get_entry_size() < kLocationAndStatusMinLength) return 0;
    return read_u8(raw_data(), kLocationAndStatusOffset);
}

uint16_t ElectricalCurrentProbeEntry::get_maximum_value() const
{
    if (get_entry_size() < kMaximumValueMinLength) return ValueUnknown;
    return read_u16(raw_data(), kMaximumValueOffset);
}

uint16_t ElectricalCurrentProbeEntry::get_minimum_value() const
{
    if (get_entry_size() < kMinimumValueMinLength) return ValueUnknown;
    return read_u16(raw_data(), kMinimumValueOffset);
}

uint16_t ElectricalCurrentProbeEntry::get_resolution() const
{
    if (get_entry_size() < kResolutionMinLength) return ValueUnknown;
    return read_u16(raw_data(), kResolutionOffset);
}

uint16_t ElectricalCurrentProbeEntry::get_tolerance() const
{
    if (get_entry_size() < kToleranceMinLength) return ValueUnknown;
    return read_u16(raw_data(), kToleranceOffset);
}

uint16_t ElectricalCurrentProbeEntry::get_accuracy() const
{
    if (get_entry_size() < kAccuracyMinLength) return ValueUnknown;
    return read_u16(raw_data(), kAccuracyOffset);
}

uint32_t ElectricalCurrentProbeEntry::get_oem_defined() const
{
    if (get_entry_size() < kOemDefinedMinLength) return 0;
    return read_u32(raw_data(), kOemDefinedOffset);
}

uint16_t ElectricalCurrentProbeEntry::get_nominal_value() const
{
    if (!has_nominal_value()) return ValueUnknown;
    return read_u16(raw_data(), kNominalValueOffset);
}

ElectricalCurrentProbeEntry::Status ElectricalCurrentProbeEntry::get_status() const
{
    return static_cast<Status>((get_location_and_status() >> kStatusShift) & kStatusMask);
}

std::string ElectricalCurrentProbeEntry::get_status_string() const
{
    auto it = status_map_.find(get_status());
    if (it != status_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

ElectricalCurrentProbeEntry::Location ElectricalCurrentProbeEntry::get_location() const
{
    return static_cast<Location>((get_location_and_status() >> kLocationShift) & kLocationMask);
}

std::string ElectricalCurrentProbeEntry::get_location_string() const
{
    auto it = location_map_.find(get_location());
    if (it != location_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

bool ElectricalCurrentProbeEntry::is_maximum_value_known() const
{
    return !is_word_value_unknown(get_maximum_value());
}

bool ElectricalCurrentProbeEntry::is_minimum_value_known() const
{
    return !is_word_value_unknown(get_minimum_value());
}

bool ElectricalCurrentProbeEntry::is_resolution_known() const
{
    return !is_word_value_unknown(get_resolution());
}

bool ElectricalCurrentProbeEntry::is_tolerance_known() const
{
    return !is_word_value_unknown(get_tolerance());
}

bool ElectricalCurrentProbeEntry::is_accuracy_known() const
{
    return !is_word_value_unknown(get_accuracy());
}

bool ElectricalCurrentProbeEntry::has_nominal_value() const
{
    return get_entry_size() >= kNominalValueMinLength;
}

bool ElectricalCurrentProbeEntry::is_nominal_value_known() const
{
    return has_nominal_value() && !is_word_value_unknown(get_nominal_value());
}

std::vector<NamedField> ElectricalCurrentProbeEntry::get_fields() const
{
    // Same "<value><unit>" or "Unknown" string the text prints.
    auto decorated = [](bool known, uint16_t value, const char* unit) {
        return known ? std::to_string(value) + unit : std::string("Unknown");
    };

    std::vector<NamedField> fields;
    fields.push_back({"Description", get_description_string()});
    fields.push_back({"Location", get_location_string()});
    fields.push_back({"Status", get_status_string()});
    fields.push_back({"Maximum Value", decorated(is_maximum_value_known(), get_maximum_value(), " mA")});
    fields.push_back({"Minimum Value", decorated(is_minimum_value_known(), get_minimum_value(), " mA")});
    fields.push_back({"Resolution", decorated(is_resolution_known(), get_resolution(), " (tenths of mA)")});
    fields.push_back({"Tolerance", decorated(is_tolerance_known(), get_tolerance(), " mA")});
    fields.push_back({"Accuracy", decorated(is_accuracy_known(), get_accuracy(), " (1/100th %)")});
    if (has_nominal_value()) {
        fields.push_back({"Nominal Value", decorated(is_nominal_value_known(), get_nominal_value(), " mA")});
    }
    return fields;
}
