#pragma once
#include <string>

#include <libsmbios/output_formatter.hpp>

namespace smbios {

/// @brief The `json` output format: a hand-rolled RFC 8259 writer over an
/// entry's structured fields (milestone 0009 R3/R4; hand-rolled rather than
/// Boost.JSON/nlohmann, ratified 2026-11-10).
///
/// Reads only AbstractSMBiosEntry::get_type() and
/// AbstractSMBiosEntry::get_fields() - never a concrete entry getter (R4) -
/// and renders exactly what get_fields() returns, in its order, without
/// clamping, dropping, deduplicating or reinterpreting any field.
///
/// Complete document (begin_document(), each render() with
/// entry_separator() between, then end_document()) is one array with one
/// object per entry:
/// @code{.json}
/// [
///   {
///     "type": "BIOS Information",
///     "fields": {
///       "Vendor": "Acme \"Firmware\"",
///       "ROM Size": 65536,
///       "ISA is supported": true,
///       "Processor ID": "a1060300ffebfbbf"
///     }
///   },
///   {
///     "type": "Inactive",
///     "fields": {}
///   }
/// ]
/// @endcode
/// A document with zero entries is `[`, an empty line, `]` - still valid
/// JSON (an empty array).
///
/// Value encoding, per FieldValue alternative:
/// - `std::string` -> a JSON string, body escaped by json_escape();
/// - `uint64_t` -> a bare JSON number written as its exact decimal digits
///   (e.g. `18446744073709551615`), never via a `double` and never in
///   exponent form, so nothing is lost on the wire;
/// - `bool` -> bare `true` or `false`;
/// - `std::vector<uint8_t>` -> a JSON string holding hex_encode() of the
///   bytes (lowercase hex, no separators; `""` for an empty vector).
///
/// @warning Integer precision: RFC 8259 §6 permits the exact-digit numbers
/// above, and arbitrary-precision consumers (Python `json`, Go
/// `json.Number`, jq 1.7+) read them exactly. JavaScript `JSON.parse` and
/// any other IEEE-754-double consumer silently round integers above 2^53
/// (9007199254740992) - e.g. 64-bit memory-error addresses and extended
/// sizes. Whether to string-encode such values is an open question; today
/// they are always bare numbers.
///
/// Object keys are the field names. Field names are unique within one
/// entry (contract C1, enforced by the get_fields() coverage tests), so no
/// object carries a duplicate key; the writer does not re-check this.
///
/// Security properties (threat model INFO-6):
/// - every string value, the `type` value and every key pass through
///   json_escape(), so the document is valid JSON and valid UTF-8 for any
///   firmware bytes (invalid UTF-8 becomes U+FFFD; no raw C0 byte, and so
///   no raw ESC, is emitted; U+2028/U+2029 are escaped for JavaScript
///   embedding);
/// - output is deterministic: no timestamps, host or locale data, and
///   always `\n` line endings. Field values are never logged.
///
/// Registered as `"json"` via REGISTER_OUTPUT_FORMATTER in
/// json_formatter.cpp. Every member is defined out of line there, so naming
/// this class links its registration into a binary that does not
/// force-load libsmbios.
class JsonFormatter : public IOutputFormatter {
public:
    /// @brief Renders one JSON object for `entry`, indented by two spaces
    /// for placement inside the document array, with no trailing newline
    /// or comma. An entry with no fields renders `"fields": {}`.
    /// @param entry the entry to render; only borrowed for the call.
    /// @return the entry's JSON object; valid JSON on its own.
    std::string render(const AbstractSMBiosEntry& entry) const override;

    /// @brief The registry key of this format.
    /// @return `"json"`.
    std::string format_name() const override;

    /// @brief Opens the document array.
    /// @return `[\n`.
    std::string begin_document() const override;

    /// @brief Separates two consecutive entry objects.
    /// @return `,\n`.
    std::string entry_separator() const override;

    /// @brief Closes the document array.
    /// @return `\n]\n`.
    std::string end_document() const override;
};

} // namespace smbios
