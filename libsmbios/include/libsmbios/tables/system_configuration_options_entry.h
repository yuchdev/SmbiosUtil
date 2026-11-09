#pragma once
#include <cstdint>
#include <string>
#include <libsmbios/abstract_smbios_entry.h>

// System Configuration Options entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Configuration
// Options' chapter (7.13), DSP0134 3.9.0. Field offset and the structure's
// "Count byte, then the trailing DMI string list" shape were confirmed
// directly against docs/standard/DSP0134_3.9.0.md's own section 7.13
// (Table 57), not assumed from any task-document reproduction.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The System Configuration Options structure (7.13) - free-form
/// strings describing how to configure the baseboard's jumpers and
/// switches (e.g. "JP2: 1-2 Cache Size is 256K, 2-3 Cache Size is 512K").
/// The only fixed field is Count (offset 0x04); every bit of actual
/// content is the trailing DMI string list it names, Count strings, string
/// indices 1..Count.
///
/// Byte-for-byte identical in shape to OemStringsEntry (7.12), but
/// deliberately NOT sharing a common base beyond AbstractSMBiosEntry
/// itself - the spec defines these as two unrelated structure types that
/// are only coincidentally identical in layout today, and this project does
/// not invent a shared intermediate base purely from current-shape
/// similarity (see this story's README "Key constraints").
class SystemConfigurationOptionsEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header, recognize how much information do we have
    /// in System Configuration Options SMBIOS entry depending on
    /// header.length.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemConfigurationOptionsEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemConfigurationOptionsEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset
    /// Number of configuration-option strings that follow in the trailing
    /// DMI string list; 0 if get_entry_size() < 0x05
    uint8_t get_option_count() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief One of the trailing configuration-option strings, by DMI
    /// string index.
    /// @param index 1-based, matching AbstractSMBiosEntry::dmi_string()'s
    /// own 1-based convention exactly (this accessor is a thin wrapper over
    /// it) - NOT 0-based. Valid range is `1..get_option_count()`; index 0
    /// returns dmi_string()'s own "Not Specified" sentinel, and any index
    /// at or past `dmi_string()`'s own internal string-table size returns
    /// its own "Bad index" sentinel - see AbstractSMBiosEntry::dmi_string()
    /// for both.
    /// @return the configuration-option string at `index`, or one of
    /// dmi_string()'s own out-of-range sentinels ("Not Specified" for index
    /// 0, "Bad index" for an index past the string table).
    std::string get_option_at(size_t index) const;
};

} // namespace smbios
