#pragma once
#include <cstdint>
#include <string>
#include <smbios/abstract_smbios_entry.h>

// System Power Controls entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Power Controls'
// chapter (7.26), DSP0134 3.9.0. Field offsets confirmed directly against
// docs/standard/DSP0134_3.9.0.md's own 7.26/7.26.1 (Table 94) - unlike
// System Reset/Hardware Security, this task's own source excerpt for this
// table was NOT redacted, so no separate bit-layout confirmation note is
// needed here; every field is a whole BCD byte, not a sub-byte bit-field.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The System Power Controls structure (7.26) - the next scheduled
/// power-on month/day/hour/minute/second, each encoded as a BCD byte
/// (7.26.1). Software uses these fields to determine the number of seconds
/// until the next scheduled power-on.
class SystemPowerControlsEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemPowerControlsEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemPowerControlsEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Raw BCD accessors

    /// @brief 0x04 offset, raw BCD byte. Documented range 01h-12h; a value
    /// outside that range does not contribute to the next-power-on
    /// calculation, per 7.26.1 (this class does not enforce the range
    /// itself - see bcd_to_decimal()'s own doc comment). Returns 0 if
    /// get_entry_size() < 0x05.
    uint8_t get_month_bcd() const;

    /// @brief 0x05 offset, raw BCD byte. Documented range 01h-31h; see
    /// get_month_bcd()'s doc comment for the out-of-range convention.
    /// Returns 0 if get_entry_size() < 0x06.
    uint8_t get_day_bcd() const;

    /// @brief 0x06 offset, raw BCD byte. Documented range 00h-23h; see
    /// get_month_bcd()'s doc comment for the out-of-range convention.
    /// Returns 0 if get_entry_size() < 0x07.
    uint8_t get_hour_bcd() const;

    /// @brief 0x07 offset, raw BCD byte. Documented range 00h-59h; see
    /// get_month_bcd()'s doc comment for the out-of-range convention.
    /// Returns 0 if get_entry_size() < 0x08.
    uint8_t get_minute_bcd() const;

    /// @brief 0x08 offset, raw BCD byte. Documented range 00h-59h; see
    /// get_month_bcd()'s doc comment for the out-of-range convention.
    /// Returns 0 if get_entry_size() < 0x09.
    uint8_t get_second_bcd() const;

    //////////////////////////////////////////////////////////////////////////
    // Decimal convenience accessors

    /// @brief get_month_bcd() converted via bcd_to_decimal(); see that
    /// helper's doc comment - this does NOT validate the result against
    /// the documented 1-12 range.
    uint8_t get_month() const;

    /// @brief get_day_bcd() converted via bcd_to_decimal(); see that
    /// helper's doc comment - this does NOT validate the result against
    /// the documented 1-31 range.
    uint8_t get_day() const;

    /// @brief get_hour_bcd() converted via bcd_to_decimal(); see that
    /// helper's doc comment - this does NOT validate the result against
    /// the documented 0-23 range.
    uint8_t get_hour() const;

    /// @brief get_minute_bcd() converted via bcd_to_decimal(); see that
    /// helper's doc comment - this does NOT validate the result against
    /// the documented 0-59 range.
    uint8_t get_minute() const;

    /// @brief get_second_bcd() converted via bcd_to_decimal(); see that
    /// helper's doc comment - this does NOT validate the result against
    /// the documented 0-59 range.
    uint8_t get_second() const;

private:

    /// @brief Mechanical BCD-nibble-pair-to-decimal conversion (7.26.1):
    /// `(b >> 4) * 10 + (b & 0x0F)`. This is a purely mechanical nibble
    /// decode - it does NOT validate that either nibble is itself a valid
    /// BCD digit (0-9), nor that the resulting decimal value falls within
    /// any field's documented range (e.g. a malformed 0xFF input decodes
    /// to (15 * 10) + 15 = 165, not a valid month/day/hour/minute/second).
    /// Range validation, if needed, is the caller's/renderer's
    /// responsibility.
    static uint8_t bcd_to_decimal(uint8_t bcd);
};

} // namespace smbios
