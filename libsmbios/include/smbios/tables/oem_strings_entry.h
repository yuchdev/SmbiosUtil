#pragma once
#include <cstdint>
#include <string>
#include <smbios/abstract_smbios_entry.h>

// OEM Strings entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'OEM Strings' chapter
// (7.12), DSP0134 3.9.0. Field offset and the structure's "Count byte,
// then the trailing DMI string list" shape were confirmed directly against
// docs/standard/DSP0134_3.9.0.md's own section 7.12 (Table 56), not assumed
// from any task-document reproduction.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The OEM Strings structure (7.12) - free-form strings defined by
/// the OEM (e.g. part numbers for system reference documents, manufacturer
/// contact information). The only fixed field is Count (offset 0x04); every
/// bit of actual content is the trailing DMI string list it names, Count
/// strings, string indices 1..Count.
class OemStringsEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header, recognize how much information do we have
    /// in OEM Strings SMBIOS entry depending on header.length.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    OemStringsEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~OemStringsEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset
    /// Number of OEM strings that follow in the trailing DMI string list; 0
    /// if get_entry_size() < 0x05
    uint8_t get_string_count() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief One of the trailing OEM strings, by DMI string index.
    /// @param index 1-based, matching AbstractSMBiosEntry::dmi_string()'s
    /// own 1-based convention exactly (this accessor is a thin wrapper over
    /// it) - NOT 0-based. Valid range is `1..get_string_count()`; index 0
    /// returns dmi_string()'s own "Not Specified" sentinel, and any index
    /// at or past `dmi_string()`'s own internal string-table size returns
    /// its own "Bad index" sentinel - see AbstractSMBiosEntry::dmi_string()
    /// for both.
    /// @return the OEM string at `index`, or one of dmi_string()'s own
    /// out-of-range sentinels ("Not Specified" for index 0, "Bad index" for
    /// an index past the string table).
    std::string get_string_at(size_t index) const;
};

} // namespace smbios
