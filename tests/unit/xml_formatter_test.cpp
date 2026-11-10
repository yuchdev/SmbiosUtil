#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>
#include <libsmbios/formatters/xml_formatter.hpp>

#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using namespace smbios::test;

// Unit tests (milestone 0009, Story 02.0, Task 02)
//
// Type: Unit - deterministic, no hardware/privileges, no XML library.
// Scenario: XmlFormatter (`xml`) renders get_type()/get_fields() only (R4)
// as `<structure type=".."><field name="..">value</field>...</structure>`
// inside an `<smbios>` root: exact output for every FieldValue alternative,
// escaping of `& < > " '` in values, names and the type label, invalid
// UTF-8 / XML-forbidden code points replaced by U+FFFD, empty-field
// entries, document framing for zero, one and many entries.
// Boundaries: UINT64_MAX / 0 / 2^53+1, a 0-byte and a 255-byte vector,
// every C0 control, U+FFFE/U+FFFF, bidi and U+2028/U+2029.
// Well-formedness is asserted by exact expected text plus
// is_well_formed_xml() (structured_output_validators.hpp, self-tested in
// structured_output_validators_test.cpp); the real-parser round trip is
// Task 03's dev-only Python check. Task 03 adds the whole-document case 9
// (zero/one/hostile multi-entry, through the registry) and the LOW-3
// placeholder-record pin (case 11).
// On failure first check: libsmbios/src/xml_formatter.cpp's xml_value()
// visitor and render() layout; for escaping, formatter_escaping.cpp; for a
// null registry lookup, that this TU still constructs XmlFormatter.

BOOST_AUTO_TEST_SUITE(XmlFormatterUnitTests)

namespace {

/// @brief A FieldsTestEntry holding one field.
FieldsTestEntry single_field(FieldValue value)
{
    return FieldsTestEntry("T", {NamedField{"F", std::move(value)}});
}

/// @brief The exact render() of single_field() holding text `body`.
std::string single_field_xml(const std::string& body)
{
    return "  <structure type=\"T\">\n    <field name=\"F\">" + body + "</field>\n  </structure>\n";
}

} // namespace

/// Normal case: `xml` is registered, resolves to an XmlFormatter and is
/// named "xml". Constructing XmlFormatter here links its registration.
BOOST_AUTO_TEST_CASE(XmlFormatterResolvesThroughRegistryTestCase)
{
    XmlFormatter direct;
    BOOST_CHECK_EQUAL(direct.format_name(), "xml");

    const IOutputFormatter* formatter = get_output_formatter("xml");
    BOOST_REQUIRE(formatter != nullptr);
    BOOST_CHECK(dynamic_cast<const XmlFormatter*>(formatter) != nullptr);
}

/// Normal case: exact output for every FieldValue alternative, in
/// get_fields() order, with escaping in the type label, a name and a value.
BOOST_AUTO_TEST_CASE(XmlFormatterRendersEveryAlternativeTestCase)
{
    FieldsTestEntry entry("Type & <Label>", {
        NamedField{"Vendor", std::string("Acme & <Sons> \"q\" 'a'")},
        NamedField{"Empty", std::string()},
        NamedField{"ROM Size", uint64_t{65536}},
        NamedField{"Max", std::numeric_limits<uint64_t>::max()},
        NamedField{"Zero", uint64_t{0}},
        NamedField{"2^53+1", uint64_t{9007199254740993ull}},
        NamedField{"ISA is supported", true},
        NamedField{"Off", false},
        NamedField{"Processor ID", std::vector<uint8_t>{0xA1, 0x06, 0x03, 0x00, 0xFF}},
        NamedField{"No Bytes", std::vector<uint8_t>{}},
        NamedField{"Name \"<&>'", std::string("x")},
    });
    const std::string expected =
        "  <structure type=\"Type &amp; &lt;Label&gt;\">\n"
        "    <field name=\"Vendor\">Acme &amp; &lt;Sons&gt; &quot;q&quot; &apos;a&apos;</field>\n"
        "    <field name=\"Empty\"></field>\n"
        "    <field name=\"ROM Size\">65536</field>\n"
        "    <field name=\"Max\">18446744073709551615</field>\n"
        "    <field name=\"Zero\">0</field>\n"
        "    <field name=\"2^53+1\">9007199254740993</field>\n"
        "    <field name=\"ISA is supported\">true</field>\n"
        "    <field name=\"Off\">false</field>\n"
        "    <field name=\"Processor ID\">a1060300ff</field>\n"
        "    <field name=\"No Bytes\"></field>\n"
        "    <field name=\"Name &quot;&lt;&amp;&gt;&apos;\">x</field>\n"
        "  </structure>\n";
    const std::string out = XmlFormatter().render(entry);
    BOOST_CHECK_EQUAL(out, expected);
    BOOST_CHECK(is_well_formed_xml(out));
}

/// Edge case (Task 03 case 7): an entry with no fields - a real synthetic
/// InactiveEntry - renders an empty <structure> element.
BOOST_AUTO_TEST_CASE(XmlFormatterRendersEmptyFieldsTestCase)
{
    SyntheticEntry inactive(static_cast<uint8_t>(SMBios::Inactive), {}, {});
    BOOST_REQUIRE(inactive.get() != nullptr);
    BOOST_REQUIRE((*inactive).get_fields().empty());
    const std::string out = XmlFormatter().render(*inactive);
    BOOST_CHECK_EQUAL(out, "  <structure type=\"Inactive\"></structure>\n");
    BOOST_CHECK(is_well_formed_xml(out));
}

/// Edge case: exact framing for zero, one and two entries; each document
/// is structurally valid.
BOOST_AUTO_TEST_CASE(XmlFormatterDocumentFramingTestCase)
{
    XmlFormatter formatter;
    BOOST_CHECK_EQUAL(formatter.begin_document(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<smbios>\n");
    BOOST_CHECK_EQUAL(formatter.entry_separator(), "");
    BOOST_CHECK_EQUAL(formatter.end_document(), "</smbios>\n");

    FieldsTestEntry a("A", {NamedField{"n", uint64_t{1}}});
    FieldsTestEntry b("B", {});

    const std::string zero = render_document(formatter, {});
    BOOST_CHECK_EQUAL(zero, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<smbios>\n</smbios>\n");
    BOOST_CHECK(is_well_formed_xml(zero));

    const std::string two = render_document(formatter, {&a, &b});
    BOOST_CHECK_EQUAL(two,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<smbios>\n"
        "  <structure type=\"A\">\n"
        "    <field name=\"n\">1</field>\n"
        "  </structure>\n"
        "  <structure type=\"B\"></structure>\n"
        "</smbios>\n");
    BOOST_CHECK(is_well_formed_xml(two));
    BOOST_CHECK(is_well_formed_xml(render_document(formatter, {&a})));
    BOOST_CHECK(two.find('\r') == std::string::npos); // always \n, never CRLF
}

/// Security case (Task 03 case 1): a synthetic BIOS Information entry with
/// each invalid-UTF-8 vendor string renders that vendor with one U+FFFD per
/// maximal invalid subpart, and the whole document stays valid.
BOOST_AUTO_TEST_CASE(XmlFormatterReplacesInvalidUtf8VendorTestCase)
{
    XmlFormatter formatter;
    for (const auto& c : invalid_utf8_cases()) {
        BOOST_TEST_CONTEXT("vendor size " << c.first.size()) {
            auto entry = make_bios_entry(c.first);
            BOOST_REQUIRE(entry->get() != nullptr);
            const std::string out = formatter.render(**entry);
            BOOST_CHECK(out.find("<structure type=\"BIOS Information\">") != std::string::npos);
            BOOST_CHECK(out.find("<field name=\"Vendor\">" + c.second + "</field>") != std::string::npos);
            BOOST_CHECK(is_well_formed_xml(render_document(formatter, {entry->get()})));
        }
    }
}

/// Security cases (Task 03 cases 2, 3, 5): TAB/LF/CR survive; every other
/// C0 byte, ESC (so ANSI sequences) and U+FFFE/U+FFFF become U+FFFD; no
/// character reference to a forbidden code point appears; DEL passes.
BOOST_AUTO_TEST_CASE(XmlFormatterReplacesForbiddenCodePointsTestCase)
{
    std::string expected_controls;
    for (int b = 0x01; b <= 0x1F; ++b) {
        expected_controls += (b == 0x09 || b == 0x0A || b == 0x0D) ? std::string(1, static_cast<char>(b)) : kReplacement;
    }
    expected_controls += '\x7F';

    const std::string controls = XmlFormatter().render(single_field(std::string(all_c0_controls_and_del())));
    BOOST_CHECK_EQUAL(controls, single_field_xml(expected_controls));
    BOOST_CHECK(controls.find("&#") == std::string::npos);
    BOOST_CHECK(is_well_formed_xml(controls));

    const std::string ansi = XmlFormatter().render(single_field(std::string("\x1b[31mRED\x1b[0m")));
    BOOST_CHECK_EQUAL(ansi, single_field_xml(kReplacement + "[31mRED" + kReplacement + "[0m"));
    BOOST_CHECK(!contains_byte_in_range(ansi, 0x1B, 0x1B));

    const std::string nonchars = XmlFormatter().render(single_field(std::string("\xEF\xBF\xBE\xEF\xBF\xBF")));
    BOOST_CHECK_EQUAL(nonchars, single_field_xml(kReplacement + kReplacement));
    BOOST_CHECK(is_well_formed_xml(nonchars));
}

/// Security case (Task 03 case 4): bidi overrides and U+2028/U+2029 pass
/// through as raw UTF-8 data and the document stays valid.
BOOST_AUTO_TEST_CASE(XmlFormatterPassesBidiThroughTestCase)
{
    const std::string bidi = "\xE2\x80\xAE" "\xE2\x81\xA6" "\xE2\x81\xA9" "\xE2\x80\xA8" "\xE2\x80\xA9";
    const std::string out = XmlFormatter().render(single_field(std::string(bidi)));
    BOOST_CHECK_EQUAL(out, single_field_xml(bidi));
    BOOST_CHECK(is_well_formed_xml(out));
}

/// Edge case (Task 03 case 8): a 255-byte vector renders as exactly 510
/// lowercase hex characters, untruncated.
BOOST_AUTO_TEST_CASE(XmlFormatterRendersMaximumByteFieldTestCase)
{
    std::vector<uint8_t> bytes;
    for (int b = 0x00; b <= 0xFE; ++b) {
        bytes.push_back(static_cast<uint8_t>(b));
    }
    const std::string out = XmlFormatter().render(single_field(bytes));
    BOOST_CHECK_EQUAL(out, single_field_xml(hex_encode(bytes)));
    BOOST_CHECK_EQUAL(hex_encode(bytes).size(), static_cast<size_t>(510));
}

/// Edge case: a multi-entry document mixing real synthetic entries (BIOS
/// with a hostile vendor, InactiveEntry) and test doubles is valid.
BOOST_AUTO_TEST_CASE(XmlFormatterMixedDocumentIsValidTestCase)
{
    auto bios = make_bios_entry("Evil\x1b]0;x\x07 & <tag> \xC0\xAF");
    SyntheticEntry inactive(static_cast<uint8_t>(SMBios::Inactive), {}, {});
    FieldsTestEntry fields("Test", {NamedField{"k", std::string("\"v\"")}, NamedField{"b", true}});
    BOOST_REQUIRE(bios->get() != nullptr);
    BOOST_REQUIRE(inactive.get() != nullptr);

    const std::string doc = render_document(XmlFormatter(), {bios->get(), inactive.get(), &fields});
    BOOST_CHECK(is_well_formed_xml(doc));
    BOOST_CHECK(doc.find("<!") == std::string::npos);
}

/// Security case (Task 03 case 9): rendered through the registry exactly as
/// the CLI frames a document, (a) zero entries, (b) one real entry and (c)
/// the hostile multi-entry corpus (one representative per Story 01.0 batch
/// with hostile DMI strings, an InactiveEntry, every case 1-5 hostile
/// vendor, the case 6/8 boundary values, an empty double) are each one
/// well-formed document; (c) has one <structure> per entry, no DOCTYPE and
/// no character reference at all.
BOOST_AUTO_TEST_CASE(XmlFormatterRegistryDocumentsAreWellFormedTestCase)
{
    XmlFormatter linked; // names the class so its registration is linked
    const IOutputFormatter* formatter = get_output_formatter("xml");
    BOOST_REQUIRE(formatter != nullptr);

    const std::string zero = render_document(*formatter, {});
    BOOST_CHECK(is_well_formed_xml(zero));

    auto bios = make_bios_entry("Plain Vendor");
    BOOST_REQUIRE(bios->get() != nullptr);
    const std::string one = render_document(*formatter, {bios->get()});
    BOOST_CHECK(is_well_formed_xml(one));
    BOOST_CHECK_EQUAL(count_occurrences(one, "<structure "), static_cast<size_t>(1));

    const HostileDocumentCorpus corpus;
    const std::string many = render_document(*formatter, corpus.entries());
    BOOST_CHECK(is_well_formed_xml(many));
    BOOST_CHECK_EQUAL(count_occurrences(many, "<structure "), corpus.entries().size());
    BOOST_CHECK(many.find("<structure type=\"Inactive\"></structure>") != std::string::npos);
    BOOST_CHECK(many.find("<field name=\"Max\">18446744073709551615</field>") != std::string::npos);
    BOOST_CHECK(many.find("<!") == std::string::npos);
    BOOST_CHECK(many.find("&#") == std::string::npos);
    BOOST_CHECK(!contains_byte_in_range(many, 0x1B, 0x1B));
}

/// Security case (Task 03 case 11, LOW-3 stance pin): a synthetic Memory
/// Channel entry declares 5 devices but its Length holds only 1 record.
/// get_fields() keeps text parity and emits all 5 (4 fabricated as 0), and
/// `xml` renders exactly as many <field> elements as get_fields() returns -
/// neither dropping nor adding. When the getter-side clamp lands, the
/// expected 13 below moves with the getter.
BOOST_AUTO_TEST_CASE(XmlFormatterRendersPlaceholderRecordsUnclampedTestCase)
{
    SyntheticEntry channel(static_cast<uint8_t>(SMBios::MemoryChannel),
                           {0x04, 0x10, 0x05, /* record 1 */ 0x07, 0x34, 0x12}, {});
    BOOST_REQUIRE(channel.get() != nullptr);
    const std::vector<NamedField> fields = (*channel).get_fields();
    BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(3 + 2 * 5)); // stance: unclamped

    const std::string out = XmlFormatter().render(*channel);
    BOOST_CHECK(is_well_formed_xml(out));
    BOOST_CHECK_EQUAL(count_occurrences(out, "<field name="), fields.size());
    BOOST_CHECK(out.find("<field name=\"Memory Device Count\">5</field>") != std::string::npos);
    BOOST_CHECK(out.find("<field name=\"Handle 1\">4660</field>") != std::string::npos); // the real record
    BOOST_CHECK(out.find("<field name=\"Load 5\">0</field>") != std::string::npos);      // fabricated
    BOOST_CHECK(out.find("<field name=\"Handle 5\">0</field>") != std::string::npos);    // ...not clamped
}

BOOST_AUTO_TEST_SUITE_END()
