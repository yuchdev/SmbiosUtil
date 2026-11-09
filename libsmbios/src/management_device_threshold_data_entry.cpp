#include <libsmbios/tables/management_device_threshold_data_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::ManagementDeviceThresholdData at
// static-init time; see REGISTER_SMBIOS_ENTRY's doc comment
// (smbios_entry_factory.hpp) for why this lives here rather than in the
// header.
REGISTER_SMBIOS_ENTRY(SMBios::ManagementDeviceThresholdData, ManagementDeviceThresholdDataEntry)

namespace {

constexpr size_t kLowerThresholdNonCriticalOffset = 0x04;
constexpr size_t kUpperThresholdNonCriticalOffset = 0x06;
constexpr size_t kLowerThresholdCriticalOffset = 0x08;
constexpr size_t kUpperThresholdCriticalOffset = 0x0A;
constexpr size_t kLowerThresholdNonRecoverableOffset = 0x0C;
constexpr size_t kUpperThresholdNonRecoverableOffset = 0x0E;

constexpr size_t kLowerThresholdNonCriticalMinLength = 0x06;
constexpr size_t kUpperThresholdNonCriticalMinLength = 0x08;
constexpr size_t kLowerThresholdCriticalMinLength = 0x0A;
constexpr size_t kUpperThresholdCriticalMinLength = 0x0C;
constexpr size_t kLowerThresholdNonRecoverableMinLength = 0x0E;
constexpr size_t kUpperThresholdNonRecoverableMinLength = 0x10;

/// Shared "is this WORD the 0x8000 unknown sentinel" check, implemented
/// once per this .cpp per this story's own Key constraints note (same
/// pattern as VoltageProbeEntry/TemperatureProbeEntry/
/// ElectricalCurrentProbeEntry/CoolingDeviceEntry's own private helpers,
/// Story 04.0), rather than inlining the comparison at each of this
/// class's 6 WORD-field call sites.
bool is_word_value_unknown(uint16_t value)
{
    return value == ManagementDeviceThresholdDataEntry::ValueUnknown;
}

} // namespace

ManagementDeviceThresholdDataEntry::ManagementDeviceThresholdDataEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::ManagementDeviceThresholdData) {
        std::stringstream err;
        err << "Wrong entry type, expected Management Device Threshold Data, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.37 (Table 111). This
    // structure deliberately has no unit information of its own - see this
    // class's own header comment.
}

std::string ManagementDeviceThresholdDataEntry::get_type() const
{
    return "Management Device Threshold Data";
}

std::string ManagementDeviceThresholdDataEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Lower Threshold - Non-critical: "
                 << (is_lower_threshold_non_critical_known() ? std::to_string(get_lower_threshold_non_critical()) : "Unknown") << '\n';
    description << "Upper Threshold - Non-critical: "
                 << (is_upper_threshold_non_critical_known() ? std::to_string(get_upper_threshold_non_critical()) : "Unknown") << '\n';
    description << "Lower Threshold - Critical: "
                 << (is_lower_threshold_critical_known() ? std::to_string(get_lower_threshold_critical()) : "Unknown") << '\n';
    description << "Upper Threshold - Critical: "
                 << (is_upper_threshold_critical_known() ? std::to_string(get_upper_threshold_critical()) : "Unknown") << '\n';
    description << "Lower Threshold - Non-recoverable: "
                 << (is_lower_threshold_non_recoverable_known() ? std::to_string(get_lower_threshold_non_recoverable()) : "Unknown") << '\n';
    description << "Upper Threshold - Non-recoverable: "
                 << (is_upper_threshold_non_recoverable_known() ? std::to_string(get_upper_threshold_non_recoverable()) : "Unknown") << '\n';

    return description.str();
}

uint16_t ManagementDeviceThresholdDataEntry::get_lower_threshold_non_critical() const
{
    if (get_entry_size() < kLowerThresholdNonCriticalMinLength) return ValueUnknown;
    return read_u16(raw_data(), kLowerThresholdNonCriticalOffset);
}

uint16_t ManagementDeviceThresholdDataEntry::get_upper_threshold_non_critical() const
{
    if (get_entry_size() < kUpperThresholdNonCriticalMinLength) return ValueUnknown;
    return read_u16(raw_data(), kUpperThresholdNonCriticalOffset);
}

uint16_t ManagementDeviceThresholdDataEntry::get_lower_threshold_critical() const
{
    if (get_entry_size() < kLowerThresholdCriticalMinLength) return ValueUnknown;
    return read_u16(raw_data(), kLowerThresholdCriticalOffset);
}

uint16_t ManagementDeviceThresholdDataEntry::get_upper_threshold_critical() const
{
    if (get_entry_size() < kUpperThresholdCriticalMinLength) return ValueUnknown;
    return read_u16(raw_data(), kUpperThresholdCriticalOffset);
}

uint16_t ManagementDeviceThresholdDataEntry::get_lower_threshold_non_recoverable() const
{
    if (get_entry_size() < kLowerThresholdNonRecoverableMinLength) return ValueUnknown;
    return read_u16(raw_data(), kLowerThresholdNonRecoverableOffset);
}

uint16_t ManagementDeviceThresholdDataEntry::get_upper_threshold_non_recoverable() const
{
    if (get_entry_size() < kUpperThresholdNonRecoverableMinLength) return ValueUnknown;
    return read_u16(raw_data(), kUpperThresholdNonRecoverableOffset);
}

bool ManagementDeviceThresholdDataEntry::is_lower_threshold_non_critical_known() const
{
    return !is_word_value_unknown(get_lower_threshold_non_critical());
}

bool ManagementDeviceThresholdDataEntry::is_upper_threshold_non_critical_known() const
{
    return !is_word_value_unknown(get_upper_threshold_non_critical());
}

bool ManagementDeviceThresholdDataEntry::is_lower_threshold_critical_known() const
{
    return !is_word_value_unknown(get_lower_threshold_critical());
}

bool ManagementDeviceThresholdDataEntry::is_upper_threshold_critical_known() const
{
    return !is_word_value_unknown(get_upper_threshold_critical());
}

bool ManagementDeviceThresholdDataEntry::is_lower_threshold_non_recoverable_known() const
{
    return !is_word_value_unknown(get_lower_threshold_non_recoverable());
}

bool ManagementDeviceThresholdDataEntry::is_upper_threshold_non_recoverable_known() const
{
    return !is_word_value_unknown(get_upper_threshold_non_recoverable());
}
