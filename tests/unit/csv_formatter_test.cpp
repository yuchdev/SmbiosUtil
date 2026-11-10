#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>

#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using namespace smbios::test;

// Unit tests (milestone 0009, Story 03.0, Task 02)
//
// Type: Unit - deterministic, no hardware/privileges, no CSV library.
// Scenario: the external `csv` example formatter
// (smbios/examples/csv_formatter.cpp, compiled into this executable by
// tests/unit/CMakeLists.txt) is reachable through the registry alone
// (get_output_formatter("csv"), R5) and renders get_type()/get_fields() as
// `type,field_name,field_value` rows: every FieldValue alternative, RFC 4180
// quoting of `"`, `,`, CR and LF, CSV formula-injection neutralisation
// (CWE-1236, threat model INFO-6) for each of `=`, `+`, `-`, `@`, TAB and
// CR, invalid UTF-8 replaced by U+FFFD, empty-field entries, and document
// framing for zero, one and many entries.
// Boundaries: UINT64_MAX / 0, an empty and a two-byte vector, an empty
// string, a lead character that is only dangerous at position 0.
// On failure first check: a null get_output_formatter("csv") means
// csv_formatter.cpp fell out of SOURCES in tests/unit/CMakeLists.txt; else
// csv_cell() (neutralise, then quote - in that order) in
// smbios/examples/csv_formatter.cpp.

namespace {

/// @brief The registered `csv` formatter; fails the test if missing.
const IOutputFormatter& csv()
{
    const IOutputFormatter* formatter = get_output_formatter("csv");
    BOOST_REQUIRE_MESSAGE(formatter != nullptr, "csv formatter is not registered");
    return *formatter;
}

/// @brief render() of an entry of type "T" with one field "F" = `value`.
std::string render_single(FieldValue value)
{
    FieldsTestEntry entry("T", {NamedField{"F", std::move(value)}});
    return csv().render(entry);
}

} // namespace

BOOST_AUTO_TEST_SUITE(CsvFormatterUnitTests)

/// Normal case: the example registers under "csv" and appears in the
/// sorted name list, without this test naming the formatter class.
BOOST_AUTO_TEST_CASE(CsvIsRegisteredThroughPublicRegistryTestCase)
{
    BOOST_CHECK_EQUAL(csv().format_name(), "csv");
    const std::vector<std::string> names = registered_output_formatter_names();
    BOOST_CHECK(std::find(names.begin(), names.end(), "csv") != names.end());
}

/// Normal case: one row per field, in get_fields() order, each FieldValue
/// alternative in its textual form (bytes via hex_encode()).
BOOST_AUTO_TEST_CASE(RendersEveryValueAlternativeTestCase)
{
    FieldsTestEntry entry("BIOS Information", {
        NamedField{"Vendor", std::string("Acme")},
        NamedField{"ROM Size", uint64_t{65536}},
        NamedField{"Max", std::numeric_limits<uint64_t>::max()},
        NamedField{"Zero", uint64_t{0}},
        NamedField{"ISA is supported", true},
        NamedField{"PCI is supported", false},
        NamedField{"Processor ID", std::vector<uint8_t>{0x00, 0xAB}},
        NamedField{"Empty Bytes", std::vector<uint8_t>{}},
    });

    BOOST_CHECK_EQUAL(csv().render(entry),
        "BIOS Information,Vendor,Acme\n"
        "BIOS Information,ROM Size,65536\n"
        "BIOS Information,Max,18446744073709551615\n"
        "BIOS Information,Zero,0\n"
        "BIOS Information,ISA is supported,true\n"
        "BIOS Information,PCI is supported,false\n"
        "BIOS Information,Processor ID,00ab\n"
        "BIOS Information,Empty Bytes,\n");
}

/// Normal case: a benign value (including an inner `=`/`-` that is not
/// leading, and an empty string) passes through unchanged and unquoted.
BOOST_AUTO_TEST_CASE(BenignValueIsUnchangedTestCase)
{
    BOOST_CHECK_EQUAL(render_single(std::string("Version 1.0 a=b-c@d")), "T,F,Version 1.0 a=b-c@d\n");
    BOOST_CHECK_EQUAL(render_single(std::string()), "T,F,\n");
}

/// Invalid-input case (CWE-1236): each of the six formula-introducing lead
/// characters gains a leading single quote. A TAB-led cell then needs no
/// quoting; a CR-led cell is also RFC 4180 quoted, because it contains CR.
BOOST_AUTO_TEST_CASE(FormulaInjectionLeadsAreNeutralisedTestCase)
{
    BOOST_CHECK_EQUAL(render_single(std::string("=1+2")), "T,F,'=1+2\n");
    BOOST_CHECK_EQUAL(render_single(std::string("+1")), "T,F,'+1\n");
    BOOST_CHECK_EQUAL(render_single(std::string("-A01")), "T,F,'-A01\n");
    BOOST_CHECK_EQUAL(render_single(std::string("@SUM(A1)")), "T,F,'@SUM(A1)\n");
    BOOST_CHECK_EQUAL(render_single(std::string("\tx")), "T,F,'\tx\n");
    BOOST_CHECK_EQUAL(render_single(std::string("\rx")), "T,F,\"'\rx\"\n");
}

/// Invalid-input case: neutralisation runs before quoting, so a formula
/// value containing a quote and comma is both prefixed and RFC 4180 quoted.
BOOST_AUTO_TEST_CASE(FormulaPrefixIsInsideQuotesTestCase)
{
    BOOST_CHECK_EQUAL(render_single(std::string("=HYPERLINK(\"x\",\"y\")")),
                      "T,F,\"'=HYPERLINK(\"\"x\"\",\"\"y\"\")\"\n");
}

/// Edge case: embedded `"`, `,` and newline force RFC 4180 quoting and
/// quote doubling.
BOOST_AUTO_TEST_CASE(Rfc4180QuotingTestCase)
{
    BOOST_CHECK_EQUAL(render_single(std::string("say \"hi\"")), "T,F,\"say \"\"hi\"\"\"\n");
    BOOST_CHECK_EQUAL(render_single(std::string("a,b")), "T,F,\"a,b\"\n");
    BOOST_CHECK_EQUAL(render_single(std::string("line1\nline2")), "T,F,\"line1\nline2\"\n");
    BOOST_CHECK_EQUAL(render_single(std::string("a\"b,c\nd")), "T,F,\"a\"\"b,c\nd\"\n");
}

/// Edge case: the type and field-name cells get the same quoting and
/// neutralisation as values.
BOOST_AUTO_TEST_CASE(TypeAndNameCellsAreEncodedTestCase)
{
    FieldsTestEntry entry("Type, \"X\"", {NamedField{"=Name", std::string("v")}});
    BOOST_CHECK_EQUAL(csv().render(entry), "\"Type, \"\"X\"\"\",'=Name,v\n");
}

/// Invalid-input case: invalid UTF-8 in a value becomes U+FFFD, so the
/// document is valid UTF-8.
BOOST_AUTO_TEST_CASE(InvalidUtf8IsReplacedTestCase)
{
    const std::string rendered = render_single(std::string("Caf\xE9"));
    BOOST_CHECK_EQUAL(rendered, "T,F,Caf" + kReplacement + "\n");
    BOOST_CHECK(is_valid_utf8(rendered));
}

/// Edge case: an entry with no fields still yields one row with empty name
/// and value.
BOOST_AUTO_TEST_CASE(EntryWithoutFieldsYieldsOneRowTestCase)
{
    FieldsTestEntry entry("Inactive", {});
    BOOST_CHECK_EQUAL(csv().render(entry), "Inactive,,\n");
}

/// Normal case: framing - the header row once, rows of every entry, null
/// entries skipped, for zero, one and many entries.
BOOST_AUTO_TEST_CASE(DocumentFramingTestCase)
{
    const IOutputFormatter& formatter = csv();
    const std::string header = "type,field_name,field_value\n";

    std::vector<std::unique_ptr<AbstractSMBiosEntry>> none;
    BOOST_CHECK_EQUAL(render_document(formatter, none.begin(), none.end()), header);

    std::vector<std::unique_ptr<AbstractSMBiosEntry>> many;
    many.push_back(std::make_unique<FieldsTestEntry>("A", std::vector<NamedField>{NamedField{"n", uint64_t{1}}}));
    many.push_back(nullptr);
    many.push_back(std::make_unique<FieldsTestEntry>("B", std::vector<NamedField>{}));
    BOOST_CHECK_EQUAL(render_document(formatter, many.begin(), many.end()), header + "A,n,1\nB,,\n");
}

BOOST_AUTO_TEST_SUITE_END()
