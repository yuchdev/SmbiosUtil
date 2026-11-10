#include <libsmbios/formatters/json_formatter.hpp>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/field_value.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
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
/// table).
std::string json_value(const FieldValue& value)
{
    return std::visit([](const auto& alternative) -> std::string {
        using T = std::decay_t<decltype(alternative)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return json_string(alternative);
        }
        else if constexpr (std::is_same_v<T, uint64_t>) {
            // Integer formatting: exact decimal digits, locale-independent,
            // never through a double and never in exponent form.
            return std::to_string(alternative);
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return alternative ? "true" : "false";
        }
        else {
            static_assert(std::is_same_v<T, std::vector<uint8_t>>, "unhandled FieldValue alternative");
            return "\"" + hex_encode(alternative) + "\"";
        }
    }, value);
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
