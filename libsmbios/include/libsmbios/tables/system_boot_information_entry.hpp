#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// System Boot Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Boot Information'
// chapter (7.33). Cross-checked against c-versions/dmidecode_linux/
// dmidecode.c's case 32 (dmi_decode(), ~line 5240, "System Boot
// Information") and its dmi_system_boot_status() helper for the field
// offset, length guard and status-code value table.
//
// Scope note on the reference's own dmi_system_boot_status() value table:
// the reference (both c-versions/dmidecode_linux and c-versions/
// dmidecode_macos, cross-checked against each other) defines only 9 named
// codes (0-8) plus two numeric ranges (128-191 "OEM-specific", 192-255
// "Product-specific") - this matches the DMTF 7.33.1 Boot Status table
// exactly. This is reproduced faithfully below via BootStatus/
// get_boot_status_string(); any other numeric table purporting to assign
// individual meanings to codes in the 9-20 range (e.g. a "Hard disk
// failure"/"Startup authentication failure"-style per-code breakdown) does
// not exist in this project's own reference material or the DMTF spec
// text and is not reproduced here.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The System Boot Information structure (7.33). Unlike every
/// other class in this milestone, this structure's "payload" past the
/// common DMIHeader is NOT a flat set of fixed-offset scalar fields:
///   - Bytes 0x04-0x09 (6 bytes): reserved, no defined meaning - not
///     exposed by any getter.
///   - Byte 0x0A: Boot Status, the one byte with a defined enum (see
///     BootStatus) - still read through the usual read_u8() + length-guard
///     pattern (R1/R2), per this task's own Constraints section.
///   - Bytes 0x0B onward (variable, up to `length - 0x0B` bytes):
///     vendor/status-code-specific raw data with no further defined
///     structure in the base spec - exposed as a raw byte vector
///     (get_status_data()) rather than decoded further, since there is no
///     fixed per-field layout to decode. This is the one place in this
///     class that is exempt from the usual per-field read_uN treatment -
///     the fixed status-code byte itself is not exempt.
class SystemBootInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief Boot Status field (offset 0x0A), see dmi_system_boot_status()
    // (7.33.1 of the spec). Named codes cover 0-8; 128-191 and 192-255 are
    // numeric ranges ("OEM-specific"/"Product-specific") rather than
    // individually named values - see get_boot_status_string().
    enum BootStatus : uint8_t {
        NoErrorsDetected = 0,
        NoBootableMedia = 1,
        OperatingSystemFailedToLoad = 2,
        FirmwareDetectedHardwareFailure = 3,
        OperatingSystemDetectedHardwareFailure = 4,
        UserRequestedBoot = 5,
        SystemSecurityViolation = 6,
        PreviouslyRequestedImage = 7,
        SystemWatchdogTimerExpired = 8
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in System Boot Information SMBIOS entry depending on version and
    /// size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemBootInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemBootInformationEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x0A offset
    /// Raw Boot Status code; see BootStatus enum and
    /// get_boot_status_string(). Defaults to 0 (NoErrorsDetected) when the
    /// structure is too short to carry this byte, matching this project's
    /// uniform "0 when absent" convention for guarded getters even though
    /// 0 also happens to be a real, meaningful status code.
    uint8_t get_boot_status() const;

    /// @brief Bytes 0x0B onward, variable length (`get_entry_size() -
    /// 0x0B`, or empty when get_entry_size() <= 0x0B): vendor/status-code-
    /// specific raw payload with no further defined structure - see class
    /// doc comment. Returned as a copy since the payload length varies per
    /// instance and callers should not need to track the owning entry's
    /// lifetime to use it.
    std::vector<uint8_t> get_status_data() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Boot status string representation: the named 0-8 table,
    /// "OEM-specific" for 128-191, "Product-specific" for 192-255, else
    /// "OutOfSpec" (reserved-for-future-assignment range 9-127)
    std::string get_boot_status_string() const;

    /// @brief Status data, rendered as a space-separated hex byte string
    /// (e.g. "01 02 03"), or "" when empty - this is a raw hex dump, not a
    /// decode, matching the class doc comment's own scope note
    std::string get_status_data_string() const;

private:

    /// Map boot status codes to string values
    void init_string_values();

private:

    /// Code to string representation (named 0-8 codes only; 128-191/192-255
    /// are rendered via explicit range checks in get_boot_status_string())
    std::map<uint8_t, std::string> boot_status_map_;
};

} // namespace smbios
