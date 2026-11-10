#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Hardware Security entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Hardware Security' chapter
// (7.25), DSP0134 3.9.0.
//
// Hardware Security Settings bit layout (DSP0134 3.9.0 Table 93, confirmed
// directly against docs/standard/DSP0134_3.9.0.md's own 7.25, since this
// task's own source excerpt had this row redacted by the
// markdown-extraction tool):
//   Bits 7:6  Power-on Password Status.
//   Bits 5:4  Keyboard Password Status.
//   Bits 3:2  Administrator Password Status.
//   Bits 1:0  Front Panel Reset Status.
// All four 2-bit fields share the identical value space: 00b Disabled,
// 01b Enabled, 10b Not Implemented, 11b Unknown - hence the single shared
// SecurityStatus enum used by all four decoded accessors below.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Hardware Security structure (7.25) - system-wide password
/// and front-panel-reset status settings.
class HardwareSecurityEntry : public AbstractSMBiosEntry {
public:

    /// @brief Shared 2-bit value space for all four Hardware Security
    /// Settings sub-fields, per Table 93 (see class doc comment's
    /// bit-layout confirmation note).
    enum SecurityStatus : uint8_t {
        Disabled = 0x00,
        Enabled = 0x01,
        NotImplemented = 0x02,
        Unknown = 0x03
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    HardwareSecurityEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~HardwareSecurityEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order. Each of the
    /// four statuses is a four-state value, not a flag, so it stays the
    /// printed string.
    std::vector<NamedField> get_fields() const override;

    /// @brief 0x04 offset, raw byte. See the class doc comment for the
    /// confirmed Table 93 bit layout. Returns 0 if get_entry_size() < 0x05.
    uint8_t get_settings() const;

    /// @brief Bits 7:6 of Settings. Returns Disabled if get_entry_size() <
    /// 0x05.
    SecurityStatus get_power_on_password_status() const;

    /// @brief String representation of get_power_on_password_status().
    std::string get_power_on_password_status_string() const;

    /// @brief Bits 5:4 of Settings. Returns Disabled if get_entry_size() <
    /// 0x05.
    SecurityStatus get_keyboard_password_status() const;

    /// @brief String representation of get_keyboard_password_status().
    std::string get_keyboard_password_status_string() const;

    /// @brief Bits 3:2 of Settings. Returns Disabled if get_entry_size() <
    /// 0x05.
    SecurityStatus get_administrator_password_status() const;

    /// @brief String representation of
    /// get_administrator_password_status().
    std::string get_administrator_password_status_string() const;

    /// @brief Bits 1:0 of Settings. Returns Disabled if get_entry_size() <
    /// 0x05.
    SecurityStatus get_front_panel_reset_status() const;

    /// @brief String representation of get_front_panel_reset_status().
    std::string get_front_panel_reset_status_string() const;

private:

    /// Map SecurityStatus values to string values
    void init_string_values();

    /// String representation of a SecurityStatus value, shared by all four
    /// decoded accessors' string variants.
    std::string status_string(SecurityStatus status) const;

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> status_map_;
};

} // namespace smbios
