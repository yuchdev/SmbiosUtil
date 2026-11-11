#include <cstdint>
#include <string>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/formatters/text_formatter.hpp>
#include <libsmbios/tables/bios_information_entry.hpp>
#include <libsmbios/tables/inactive_entry.hpp>
#include <libsmbios/tables/processor_information_entry.hpp>
#include <libsmbios/tables/system_slots_entry.hpp>
#include <libsmbios/tables/temperature_probe_entry.hpp>

#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using namespace smbios::test;

// Unit tests (milestone 0009, Story 02.0, Task 02)
//
// Type: Unit - deterministic, no hardware/privileges.
// Scenario: TextFormatter (`text`) is byte-identical to
// render_to_description() (R3) - for a representative synthetic entry from
// each Story 01.0 batch (platform/firmware, processor/memory,
// slots/devices/power, probes/management), for InactiveEntry, and for
// hostile firmware strings (invalid UTF-8, ANSI escapes), which `text`
// deliberately does NOT sanitise (known CWE-150 gap, post-0009 follow-up).
// Also: `text` resolves through the registry, and keeps the empty framing
// hooks (the CLI owns the text preamble).
// Task 03 adds the text legs of security cases 2/4/5 and 9 (hostile
// valid-UTF-8 vendors pass through; a framed hostile document is the plain
// concatenation of render_to_description()).
// Boundaries: one well-formed structure per entry, formatted area filled
// with 0x02 at SMBIOS 3.9 (same shape as get_fields_coverage_test.cpp).
// On failure first check: libsmbios/src/text_formatter.cpp (must return
// render_to_description() unchanged), and that this TU still constructs
// TextFormatter so its REGISTER_OUTPUT_FORMATTER is linked in
// (smbios_unit_test does not force-load libsmbios).

BOOST_AUTO_TEST_SUITE(TextFormatterUnitTests)

namespace {

/// @brief Formatted-area length used for the per-batch representatives.
constexpr size_t kFormattedAreaLength = 0x60;

} // namespace

/// Normal case: `text` is registered, resolves to a TextFormatter and is
/// named "text". Constructing TextFormatter here links its registration.
BOOST_AUTO_TEST_CASE(TextFormatterResolvesThroughRegistryTestCase)
{
    TextFormatter direct;
    BOOST_CHECK_EQUAL(direct.format_name(), "text");

    const IOutputFormatter* formatter = get_output_formatter("text");
    BOOST_REQUIRE(formatter != nullptr);
    BOOST_CHECK(dynamic_cast<const TextFormatter*>(formatter) != nullptr);
    BOOST_CHECK_EQUAL(formatter->format_name(), "text");
}

/// Edge case: `text` adds no document framing; the CLI keeps its own
/// preamble, Header ID lines and trailing newline (R3).
BOOST_AUTO_TEST_CASE(TextFormatterHasNoFramingTestCase)
{
    TextFormatter formatter;
    BOOST_CHECK_EQUAL(formatter.begin_document(), "");
    BOOST_CHECK_EQUAL(formatter.entry_separator(), "");
    BOOST_CHECK_EQUAL(formatter.end_document(), "");
}

/// Normal case (R3): byte-identical to render_to_description() for one
/// representative of each Story 01.0 batch plus InactiveEntry, each built
/// through the entry factory from a synthetic table.
BOOST_AUTO_TEST_CASE(TextFormatterMatchesRenderToDescriptionPerBatchTestCase)
{
    struct Representative {
        uint8_t type;
        const char* name;
        bool (*is_expected_class)(const AbstractSMBiosEntry*);
    };
    const std::vector<Representative> representatives{
        {static_cast<uint8_t>(SMBios::BIOSInformation), "BiosInformationEntry (batch 1)",
         [](const AbstractSMBiosEntry* e) { return dynamic_cast<const BiosInformationEntry*>(e) != nullptr; }},
        {static_cast<uint8_t>(SMBios::ProcessorInformation), "ProcessorInformationEntry (batch 2)",
         [](const AbstractSMBiosEntry* e) { return dynamic_cast<const ProcessorInformationEntry*>(e) != nullptr; }},
        {static_cast<uint8_t>(SMBios::SystemSlots), "SystemSlotsEntry (batch 3)",
         [](const AbstractSMBiosEntry* e) { return dynamic_cast<const SystemSlotsEntry*>(e) != nullptr; }},
        {static_cast<uint8_t>(SMBios::TemperatureProbe), "TemperatureProbeEntry (batch 4)",
         [](const AbstractSMBiosEntry* e) { return dynamic_cast<const TemperatureProbeEntry*>(e) != nullptr; }},
        {static_cast<uint8_t>(SMBios::Inactive), "InactiveEntry",
         [](const AbstractSMBiosEntry* e) { return dynamic_cast<const InactiveEntry*>(e) != nullptr; }},
    };

    TextFormatter formatter;
    for (const auto& representative : representatives) {
        BOOST_TEST_CONTEXT(representative.name) {
            SyntheticEntry entry(representative.type, std::vector<uint8_t>(kFormattedAreaLength, 0x02),
                                 {"String One", "String Two"});
            BOOST_REQUIRE(entry.get() != nullptr);
            BOOST_CHECK(representative.is_expected_class(entry.get()));
            const std::string expected = (*entry).render_to_description();
            BOOST_CHECK(!expected.empty());
            BOOST_CHECK(formatter.render(*entry) == expected);
        }
    }
}

/// Security case (Task 03 case 1, text leg): invalid-UTF-8 vendor strings
/// reach `text` unchanged - byte-identical to render_to_description(), no
/// U+FFFD substitution (R3 forbids changing text output).
BOOST_AUTO_TEST_CASE(TextFormatterPassesInvalidUtf8ThroughTestCase)
{
    const IOutputFormatter* formatter = get_output_formatter("text");
    BOOST_REQUIRE(formatter != nullptr);
    for (const auto& c : invalid_utf8_cases()) {
        BOOST_TEST_CONTEXT("vendor size " << c.first.size()) {
            auto entry = make_bios_entry(c.first);
            BOOST_REQUIRE(entry->get() != nullptr);
            const std::string out = formatter->render(**entry);
            BOOST_CHECK(out == (**entry).render_to_description());
            BOOST_CHECK(out.find("Vendor: " + c.first + "\n") != std::string::npos);
        }
    }
}

/// Security case (Task 03 case 3, text leg): an ANSI escape sequence passes
/// through `text` byte-identically. This documents the known, accepted
/// CWE-150 gap (post-0009 follow-up) rather than hiding it - when the
/// follow-up lands, this expectation changes together with
/// render_to_description().
BOOST_AUTO_TEST_CASE(TextFormatterPassesAnsiEscapeThroughKnownGapTestCase)
{
    const std::string ansi = "\x1b[31mRED\x1b[0m";
    auto entry = make_bios_entry(ansi);
    BOOST_REQUIRE(entry->get() != nullptr);
    const std::string out = TextFormatter().render(**entry);
    BOOST_CHECK(out == (**entry).render_to_description());
    BOOST_CHECK(out.find("Vendor: " + ansi + "\n") != std::string::npos);
}

/// Security cases (Task 03 cases 2, 4, 5, text leg): every C0 control plus
/// DEL, bidi overrides with U+2028/U+2029, and U+FFFE/U+FFFF as a BIOS
/// vendor reach `text` byte-identically to render_to_description() - `text`
/// sanitises nothing (R3; same known CWE-150 gap as the ANSI case above).
BOOST_AUTO_TEST_CASE(TextFormatterPassesHostileValidUtf8ThroughTestCase)
{
    const IOutputFormatter* formatter = get_output_formatter("text");
    BOOST_REQUIRE(formatter != nullptr);
    for (const auto& v : hostile_valid_utf8_values()) {
        BOOST_TEST_CONTEXT(v.first) {
            auto entry = make_bios_entry(v.second);
            BOOST_REQUIRE(entry->get() != nullptr);
            const std::string out = formatter->render(**entry);
            BOOST_CHECK(out == (**entry).render_to_description());
            BOOST_CHECK(out.find("Vendor: " + v.second + "\n") != std::string::npos);
        }
    }
}

/// Edge case (Task 03 case 9, text leg): framing the hostile multi-entry
/// corpus through the registry yields exactly the concatenation of every
/// entry's render_to_description() - no framing, separators or rewriting.
BOOST_AUTO_TEST_CASE(TextFormatterRegistryDocumentIsConcatenationTestCase)
{
    const IOutputFormatter* formatter = get_output_formatter("text");
    BOOST_REQUIRE(formatter != nullptr);
    const HostileDocumentCorpus corpus;
    std::string expected;
    for (const AbstractSMBiosEntry* entry : corpus.entries()) {
        expected += entry->render_to_description();
    }
    BOOST_CHECK(render_document(*formatter, corpus.entries()) == expected);
    BOOST_CHECK_EQUAL(render_document(*formatter, {}), "");
}

/// Edge case: a test double's description is returned verbatim - `text`
/// never consults get_fields().
BOOST_AUTO_TEST_CASE(TextFormatterIgnoresFieldsTestCase)
{
    FieldsTestEntry entry("Double", {NamedField{"Ignored", std::string("value")}});
    BOOST_CHECK_EQUAL(TextFormatter().render(entry), "Header type: Double\n");
}

BOOST_AUTO_TEST_SUITE_END()
