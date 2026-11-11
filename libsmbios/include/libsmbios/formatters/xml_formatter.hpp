#pragma once
#include <string>

#include <libsmbios/output_formatter.hpp>

namespace smbios {

/// @brief The `xml` output format: a hand-rolled XML 1.0 writer over an
/// entry's structured fields (milestone 0009 R3/R4). No third-party XML
/// library is used.
///
/// Reads only AbstractSMBiosEntry::get_type() and
/// AbstractSMBiosEntry::get_fields() - never a concrete entry getter (R4) -
/// and renders exactly what get_fields() returns, in its order, without
/// clamping, dropping, deduplicating or reinterpreting any field.
///
/// Complete document (begin_document(), each render() with
/// entry_separator() between, then end_document()):
/// @code{.xml}
/// <?xml version="1.0" encoding="UTF-8"?>
/// <smbios>
///   <structure type="BIOS Information">
///     <field name="Vendor">Acme &amp; Sons</field>
///     <field name="ROM Size">65536</field>
///     <field name="ISA is supported">true</field>
///   </structure>
///   <structure type="Inactive"></structure>
/// </smbios>
/// @endcode
///
/// Value encoding, per FieldValue alternative:
/// - `std::string` -> xml_escape() of the text;
/// - `uint64_t` -> exact decimal digits;
/// - `bool` -> `true` or `false`;
/// - `std::vector<uint8_t>` -> hex_encode() (lowercase hex, no separators;
///   empty for an empty vector).
///
/// The alternative is not recorded in the markup, so `true` or `42` read
/// back as text; consumers needing the type should use `json`.
///
/// Security properties (threat model INFO-6):
/// - every value, the `type` attribute and every `name` attribute pass
///   through xml_escape(), so the document is well-formed and valid UTF-8
///   for any firmware bytes (invalid UTF-8 and XML-forbidden code points
///   become U+FFFD);
/// - field names and type labels only ever appear as escaped attribute
///   *values*, never as element or attribute *names* (CWE-91);
/// - no DOCTYPE, entity declaration or processing instruction is emitted
///   other than the fixed XML declaration from begin_document();
/// - output is deterministic: no timestamps, host or locale data, and
///   always `\n` line endings. Field values are never logged.
///
/// @warning Terminal display: the guarantee is document well-formedness,
/// not terminal safety. ESC and the other forbidden C0 controls are
/// replaced, but DEL and the C1 controls U+007F-U+009F (including U+009B,
/// the 8-bit CSI introducer) are legal XML 1.0 character data and pass
/// through raw, because the only way to escape them is a character
/// reference, which this writer never emits (threat model SR-1). Bidi
/// overrides pass through too. Prefer `json` for output that may be shown
/// directly in a terminal.
///
/// Registered as `"xml"` via REGISTER_OUTPUT_FORMATTER in xml_formatter.cpp.
/// Every member is defined out of line there, so naming this class links
/// its registration into a binary that does not force-load libsmbios.
class XmlFormatter : public IOutputFormatter {
public:
    /// @brief Renders one `<structure>` element for `entry`, indented for
    /// placement inside the document's root element, ending in `\n`. An
    /// entry with no fields renders as `<structure type="..."></structure>`.
    /// @param entry the entry to render; only borrowed for the call.
    /// @return the entry's `<structure>` element; well-formed on its own.
    std::string render(const AbstractSMBiosEntry& entry) const override;

    /// @brief The registry key of this format.
    /// @return `"xml"`.
    std::string format_name() const override;

    /// @brief The XML declaration and the opening root element.
    /// @return `<?xml version="1.0" encoding="UTF-8"?>\n<smbios>\n`.
    std::string begin_document() const override;

    /// @brief Nothing: each rendered `<structure>` already ends in `\n`.
    /// @return the empty string.
    std::string entry_separator() const override;

    /// @brief The closing root element.
    /// @return `</smbios>\n`.
    std::string end_document() const override;
};

} // namespace smbios
