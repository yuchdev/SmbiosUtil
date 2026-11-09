#pragma once
#include <string>
#include <libsmbios/abstract_smbios_entry.h>

// Inactive entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Inactive' chapter
// (7.48), DSP0134 3.9.0.
//
// DSP0134 3.9.0's own 7.48 text: structure type 126 (decimal) signals that
// a structure's slot in the SMBIOS structure table is currently inactive
// and must not be interpreted by upper-level software, which should bypass
// an Inactive structure just as it would a structure type it does not
// recognize (e.g. a portable system reporting System Slot structures as
// Inactive while undocked, switching them to the real System Slot type
// once docked). Table 140 defines no fields beyond the standard Type byte
// itself - this class is therefore deliberately content-free, the same
// minimal shape as BootIntegrityServicesEntry (7.32, Story 05.0): it does
// the usual type check and nothing else.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Inactive structure (7.48) - a marker indicating that this
/// slot in the SMBIOS structure table is currently inactive and should be
/// bypassed by upper-level software, per 7.48's own guidance. DSP0134
/// 3.9.0 defines no fields for this structure beyond the standard header,
/// so this class decodes none; it exists purely so type 126 resolves
/// through SMBiosEntryFactory::create() to a concrete, correctly-typed
/// entry instead of nullptr.
class InactiveEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header; the usual type check only - no fields to
    /// read (see class doc comment).
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing (there are no strings in this structure, but every
    /// entry class takes this parameter for consistency with the base
    /// class constructor).
    InactiveEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~InactiveEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into a single string. States
    /// plainly, per 7.48's own guidance, that upper-level software should
    /// bypass/skip this structure rather than interpret it.
    virtual std::string render_to_description() const override;
};

} // namespace smbios
