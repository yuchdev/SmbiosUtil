#include <libsmbios/tables/hardware_security_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::HardwareSecurity at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::HardwareSecurity, HardwareSecurityEntry)

namespace {

constexpr size_t kSettingsOffset = 0x04;
constexpr size_t kSettingsMinLength = 0x05;

/// Settings bit positions/masks, DSP0134 3.9.0 Table 93 (see
/// hardware_security_entry.hpp's class doc comment for the full confirmed
/// layout).
constexpr uint8_t kPowerOnPasswordShift = 6; // Bits 7:6
constexpr uint8_t kKeyboardPasswordShift = 4; // Bits 5:4
constexpr uint8_t kAdministratorPasswordShift = 2; // Bits 3:2
constexpr uint8_t kFrontPanelResetShift = 0; // Bits 1:0
constexpr uint8_t kStatusMask = 0x03;

} // namespace

HardwareSecurityEntry::HardwareSecurityEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::HardwareSecurity) {
        std::stringstream err;
        err << "Wrong entry type, expected Hardware Security, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // The single formatted field below is distinguished by header.length
    // alone, confirmed against docs/standard/DSP0134_3.9.0.md's own 7.25
    // (Table 93).
    init_string_values();
}

void HardwareSecurityEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 93; shared by all four 2-bit status fields.
    status_map_[Disabled] = "Disabled";
    status_map_[Enabled] = "Enabled";
    status_map_[NotImplemented] = "Not Implemented";
    status_map_[Unknown] = "Unknown";
}

std::string HardwareSecurityEntry::get_type() const
{
    return "Hardware Security";
}

std::string HardwareSecurityEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Power-on Password Status: " << get_power_on_password_status_string() << '\n';
    description << "Keyboard Password Status: " << get_keyboard_password_status_string() << '\n';
    description << "Administrator Password Status: " << get_administrator_password_status_string() << '\n';
    description << "Front Panel Reset Status: " << get_front_panel_reset_status_string() << '\n';

    return description.str();
}

uint8_t HardwareSecurityEntry::get_settings() const
{
    if (get_entry_size() < kSettingsMinLength) return 0;
    return read_u8(raw_data(), kSettingsOffset);
}

HardwareSecurityEntry::SecurityStatus HardwareSecurityEntry::get_power_on_password_status() const
{
    return static_cast<SecurityStatus>((get_settings() >> kPowerOnPasswordShift) & kStatusMask);
}

std::string HardwareSecurityEntry::get_power_on_password_status_string() const
{
    return status_string(get_power_on_password_status());
}

HardwareSecurityEntry::SecurityStatus HardwareSecurityEntry::get_keyboard_password_status() const
{
    return static_cast<SecurityStatus>((get_settings() >> kKeyboardPasswordShift) & kStatusMask);
}

std::string HardwareSecurityEntry::get_keyboard_password_status_string() const
{
    return status_string(get_keyboard_password_status());
}

HardwareSecurityEntry::SecurityStatus HardwareSecurityEntry::get_administrator_password_status() const
{
    return static_cast<SecurityStatus>((get_settings() >> kAdministratorPasswordShift) & kStatusMask);
}

std::string HardwareSecurityEntry::get_administrator_password_status_string() const
{
    return status_string(get_administrator_password_status());
}

HardwareSecurityEntry::SecurityStatus HardwareSecurityEntry::get_front_panel_reset_status() const
{
    return static_cast<SecurityStatus>((get_settings() >> kFrontPanelResetShift) & kStatusMask);
}

std::string HardwareSecurityEntry::get_front_panel_reset_status_string() const
{
    return status_string(get_front_panel_reset_status());
}

std::string HardwareSecurityEntry::status_string(SecurityStatus status) const
{
    assert(!status_map_.empty());
    auto it = status_map_.find(status);
    if (it != status_map_.end()) {
        return (*it).second;
    }
    return "Unknown";
}
