#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// String Property entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'String Property' chapter
// (7.47), DSP0134 3.9.0. Field offsets (Table 138) and the String Property
// ID ranges (7.47.1, Table 139) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
//
// A String Property structure adds a string-valued property to another
// SMBIOS structure (the Parent Handle, 0x07) without having to modify that
// structure's own definition. Multiple Type 46 structures may add
// different properties to the same parent.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The String Property structure (7.47) - a cross-reference
/// structure that attaches one string-valued property (identified by a
/// String Property ID, 7.47.1) to another SMBIOS structure referenced by
/// its handle.
class StringPropertyEntry : public AbstractSMBiosEntry {
public:

    /// @brief String Property ID values, DSP0134 3.9.0 Table 139 (7.47.1).
    /// Only `UefiDevicePath` (1) is currently assigned by the
    /// specification; `Reserved` (0) is explicitly "do not use". The three
    /// numeric ranges above `UefiDevicePath` (2-32767, 32768-49151,
    /// 49152-65535) are not individually enumerated here - they are
    /// reserved *ranges*, not individual values - and are instead handled
    /// directly by get_property_id_string()'s range checks.
    enum PropertyId : uint16_t {
        Reserved = 0,
        UefiDevicePath = 1
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    StringPropertyEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~StringPropertyEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Raw/string values

    /// @brief 0x04 offset, raw WORD (Table 139, 7.47.1). Returns
    /// Reserved (0) if get_entry_size() < 0x06.
    uint16_t get_property_id() const;

    /// @brief String representation of get_property_id(): "Reserved" for
    /// 0, "UEFI device path" for 1, and - per 7.47.1's own three distinct
    /// reserved *ranges* - "Reserved (DMTF)" for 2-32767, "Reserved
    /// (Firmware Vendor)" for 32768-49151, and "Reserved (OEM)" for
    /// 49152-65535, rather than a flat "Unknown" for every unassigned
    /// value. These ranges carry meaning (which party is permitted to
    /// claim a future value within them) even though no specific ID within
    /// any of them is assigned yet, so collapsing them into "Unknown"
    /// would discard information the spec itself conveys.
    std::string get_property_id_string() const;

    /// @brief 0x06 offset DMI string: the String Property Value itself,
    /// whose own format/meaning is defined by get_property_id(). Returns
    /// dmi_string(0) ("Not Specified") if get_entry_size() < 0x07.
    std::string get_value_string() const;

    /// @brief 0x07 offset, raw WORD: handle of the SMBIOS structure this
    /// string property applies to. Returns 0 if get_entry_size() < 0x09.
    uint16_t get_parent_handle() const;
};

} // namespace smbios
