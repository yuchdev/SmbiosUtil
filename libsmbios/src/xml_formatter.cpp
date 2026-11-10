#include <libsmbios/formatters/xml_formatter.hpp>

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

/// @brief XML character data for one field value (see XmlFormatter's value
/// encoding table). Every string goes through xml_escape(); the other
/// alternatives produce only [0-9a-z] and need no escaping.
std::string xml_value(const FieldValue& value)
{
    return std::visit([](const auto& alternative) -> std::string {
        using T = std::decay_t<decltype(alternative)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return xml_escape(alternative);
        }
        else if constexpr (std::is_same_v<T, uint64_t>) {
            return std::to_string(alternative); // exact decimal digits
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return alternative ? "true" : "false";
        }
        else {
            static_assert(std::is_same_v<T, std::vector<uint8_t>>, "unhandled FieldValue alternative");
            return hex_encode(alternative);
        }
    }, value);
}

} // namespace

std::string XmlFormatter::render(const AbstractSMBiosEntry& entry) const
{
    const std::vector<NamedField> fields = entry.get_fields();

    // Names and the type label are library constants (C1), but they still
    // only ever land in escaped attribute *values*, never in markup names.
    std::string out = "  <structure type=\"" + xml_escape(entry.get_type()) + "\">";
    if (fields.empty()) {
        out += "</structure>\n";
        return out;
    }
    out += '\n';
    for (const NamedField& field : fields) {
        out += "    <field name=\"";
        out += xml_escape(field.name);
        out += "\">";
        out += xml_value(field.value);
        out += "</field>\n";
    }
    out += "  </structure>\n";
    return out;
}

std::string XmlFormatter::format_name() const
{
    return "xml";
}

std::string XmlFormatter::begin_document() const
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<smbios>\n";
}

std::string XmlFormatter::entry_separator() const
{
    return {};
}

std::string XmlFormatter::end_document() const
{
    return "</smbios>\n";
}

REGISTER_OUTPUT_FORMATTER(XmlFormatter)
