#include <libsmbios/tables/system_reset_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::SystemReset at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemReset, SystemResetEntry)

namespace {

constexpr size_t kCapabilitiesOffset = 0x04;
constexpr size_t kResetCountOffset = 0x05;
constexpr size_t kResetLimitOffset = 0x07;
constexpr size_t kTimerIntervalOffset = 0x09;
constexpr size_t kTimeoutOffset = 0x0B;

constexpr size_t kCapabilitiesMinLength = 0x05;
constexpr size_t kResetCountMinLength = 0x07;
constexpr size_t kResetLimitMinLength = 0x09;
constexpr size_t kTimerIntervalMinLength = 0x0B;
constexpr size_t kTimeoutMinLength = 0x0D;

/// Capabilities bit positions/masks, DSP0134 3.9.0 Table 92 (see
/// system_reset_entry.hpp's class doc comment for the full confirmed layout).
constexpr uint8_t kStatusMask = 0x01; // Bit 0
constexpr uint8_t kBootOptionShift = 1; // Bits 2:1
constexpr uint8_t kBootOptionMask = 0x03;
constexpr uint8_t kBootOptionOnLimitShift = 3; // Bits 4:3
constexpr uint8_t kBootOptionOnLimitMask = 0x03;
constexpr uint8_t kWatchdogMask = 0x20; // Bit 5

} // namespace

SystemResetEntry::SystemResetEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::SystemReset) {
        std::stringstream err;
        err << "Wrong entry type, expected System Reset, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.24 (Table 92).
    init_string_values();
}

void SystemResetEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 92; shared by Boot Option and Boot Option on
    // Limit, which occupy the identical 2-bit value space.
    boot_option_map_[Reserved] = "Reserved";
    boot_option_map_[OperatingSystem] = "Operating system";
    boot_option_map_[SystemUtilities] = "System utilities";
    boot_option_map_[DoNotReboot] = "Do not reboot";
}

std::string SystemResetEntry::get_type() const
{
    return "System Reset";
}

std::string SystemResetEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Status: " << (is_status_enabled() ? "Enabled" : "Disabled") << '\n';
    description << "Watchdog Timer: " << (is_watchdog_present() ? "Present" : "Not Present") << '\n';
    description << "Boot Option: " << get_boot_option_string() << '\n';
    description << "Boot Option on Limit: " << get_boot_option_on_limit_string() << '\n';
    description << "Reset Count: "
                 << (is_reset_count_known() ? std::to_string(get_reset_count()) : "Unknown") << '\n';
    description << "Reset Limit: "
                 << (is_reset_limit_known() ? std::to_string(get_reset_limit()) : "Unknown") << '\n';
    description << "Timer Interval: "
                 << (is_timer_interval_known() ? std::to_string(get_timer_interval()) + " min" : "Unknown") << '\n';
    description << "Timeout: "
                 << (is_timeout_known() ? std::to_string(get_timeout()) + " min" : "Unknown") << '\n';

    return description.str();
}

uint8_t SystemResetEntry::get_capabilities() const
{
    if (get_entry_size() < kCapabilitiesMinLength) return 0;
    return read_u8(raw_data(), kCapabilitiesOffset);
}

uint16_t SystemResetEntry::get_reset_count() const
{
    if (get_entry_size() < kResetCountMinLength) return ValueUnknown;
    return read_u16(raw_data(), kResetCountOffset);
}

uint16_t SystemResetEntry::get_reset_limit() const
{
    if (get_entry_size() < kResetLimitMinLength) return ValueUnknown;
    return read_u16(raw_data(), kResetLimitOffset);
}

uint16_t SystemResetEntry::get_timer_interval() const
{
    if (get_entry_size() < kTimerIntervalMinLength) return ValueUnknown;
    return read_u16(raw_data(), kTimerIntervalOffset);
}

uint16_t SystemResetEntry::get_timeout() const
{
    if (get_entry_size() < kTimeoutMinLength) return ValueUnknown;
    return read_u16(raw_data(), kTimeoutOffset);
}

bool SystemResetEntry::is_status_enabled() const
{
    return (get_capabilities() & kStatusMask) != 0;
}

bool SystemResetEntry::is_watchdog_present() const
{
    return (get_capabilities() & kWatchdogMask) != 0;
}

SystemResetEntry::BootOption SystemResetEntry::get_boot_option() const
{
    return static_cast<BootOption>((get_capabilities() >> kBootOptionShift) & kBootOptionMask);
}

std::string SystemResetEntry::get_boot_option_string() const
{
    return boot_option_string(get_boot_option());
}

SystemResetEntry::BootOption SystemResetEntry::get_boot_option_on_limit() const
{
    return static_cast<BootOption>((get_capabilities() >> kBootOptionOnLimitShift) & kBootOptionOnLimitMask);
}

std::string SystemResetEntry::get_boot_option_on_limit_string() const
{
    return boot_option_string(get_boot_option_on_limit());
}

std::string SystemResetEntry::boot_option_string(BootOption option) const
{
    assert(!boot_option_map_.empty());
    auto it = boot_option_map_.find(option);
    if (it != boot_option_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

bool SystemResetEntry::is_reset_count_known() const
{
    return get_reset_count() != ValueUnknown;
}

bool SystemResetEntry::is_reset_limit_known() const
{
    return get_reset_limit() != ValueUnknown;
}

bool SystemResetEntry::is_timer_interval_known() const
{
    return get_timer_interval() != ValueUnknown;
}

bool SystemResetEntry::is_timeout_known() const
{
    return get_timeout() != ValueUnknown;
}

std::vector<NamedField> SystemResetEntry::get_fields() const
{
    std::vector<NamedField> fields;
    fields.push_back({"Status", is_status_enabled()});
    fields.push_back({"Watchdog Timer", is_watchdog_present()});
    fields.push_back({"Boot Option", get_boot_option_string()});
    fields.push_back({"Boot Option on Limit", get_boot_option_on_limit_string()});
    fields.push_back({"Reset Count", is_reset_count_known() ? std::to_string(get_reset_count())
                                                            : std::string("Unknown")});
    fields.push_back({"Reset Limit", is_reset_limit_known() ? std::to_string(get_reset_limit())
                                                            : std::string("Unknown")});
    fields.push_back({"Timer Interval", is_timer_interval_known() ? std::to_string(get_timer_interval()) + " min"
                                                                  : std::string("Unknown")});
    fields.push_back({"Timeout", is_timeout_known() ? std::to_string(get_timeout()) + " min"
                                                    : std::string("Unknown")});
    return fields;
}
