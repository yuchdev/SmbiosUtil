#include <libsmbios/formatters/yaml_formatter.hpp>

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

/// @brief A complete YAML double-quoted scalar (quotes included) for `text`.
std::string yaml_string(const std::string& text)
{
    return "\"" + yaml_double_quoted_escape(text) + "\"";
}

/// @brief The YAML scalar for one field (see YamlFormatter's value encoding
/// rules). YAML records the alternative, so this dispatches on it, using the
/// shared field_value_to_string() encoding for the textual form.
std::string yaml_value(const FieldValue& value)
{
    if (const auto* text = std::get_if<std::string>(&value)) {
        return yaml_string(*text);
    }
    if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        // hex_encode() output: [0-9a-f] only, so no escaping is needed, but
        // it must still be quoted (e.g. "00" or "1e10" would load as numbers).
        return "\"" + field_value_to_string(value) + "\"";
    }
    // uint64_t: exact decimal digits as a plain integer; bool: a plain
    // `true`/`false`. Both are valid YAML 1.1 and 1.2 core-schema scalars.
    return field_value_to_string(value);
}

} // namespace

std::string YamlFormatter::render(const AbstractSMBiosEntry& entry) const
{
    const std::vector<NamedField> fields = entry.get_fields();

    std::string out = "- type: " + yaml_string(entry.get_type()) + "\n  fields:";
    if (fields.empty()) {
        out += " {}";
        return out;
    }
    for (const NamedField& field : fields) {
        out += "\n    ";
        out += yaml_string(field.name);
        out += ": ";
        out += yaml_value(field.value);
    }
    return out;
}

std::string YamlFormatter::format_name() const
{
    return "yaml";
}

std::string YamlFormatter::begin_document() const
{
    return "---\n";
}

std::string YamlFormatter::entry_separator() const
{
    return "\n";
}

std::string YamlFormatter::end_document() const
{
    return "\n";
}

REGISTER_OUTPUT_FORMATTER(YamlFormatter)
