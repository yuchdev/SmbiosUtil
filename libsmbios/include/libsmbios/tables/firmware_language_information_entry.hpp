#pragma once
#include <cstdint>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios.hpp>

// Firmware Language Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Firmware Language
// Information' chapter (7.14), DSP0134 3.9.0. Field offsets (including the
// 0x15 Current Language offset and 0x16 total structure length) and the
// version gating on the Flags field (2.1+) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md's own section 7.14 (Table 58), not assumed
// from any task-document reproduction.
//
// Discrepancy found against the task document: the task's own description
// of get_language_at()'s indexing ("language strings start at DMI string
// index 1 immediately after Current Language's own index") does not match
// 7.14's own worked Example 1/Example 2 ("db 2 ; current language is French
// Canadian" followed immediately by three strings 'en|US|...', 'fr|CA|...',
// 'ja|JP|...' at string indices 1, 2, 3 - index 2, the Current Language
// value, resolves directly to the *second* of those three strings, not to
// a string positioned after a separate "current language" string). Current
// Language (0x15) is an ordinary one-based STRING field indexing directly
// into the same Installable-Languages string list the trailing strings
// occupy (indices 1..Installable Languages) - it is not an anchor the
// language list starts "after." Implemented per the spec's own example,
// not per the task document's reproduction.

namespace smbios {

struct DMIHeader;
// SMBiosVersion (smbios.hpp) is included for real, not just forward-declared:
// this class stores a copy of it (version_) for the Flags field's
// version-gated abbreviated-format bit, so its full definition is needed
// here.

/// @brief The Firmware Language Information structure (7.14) - the number
/// of installable languages, a 2.1+ Flags byte (bit 0 selects between the
/// abbreviated and long trailing-language-string format), 15 reserved
/// bytes, and the Current Language string index, followed by the trailing
/// list of language description strings themselves (Installable Languages
/// entries, at DMI string indices 1..Installable Languages). Current
/// Language is simply a one-based STRING field indexing directly into that
/// same list - see the header comment's "Discrepancy found" note for why
/// this differs from the task document's own description.
///
/// Scope note: per this story's own constraint, the abbreviated ("enUS")
/// vs. long ("en|US|iso8859-1") trailing-string sub-format that Flags bit 0
/// selects is NOT decomposed into separate fields here - get_language_at()
/// and get_current_language_string() both return the raw DMI string
/// unmodified either way. Labelling/parsing that sub-format for display is
/// presentation, left to the CLI, matching this project's existing
/// precedent of not inventing a fixed binary sub-layout the spec itself
/// does not define.
class FirmwareLanguageInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief Flags field (offset 0x05, 2.1+), see 7.14's Table 58. Bits 7:1
    // are reserved by the spec (set to 0) and are not given names here.
    enum Flags : uint8_t {
        AbbreviatedFormat = 0x01
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in Firmware Language Information SMBIOS entry depending on version
    /// and size.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    FirmwareLanguageInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~FirmwareLanguageInformationEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset
    /// Number of installable-language description strings that follow
    /// Current Language in the trailing DMI string list; 0 if
    /// get_entry_size() < 0x05
    uint8_t get_installable_language_count() const;

    /// @brief 0x05 offset (2.1+). Raw Flags byte; see Flags enum. Reads 0
    /// (not "absent") when either get_entry_size() < 0x06 or the table's
    /// own SMBiosVersion predates 2.1 - this field did not exist before
    /// 2.1, so pre-2.1 tables never carry a meaningful value here even if
    /// get_entry_size() happens to be large enough.
    uint8_t get_flags() const;

    /// @brief Bit 0 of get_flags() (2.1+): true if the Current Language and
    /// trailing language strings use the abbreviated two-character-code
    /// format, false if they use the long "ISO 639-1 | ISO 3166-1-alpha-2 |
    /// Encoding" format. Pre-2.1, this field does not exist - this accessor
    /// reports false (not garbage) in that case, mirroring get_flags()'s own
    /// 0 default for the same reason.
    bool is_abbreviated_format() const;

    /// @brief 0x15 offset
    /// String number (one-based) of the currently installed language; 0
    /// (dmi_string()'s own "Not Specified" index) if get_entry_size() <
    /// 0x16
    uint8_t get_current_language_index() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Current Language DMI string (see get_current_language_index()),
    /// raw and unmodified regardless of is_abbreviated_format() - see class
    /// doc comment's scope note.
    std::string get_current_language_string() const;

    /// @brief One of the trailing installable-language description strings.
    /// @param index 0-based across the Installable Languages entries, which
    /// occupy DMI string indices `1..get_installable_language_count()`
    /// (maps to `dmi_string(index + 1)`) - per 7.14's own Example 1/Example
    /// 2 (see class doc comment's "Discrepancy found" note), NOT offset by
    /// Current Language's own index. This accessor's own index parameter is
    /// 0-based (not the 1-based dmi_string() convention
    /// OemStringsEntry/SystemConfigurationOptionsEntry mirror directly) for
    /// symmetry with those two classes' own 0-based iteration idiom at call
    /// sites, while the underlying mapping is a plain +1.
    /// @return the language string at `index`, raw and unmodified regardless
    /// of is_abbreviated_format() - see class doc comment's scope note; "Not
    /// Specified" if `index >= get_installable_language_count()`.
    std::string get_language_at(size_t index) const;

private:

    /// Copy of the SMBIOS version, needed only for the Flags field's
    /// version-gated availability (every other field in this structure is
    /// distinguished by header.length alone).
    SMBiosVersion version_;
};

} // namespace smbios
