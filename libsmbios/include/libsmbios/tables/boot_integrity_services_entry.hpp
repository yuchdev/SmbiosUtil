#pragma once
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Boot Integrity Services (BIS) Entry Point entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Boot Integrity Services
// (BIS) Entry Point' chapter (7.32), DSP0134 3.9.0.
//
// DSP0134 3.9.0's own 7.32 text in full: "Structure type 31 (decimal) is
// reserved for use by the Boot Integrity Services (BIS). See the Boot
// Integrity Services API Specification for details." There is no field
// table (no "Table 1xx") for this structure anywhere in 3.9.0 - the type
// number is reserved as a marker/entry-point for an external API
// specification this codebase does not implement, not a structure with
// fields this project simply hasn't gotten to yet. This class is therefore
// deliberately content-free: it does the usual type check and nothing
// else. Do not add decoded fields here without first finding them defined
// in DSP0134 itself (not the external BIS API spec, which is out of scope
// for this project).
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Boot Integrity Services (BIS) Entry Point structure (7.32) -
/// a type-number marker reserved by DSP0134 for an external Boot Integrity
/// Services API this codebase does not implement. DSP0134 3.9.0 defines no
/// fields for this structure (see this header's own class doc comment), so
/// this class decodes none; it exists purely so type 31 resolves through
/// SMBiosEntryFactory::create() to a concrete, correctly-typed entry
/// instead of nullptr.
class BootIntegrityServicesEntry : public AbstractSMBiosEntry {
public:

    /// @brief Parse the header; the usual type check only - no fields to
    /// read (see class doc comment).
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing (there are no strings in this structure, but every
    /// entry class takes this parameter for consistency with the base
    /// class constructor).
    BootIntegrityServicesEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~BootIntegrityServicesEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into a single string. States
    /// plainly that this structure carries no DSP0134-defined fields and is
    /// reserved for the external Boot Integrity Services API, rather than
    /// rendering an empty-looking description that could be mistaken for a
    /// bug.
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Deliberately empty, like InactiveEntry: the text output carries no
    /// data fields beyond the header type and a fixed explanatory sentence.
    std::vector<NamedField> get_fields() const override;
};

} // namespace smbios
