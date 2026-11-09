#include <libsmbios/tables/system_power_controls_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::SystemPowerControls at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemPowerControls, SystemPowerControlsEntry)

namespace {

constexpr size_t kMonthOffset = 0x04;
constexpr size_t kDayOffset = 0x05;
constexpr size_t kHourOffset = 0x06;
constexpr size_t kMinuteOffset = 0x07;
constexpr size_t kSecondOffset = 0x08;

constexpr size_t kMonthMinLength = 0x05;
constexpr size_t kDayMinLength = 0x06;
constexpr size_t kHourMinLength = 0x07;
constexpr size_t kMinuteMinLength = 0x08;
constexpr size_t kSecondMinLength = 0x09;

} // namespace

SystemPowerControlsEntry::SystemPowerControlsEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::SystemPowerControls) {
        std::stringstream err;
        err << "Wrong entry type, expected System Power Controls, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.26/7.26.1 (Table 94).
}

std::string SystemPowerControlsEntry::get_type() const
{
    return "System Power Controls";
}

std::string SystemPowerControlsEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Next Scheduled Power-on: "
                 << static_cast<unsigned>(get_month()) << '/'
                 << static_cast<unsigned>(get_day()) << ' '
                 << static_cast<unsigned>(get_hour()) << ':'
                 << static_cast<unsigned>(get_minute()) << ':'
                 << static_cast<unsigned>(get_second()) << '\n';

    return description.str();
}

uint8_t SystemPowerControlsEntry::get_month_bcd() const
{
    if (get_entry_size() < kMonthMinLength) return 0;
    return read_u8(raw_data(), kMonthOffset);
}

uint8_t SystemPowerControlsEntry::get_day_bcd() const
{
    if (get_entry_size() < kDayMinLength) return 0;
    return read_u8(raw_data(), kDayOffset);
}

uint8_t SystemPowerControlsEntry::get_hour_bcd() const
{
    if (get_entry_size() < kHourMinLength) return 0;
    return read_u8(raw_data(), kHourOffset);
}

uint8_t SystemPowerControlsEntry::get_minute_bcd() const
{
    if (get_entry_size() < kMinuteMinLength) return 0;
    return read_u8(raw_data(), kMinuteOffset);
}

uint8_t SystemPowerControlsEntry::get_second_bcd() const
{
    if (get_entry_size() < kSecondMinLength) return 0;
    return read_u8(raw_data(), kSecondOffset);
}

uint8_t SystemPowerControlsEntry::get_month() const
{
    return bcd_to_decimal(get_month_bcd());
}

uint8_t SystemPowerControlsEntry::get_day() const
{
    return bcd_to_decimal(get_day_bcd());
}

uint8_t SystemPowerControlsEntry::get_hour() const
{
    return bcd_to_decimal(get_hour_bcd());
}

uint8_t SystemPowerControlsEntry::get_minute() const
{
    return bcd_to_decimal(get_minute_bcd());
}

uint8_t SystemPowerControlsEntry::get_second() const
{
    return bcd_to_decimal(get_second_bcd());
}

uint8_t SystemPowerControlsEntry::bcd_to_decimal(uint8_t bcd)
{
    // Mechanical nibble-pair conversion only - see the header's doc
    // comment for why this does not validate against any documented
    // range.
    return static_cast<uint8_t>(((bcd >> 4) * 10) + (bcd & 0x0F));
}
