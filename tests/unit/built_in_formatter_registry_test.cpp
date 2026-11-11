#include <algorithm>
#include <string>
#include <vector>

#include <libsmbios/output_formatter.hpp>
#include <libsmbios/formatters/json_formatter.hpp>
#include <libsmbios/formatters/text_formatter.hpp>
#include <libsmbios/formatters/xml_formatter.hpp>
#include <libsmbios/formatters/yaml_formatter.hpp>

#include <boost/test/unit_test.hpp>

using namespace smbios;

// Unit tests (milestone 0009, Story 02.0 review fixes; Story 03.0 Task 01)
//
// Type: Unit - deterministic, no hardware/privileges needed.
// Scenario: the registry inside smbios_unit_test holds at least the four
// built-in formats (`json`, `text`, `xml`, `yaml`), each resolving to its own
// class, and the name enumeration lists them.
// Boundaries: "at least" - test-local fakes from other TUs may also be
// registered, so the enumeration is checked for containment, never for an
// exact list. smbios_unit_test force-loads libsmbios since milestone 0011
// (tests/unit/CMakeLists.txt); this TU still constructs each built-in once
// (registered_built_ins_are_linked) so it keeps pulling each object file -
// and with it the REGISTER_OUTPUT_FORMATTER registration - into the binary
// even without that whole-archive link.
// On failure first check: that json/text/xml/yaml_formatter.cpp still invoke
// REGISTER_OUTPUT_FORMATTER at namespace scope, and that their members stay
// out of line (so naming the class links the registration).

namespace {

/// @brief Names every built-in formatter class so the linker keeps each
/// one's object file (and registration) in smbios_unit_test.
bool registered_built_ins_are_linked()
{
    return !JsonFormatter().format_name().empty() && !TextFormatter().format_name().empty()
        && !XmlFormatter().format_name().empty() && !YamlFormatter().format_name().empty();
}

} // namespace

BOOST_AUTO_TEST_SUITE(BuiltInFormatterRegistryUnitTests)

/// Normal case: `json`, `text`, `xml` and `yaml` are all registered and enumerated.
BOOST_AUTO_TEST_CASE(RegistryContainsBuiltInFormatsTestCase)
{
    BOOST_REQUIRE(registered_built_ins_are_linked());

    const std::vector<std::string> names = registered_output_formatter_names();
    for (const char* name : {"json", "text", "xml", "yaml"}) {
        BOOST_TEST_CONTEXT("format " << name)
        {
            BOOST_CHECK(std::find(names.begin(), names.end(), name) != names.end());
            const IOutputFormatter* formatter = get_output_formatter(name);
            BOOST_REQUIRE(formatter != nullptr);
            BOOST_CHECK_EQUAL(formatter->format_name(), name);
        }
    }
    BOOST_CHECK(dynamic_cast<const JsonFormatter*>(get_output_formatter("json")) != nullptr);
    BOOST_CHECK(dynamic_cast<const TextFormatter*>(get_output_formatter("text")) != nullptr);
    BOOST_CHECK(dynamic_cast<const XmlFormatter*>(get_output_formatter("xml")) != nullptr);
    BOOST_CHECK(dynamic_cast<const YamlFormatter*>(get_output_formatter("yaml")) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
