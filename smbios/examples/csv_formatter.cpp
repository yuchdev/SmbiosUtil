/// @file csv_formatter.cpp
/// @brief The `csv` output format: a complete, minimal, *external*
/// IOutputFormatter (milestone 0009 Story 03.0 Task 02, requirement R5).
///
/// This file is the proof that a new output format can be added without
/// editing libsmbios: it lives outside libsmbios's source tree, is compiled
/// straight into the `smbios` executable (smbios/examples/CMakeLists.txt,
/// never through a static archive, so its REGISTER_OUTPUT_FORMATTER
/// initializer runs without any whole-archive flag), and includes only the
/// public registry, entry-base and escaping headers below - never a concrete
/// libsmbios/tables/*.hpp entry header. It sees every entry only through
/// AbstractSMBiosEntry::get_type() and AbstractSMBiosEntry::get_fields().
///
/// Document shape: a header row, then one row per field of every entry, in
/// get_fields() order:
/// @code
/// type,field_name,field_value
/// BIOS Information,Vendor,Acme
/// BIOS Information,ROM Size,65536
/// BIOS Information,ISA is supported,true
/// Inactive,,
/// @endcode
/// - `std::string` values are written as-is; `uint64_t` as exact decimal
///   digits; `bool` as `true`/`false`; `std::vector<uint8_t>` as
///   hex_encode() output (lowercase hex, no separators).
/// - An entry with no fields still yields one row, with empty field_name
///   and field_value, so every entry stays visible in the output.
/// - Every row ends with LF (`\n`), not RFC 4180's CRLF: output is
///   deterministic and platform-independent like every other built-in
///   format, and common CSV readers accept LF.
/// - Every cell is first run through sanitize_utf8(), so the document is
///   valid UTF-8 whatever bytes the firmware strings hold.
/// - RFC 4180 quoting: a cell containing `"`, `,`, CR or LF is wrapped in
///   double quotes and each embedded `"` is doubled; any other cell is
///   written unquoted and unchanged.
///
/// CSV formula injection (CWE-1236, threat model
/// docs/security/2026-11-10-structured-field-model.md INFO-6): a
/// spreadsheet opening this file would evaluate a cell beginning with `=`,
/// `+`, `-`, `@`, TAB or CR as a formula, and firmware strings are
/// attacker-controllable. So any cell (after hex_encode() for bytes, and
/// before RFC 4180 quoting) starting with one of those six characters is
/// prefixed with a single quote `'`. The neutralisation is applied to every
/// cell, not only field values. Accepted trade-off: `-` is common in real
/// values (e.g. part numbers such as `-A01`), and such values gain a
/// visible leading `'` that a consumer wanting the raw value must strip.
///
/// Field values are never logged; the formatter only returns them in the
/// output the caller asked for.

#include <libsmbios/output_formatter.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace {

/// @brief Encodes one cell: UTF-8 sanitising, formula-injection
/// neutralisation, then RFC 4180 quoting (see the file comment).
/// @param raw the cell's unescaped text; arbitrary bytes.
/// @return the cell as it appears in the row.
std::string csv_cell(const std::string& raw)
{
    std::string cell = smbios::sanitize_utf8(raw);

    if (!cell.empty()) {
        const char lead = cell.front();
        if (lead == '=' || lead == '+' || lead == '-' || lead == '@' || lead == '\t' || lead == '\r') {
            cell.insert(cell.begin(), '\'');
        }
    }

    if (cell.find_first_of("\",\r\n") == std::string::npos) {
        return cell;
    }
    std::string quoted = "\"";
    for (char c : cell) {
        if (c == '"') {
            quoted += '"';
        }
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

/// @brief The cell text of one field value, before csv_cell() encoding.
/// @param value the field value.
/// @return its textual form; bytes via hex_encode().
std::string value_text(const smbios::FieldValue& value)
{
    if (const auto* text = std::get_if<std::string>(&value)) {
        return *text;
    }
    if (const auto* number = std::get_if<uint64_t>(&value)) {
        return std::to_string(*number);
    }
    if (const auto* flag = std::get_if<bool>(&value)) {
        return *flag ? "true" : "false";
    }
    return smbios::hex_encode(std::get<std::vector<uint8_t>>(value));
}

/// @brief One complete CSV row, LF-terminated.
std::string csv_row(const std::string& type, const std::string& name, const std::string& value)
{
    return csv_cell(type) + ',' + csv_cell(name) + ',' + csv_cell(value) + '\n';
}

} // namespace

namespace smbios_examples {

/// @brief The `csv` output format: `type,field_name,field_value` rows with
/// RFC 4180 quoting and CSV formula-injection neutralisation. See the file
/// comment for the full encoding rules. Registered as `"csv"`.
class CsvFormatter : public smbios::IOutputFormatter {
public:
    /// @brief Renders every field of `entry` as one LF-terminated row each
    /// (one row with empty name and value for an entry with no fields).
    /// @param entry the entry to render; only borrowed for the call.
    /// @return the entry's rows.
    std::string render(const smbios::AbstractSMBiosEntry& entry) const override
    {
        const std::string type = entry.get_type();
        const std::vector<smbios::NamedField> fields = entry.get_fields();
        if (fields.empty()) {
            return csv_row(type, std::string(), std::string());
        }
        std::string out;
        for (const smbios::NamedField& field : fields) {
            out += csv_row(type, field.name, value_text(field.value));
        }
        return out;
    }

    /// @brief The registry key of this format.
    /// @return `"csv"`.
    std::string format_name() const override { return "csv"; }

    /// @brief The fixed header row.
    /// @return `type,field_name,field_value\n`.
    std::string begin_document() const override { return "type,field_name,field_value\n"; }
};

} // namespace smbios_examples

REGISTER_OUTPUT_FORMATTER(smbios_examples::CsvFormatter)
