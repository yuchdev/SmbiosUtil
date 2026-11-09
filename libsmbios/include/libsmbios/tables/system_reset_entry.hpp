#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// System Reset entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Reset' chapter
// (7.24), DSP0134 3.9.0.
//
// Capabilities bit layout (DSP0134 3.9.0 Table 92, confirmed directly
// against docs/standard/DSP0134_3.9.0.md's own 7.24, since this task's own
// source excerpt had this row redacted by the markdown-extraction tool):
//   Bits 7:6  Reserved for future assignment by this specification; set to
//             00b.
//   Bit  5    System contains a watchdog timer: True (1) or False (0).
//   Bits 4:3  Boot Option on Limit - action taken when the Reset Limit is
//             reached: 00b Reserved (do not use), 01b Operating system,
//             10b System utilities, 11b Do not reboot.
//   Bits 2:1  Boot Option - action taken after a watchdog reset: 00b
//             Reserved (do not use), 01b Operating system, 10b System
//             utilities, 11b Do not reboot.
//   Bit  0    Status - whether (1) or not (0) the system reset is enabled
//             by the user.
// Boot Option and Boot Option on Limit share the identical 2-bit value
// space, so both are exposed through the same BootOption enum.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The System Reset structure (7.24) - whether Automatic System
/// Reset is enabled, whether a watchdog timer is present, the boot action
/// taken after a watchdog reset and after the reset-attempt limit is
/// reached, and reset/limit/interval/timeout counters.
class SystemResetEntry : public AbstractSMBiosEntry {
public:

    /// @brief Boot Option / Boot Option on Limit 2-bit value space, shared
    /// by both fields per Table 92 (see class doc comment's bit-layout
    /// confirmation note). `00b` is documented "Reserved, do not use" - kept
    /// here as `Reserved` rather than this project's usual `OutOfSpec`
    /// sentinel, since the spec itself names it a reserved value, not an
    /// out-of-spec one.
    enum BootOption : uint8_t {
        Reserved = 0x00,
        OperatingSystem = 0x01,
        SystemUtilities = 0x02,
        DoNotReboot = 0x03
    };

    /// @brief Reset Count/Reset Limit/Timer Interval/Timeout sentinel
    /// (0xFFFF): the value is unknown, per spec.
    static constexpr uint16_t ValueUnknown = 0xFFFF;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemResetEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemResetEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset, raw byte. See the class doc comment for the
    /// confirmed Table 92 bit layout. Returns 0 if get_entry_size() < 0x05.
    uint8_t get_capabilities() const;

    /// @brief 0x05 offset, 2 raw bytes. Number of automatic system resets
    /// since the last intentional reset. ValueUnknown (0xFFFF) means
    /// unknown, per spec. Returns ValueUnknown if get_entry_size() < 0x07.
    uint16_t get_reset_count() const;

    /// @brief 0x07 offset, 2 raw bytes. Number of consecutive times the
    /// system reset is attempted. ValueUnknown (0xFFFF) means unknown, per
    /// spec. Returns ValueUnknown if get_entry_size() < 0x09.
    uint16_t get_reset_limit() const;

    /// @brief 0x09 offset, 2 raw bytes, minutes. Watchdog timer interval:
    /// if the timer is not reset within this interval, the system reset
    /// timeout begins. ValueUnknown (0xFFFF) means unknown, per spec.
    /// Returns ValueUnknown if get_entry_size() < 0x0B.
    uint16_t get_timer_interval() const;

    /// @brief 0x0B offset, 2 raw bytes, minutes. Time before reboot is
    /// initiated after a system power cycle, system reset (local or
    /// remote), or automatic system reset. ValueUnknown (0xFFFF) means
    /// unknown, per spec. Returns ValueUnknown if get_entry_size() < 0x0D.
    uint16_t get_timeout() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Capabilities sub-fields (Table 92)

    /// @brief Bit 0 of Capabilities: whether the system reset is enabled by
    /// the user. Returns false if get_entry_size() < 0x05.
    bool is_status_enabled() const;

    /// @brief Bit 5 of Capabilities: whether the system contains a watchdog
    /// timer. Returns false if get_entry_size() < 0x05.
    bool is_watchdog_present() const;

    /// @brief Bits 2:1 of Capabilities: action taken after a watchdog
    /// reset. See BootOption for values. Returns Reserved if
    /// get_entry_size() < 0x05.
    BootOption get_boot_option() const;

    /// @brief String representation of get_boot_option().
    std::string get_boot_option_string() const;

    /// @brief Bits 4:3 of Capabilities: action taken when the Reset Limit
    /// is reached. See BootOption for values. Returns Reserved if
    /// get_entry_size() < 0x05.
    BootOption get_boot_option_on_limit() const;

    /// @brief String representation of get_boot_option_on_limit().
    std::string get_boot_option_on_limit_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether Reset Count (0x05) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x07.
    bool is_reset_count_known() const;

    /// @brief Whether Reset Limit (0x07) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x09.
    bool is_reset_limit_known() const;

    /// @brief Whether Timer Interval (0x09) is known, i.e. not
    /// ValueUnknown. Returns false if get_entry_size() < 0x0B.
    bool is_timer_interval_known() const;

    /// @brief Whether Timeout (0x0B) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x0D.
    bool is_timeout_known() const;

private:

    /// Map Boot Option / Boot Option on Limit values to string values
    void init_string_values();

    /// String representation of a BootOption value, shared by
    /// get_boot_option_string() and get_boot_option_on_limit_string().
    std::string boot_option_string(BootOption option) const;

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> boot_option_map_;
};

} // namespace smbios
