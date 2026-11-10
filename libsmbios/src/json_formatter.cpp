#include <libsmbios/formatters/json_formatter.hpp>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/field_value.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// R4: this writer sees entries only through get_type()/get_fields(). It must
// never include a libsmbios/tables/*.hpp header.

using namespace smbios;

namespace {

/// @brief A complete JSON string literal (quotes included) for `text`.
std::string json_string(const std::string& text)
{
    return "\"" + json_escape(text) + "\"";
}

/// @brief The JSON value for one field (see JsonFormatter's value encoding
/// table). JSON records the alternative, so this dispatches on it, using the
/// shared field_value_to_string() encoding for the textual form.
std::string json_value(const FieldValue& value)
{
    if (const auto* text = std::get_if<std::string>(&value)) {
        return json_string(*text);
    }
    if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        // hex_encode() output: [0-9a-f] only, so no escaping is needed.
        return "\"" + field_value_to_string(value) + "\"";
    }
    // uint64_t: exact decimal digits (never via a double, never in exponent
    // form) as a bare JSON number; bool: a bare `true`/`false` literal.
    return field_value_to_string(value);
}

} // namespace

std::string JsonFormatter::render(const AbstractSMBiosEntry& entry) const
{
    const std::vector<NamedField> fields = entry.get_fields();

    std::string out = "  {\n    \"type\": " + json_string(entry.get_type()) + ",\n    \"fields\": {";
    if (fields.empty()) {
        out += "}\n  }";
        return out;
    }
    out += '\n';
    for (size_t i = 0; i < fields.size(); ++i) {
        out += "      ";
        out += json_string(fields[i].name);
        out += ": ";
        out += json_value(fields[i].value);
        out += (i + 1 < fields.size()) ? ",\n" : "\n";
    }
    out += "    }\n  }";
    return out;
}

std::string JsonFormatter::format_name() const
{
    return "json";
}

std::string JsonFormatter::begin_document() const
{
    return "[\n";
}

std::string JsonFormatter::entry_separator() const
{
    return ",\n";
}

std::string JsonFormatter::end_document() const
{
    return "\n]\n";
}

REGISTER_OUTPUT_FORMATTER(JsonFormatter)
