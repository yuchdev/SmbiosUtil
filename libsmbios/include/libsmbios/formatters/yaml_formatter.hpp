#pragma once
#include <string>

#include <libsmbios/output_formatter.hpp>

namespace smbios {

/// @brief The `yaml` output format: a hand-rolled, flat YAML 1.2 emitter
/// over an entry's structured fields (milestone 0009 Story 03.0 Task 01;
/// hand-rolled rather than yaml-cpp, ratified 2026-11-10).
///
/// Reads only AbstractSMBiosEntry::get_type() and
/// AbstractSMBiosEntry::get_fields() - never a concrete entry getter (R4) -
/// and renders exactly what get_fields() returns, in its order, without
/// clamping, dropping, deduplicating or reinterpreting any field.
///
/// Complete document (begin_document(), each render() with
/// entry_separator() between, then end_document()) is one explicit YAML
/// document holding a block sequence with one mapping per entry:
/// @code{.yaml}
/// ---
/// - type: "BIOS Information"
///   fields:
///     "Vendor": "Acme \"Firmware\""
///     "ROM Size": 65536
///     "ISA is supported": true
///     "Processor ID": "a1060300ffebfbbf"
/// - type: "Inactive"
///   fields: {}
/// @endcode
/// A document with zero entries is `---` followed by an empty line: one
/// valid YAML document whose root node is empty, which a YAML parser loads
/// as null (e.g. Python `yaml.safe_load` returns `None`), not as an empty
/// sequence. A consumer should treat a null root as "no entries".
///
/// Quoting and value encoding (ratified Story 03.0 rule):
/// - every string scalar is double-quoted - every field-name key, every
///   `std::string` value and the `type` value - with its body escaped by
///   yaml_double_quoted_escape(). So a value such as `NO`, `yes`, `1.10`,
///   `0x0A`, `~`, `null`, one containing `": "` or one starting with `-`
///   always loads back as a string, never as a bool, number or null;
/// - only the two fixed structural keys `type` and `fields` are plain;
/// - `uint64_t` -> an unquoted decimal integer written as its exact digits
///   (never via a `double`, never in exponent form);
/// - `bool` -> unquoted `true` or `false` (valid in YAML 1.1 and 1.2);
/// - `std::vector<uint8_t>` -> a double-quoted hex_encode() string
///   (lowercase hex, no separators; `""` for an empty vector);
/// - an entry with no fields renders `fields: {}` (an empty flow mapping),
///   so `fields` is always a mapping.
///
/// The emitter is flat by design: no anchors (`&`), aliases (`*`), tags
/// (`!`), `%` directives, block scalars or nesting beyond
/// `- type / fields / key: value`. A string containing a newline is escaped
/// (`\n`) inside its double quotes, never emitted as a block scalar.
///
/// Mapping keys are the field names. Field names are unique within one
/// entry (contract C1, enforced by the get_fields() coverage tests), so no
/// mapping carries a duplicate key; the writer does not re-check this.
///
/// Security properties (threat model INFO-6, SR-2):
/// - every string passes through yaml_double_quoted_escape(), so the
///   document is valid UTF-8 for any firmware bytes (invalid UTF-8 becomes
///   U+FFFD) and no code point outside YAML 1.2 `c-printable` is emitted
///   raw: C0, DEL and C1 are escaped (ESC as `\e`), U+0085/U+2028/U+2029 as
///   `\N`/`\L`/`\P`, U+FEFF/U+FFFE/U+FFFF as `\ufeff`/`\ufffe`/`\uffff`;
///   bidi overrides are printable and pass through as data;
/// - output is deterministic: no timestamps, host or locale data, and
///   always `\n` line endings. Field values are never logged.
///
/// Registered as `"yaml"` via REGISTER_OUTPUT_FORMATTER in
/// yaml_formatter.cpp. Every member is defined out of line there, so naming
/// this class links its registration into a binary that does not
/// force-load libsmbios.
class YamlFormatter : public IOutputFormatter {
public:
    /// @brief Renders one block-sequence item (`- type: ...` then
    /// `  fields: ...`) for `entry`, with no trailing newline. An entry with
    /// no fields renders `  fields: {}`.
    /// @param entry the entry to render; only borrowed for the call.
    /// @return the entry's sequence item; valid YAML on its own.
    std::string render(const AbstractSMBiosEntry& entry) const override;

    /// @brief The registry key of this format.
    /// @return `"yaml"`.
    std::string format_name() const override;

    /// @brief Opens the document with an explicit document-start marker.
    /// @return `---\n`.
    std::string begin_document() const override;

    /// @brief Separates two consecutive sequence items.
    /// @return `\n`.
    std::string entry_separator() const override;

    /// @brief Terminates the last line of the document.
    /// @return `\n`.
    std::string end_document() const override;
};

} // namespace smbios
