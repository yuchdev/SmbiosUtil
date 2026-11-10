#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>
#include <libsmbios/formatters/json_formatter.hpp>

#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using namespace smbios::test;

// Unit tests (milestone 0009, Story 02.0, Task 02)
//
// Type: Unit - deterministic, no hardware/privileges, no JSON library.
// Scenario: JsonFormatter (`json`) renders get_type()/get_fields() only
// (R4) as `{"type": .., "fields": {name: value, ...}}` objects inside one
// array: exact output for every FieldValue alternative (uint64_t as exact
// bare digits, bool bare, bytes as a hex string), escaping of `"` and `\`
// in values, keys and the type label, every C0 control escaped,
// U+2028/U+2029 escaped, invalid UTF-8 replaced by U+FFFD, empty-field
// entries, document framing for zero, one and many entries.
// Boundaries: UINT64_MAX / 0 / 2^53+1, a 0-byte and a 255-byte vector,
// every C0 control plus DEL, U+FFFE/U+FFFF, bidi overrides.
// Well-formedness is asserted by exact expected text plus
// is_well_formed_json() (structured_output_validators.hpp, self-tested in
// structured_output_validators_test.cpp); the real-parser round trip is
// Task 03's dev-only Python check. Task 03 adds the whole-document case 9
// (zero/one/hostile multi-entry, through the registry) and the LOW-3
// placeholder-record pin (case 11).
// On failure first check: libsmbios/src/json_formatter.cpp's json_value()
// visitor and render() layout; for escaping, formatter_escaping.cpp; for a
// null registry lookup, that this TU still constructs JsonFormatter.

BOOST_AUTO_TEST_SUITE(JsonFormatterUnitTests)

namespace {

/// @brief A FieldsTestEntry holding one field.
FieldsTestEntry single_field(FieldValue value)
{
    return FieldsTestEntry("T", {NamedField{"F", std::move(value)}});
}

/// @brief The exact render() of single_field() whose JSON value is `value`.
std::string single_field_json(const std::string& value)
{
    return "  {\n    \"type\": \"T\",\n    \"fields\": {\n      \"F\": " + value + "\n    }\n  }";
}

} // namespace

/// Normal case: `json` is registered, resolves to a JsonFormatter and is
/// named "json". Constructing JsonFormatter here links its registration.
BOOST_AUTO_TEST_CASE(JsonFormatterResolvesThroughRegistryTestCase)
{
    JsonFormatter direct;
    BOOST_CHECK_EQUAL(direct.format_name(), "json");

    const IOutputFormatter* formatter = get_output_formatter("json");
    BOOST_REQUIRE(formatter != nullptr);
    BOOST_CHECK(dynamic_cast<const JsonFormatter*>(formatter) != nullptr);
}

/// Normal and boundary case (Task 03 case 6): exact output for every
/// FieldValue alternative in get_fields() order; UINT64_MAX, 0 and 2^53+1
/// are bare exact digits (no quotes, exponent or rounding); escaping in the
/// type label, a key and a value.
BOOST_AUTO_TEST_CASE(JsonFormatterRendersEveryAlternativeTestCase)
{
    FieldsTestEntry entry("Type \"Q\" \\", {
        NamedField{"Vendor", std::string("Acme \"Firmware\" \\ & <x> 'a' /")},
        NamedField{"Empty", std::string()},
        NamedField{"ROM Size", uint64_t{65536}},
        NamedField{"Max", std::numeric_limits<uint64_t>::max()},
        NamedField{"Zero", uint64_t{0}},
        NamedField{"2^53+1", uint64_t{9007199254740993ull}},
        NamedField{"ISA is supported", true},
        NamedField{"Off", false},
        NamedField{"Processor ID", std::vector<uint8_t>{0xA1, 0x06, 0x03, 0x00, 0xFF}},
        NamedField{"No Bytes", std::vector<uint8_t>{}},
        NamedField{"Key \"q\"\n", std::string("x")},
    });
    const std::string expected =
        "  {\n"
        "    \"type\": \"Type \\\"Q\\\" \\\\\",\n"
        "    \"fields\": {\n"
        "      \"Vendor\": \"Acme \\\"Firmware\\\" \\\\ & <x> 'a' /\",\n"
        "      \"Empty\": \"\",\n"
        "      \"ROM Size\": 65536,\n"
        "      \"Max\": 18446744073709551615,\n"
        "      \"Zero\": 0,\n"
        "      \"2^53+1\": 9007199254740993,\n"
        "      \"ISA is supported\": true,\n"
        "      \"Off\": false,\n"
        "      \"Processor ID\": \"a1060300ff\",\n"
        "      \"No Bytes\": \"\",\n"
        "      \"Key \\\"q\\\"\\n\": \"x\"\n"
        "    }\n"
        "  }";
    const std::string out = JsonFormatter().render(entry);
    BOOST_CHECK_EQUAL(out, expected);
    BOOST_CHECK(is_well_formed_json(out));
}

/// Edge case (Task 03 case 7): an entry with no fields - a real synthetic
/// InactiveEntry - renders `"fields": {}`.
BOOST_AUTO_TEST_CASE(JsonFormatterRendersEmptyFieldsTestCase)
{
    SyntheticEntry inactive(static_cast<uint8_t>(SMBios::Inactive), {}, {});
    BOOST_REQUIRE(inactive.get() != nullptr);
    BOOST_REQUIRE((*inactive).get_fields().empty());
    const std::string out = JsonFormatter().render(*inactive);
    BOOST_CHECK_EQUAL(out, "  {\n    \"type\": \"Inactive\",\n    \"fields\": {}\n  }");
    BOOST_CHECK(is_well_formed_json(out));
}

/// Edge case: exact framing for zero, one and two entries; each document
/// is one valid JSON array.
BOOST_AUTO_TEST_CASE(JsonFormatterDocumentFramingTestCase)
{
    JsonFormatter formatter;
    BOOST_CHECK_EQUAL(formatter.begin_document(), "[\n");
    BOOST_CHECK_EQUAL(formatter.entry_separator(), ",\n");
    BOOST_CHECK_EQUAL(formatter.end_document(), "\n]\n");

    FieldsTestEntry a("A", {NamedField{"n", uint64_t{1}}});
    FieldsTestEntry b("B", {});

    const std::string zero = render_document(formatter, {});
    BOOST_CHECK_EQUAL(zero, "[\n\n]\n");
    BOOST_CHECK(is_well_formed_json(zero));

    const std::string two = render_document(formatter, {&a, &b});
    BOOST_CHECK_EQUAL(two,
        "[\n"
        "  {\n"
        "    \"type\": \"A\",\n"
        "    \"fields\": {\n"
        "      \"n\": 1\n"
        "    }\n"
        "  },\n"
        "  {\n"
        "    \"type\": \"B\",\n"
        "    \"fields\": {}\n"
        "  }\n"
        "]\n");
    BOOST_CHECK(is_well_formed_json(two));
    BOOST_CHECK(is_well_formed_json(render_document(formatter, {&a})));
    BOOST_CHECK(two.find('\r') == std::string::npos); // always \n, never CRLF
}

/// Security case (Task 03 case 1): a synthetic BIOS Information entry with
/// each invalid-UTF-8 vendor string renders that vendor with one U+FFFD per
/// maximal invalid subpart, and the whole document stays valid.
BOOST_AUTO_TEST_CASE(JsonFormatterReplacesInvalidUtf8VendorTestCase)
{
    JsonFormatter formatter;
    for (const auto& c : invalid_utf8_cases()) {
        BOOST_TEST_CONTEXT("vendor size " << c.first.size()) {
            auto entry = make_bios_entry(c.first);
            BOOST_REQUIRE(entry->get() != nullptr);
            const std::string out = formatter.render(**entry);
            BOOST_CHECK(out.find("\"type\": \"BIOS Information\"") != std::string::npos);
            BOOST_CHECK(out.find("\"Vendor\": \"" + c.second + "\",") != std::string::npos);
            BOOST_CHECK(is_well_formed_json(render_document(formatter, {entry->get()})));
        }
    }
}

/// Security cases (Task 03 cases 2, 3): every C0 control is escaped -
/// short forms where defined, lowercase \u00XX otherwise, ESC -> \u001b -
/// so no raw C0 byte (and no raw ESC of an ANSI sequence) appears; DEL
/// passes raw.
BOOST_AUTO_TEST_CASE(JsonFormatterEscapesControlCharactersTestCase)
{
    const std::string controls = JsonFormatter().render(single_field(std::string(all_c0_controls_and_del())));
    BOOST_CHECK_EQUAL(controls, single_field_json(
        "\"\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\b\\t\\n\\u000b\\f\\r\\u000e\\u000f"
        "\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017\\u0018\\u0019\\u001a\\u001b"
        "\\u001c\\u001d\\u001e\\u001f\x7F\""));
    BOOST_CHECK(is_well_formed_json(controls));

    const std::string ansi = JsonFormatter().render(single_field(std::string("\x1b[31mRED\x1b[0m")));
    BOOST_CHECK_EQUAL(ansi, single_field_json("\"\\u001b[31mRED\\u001b[0m\""));
    BOOST_CHECK(!contains_byte_in_range(ansi, 0x1B, 0x1B));
}

/// Security cases (Task 03 cases 4, 5): U+2028/U+2029 become \u2028 and
/// \u2029; bidi overrides, U+FFFE and U+FFFF pass through as raw UTF-8.
BOOST_AUTO_TEST_CASE(JsonFormatterLineSeparatorsAndNoncharactersTestCase)
{
    const std::string bidi = "\xE2\x80\xAE" "\xE2\x81\xA6" "\xE2\x81\xA9";
    const std::string out = JsonFormatter().render(
        single_field(std::string(bidi + "\xE2\x80\xA8" "\xE2\x80\xA9" "\xEF\xBF\xBE\xEF\xBF\xBF")));
    BOOST_CHECK_EQUAL(out, single_field_json("\"" + bidi + "\\u2028\\u2029" "\xEF\xBF\xBE\xEF\xBF\xBF" "\""));
    BOOST_CHECK(out.find("\xE2\x80\xA8") == std::string::npos);
    BOOST_CHECK(out.find("\xE2\x80\xA9") == std::string::npos);
    BOOST_CHECK(is_well_formed_json(out));
}

/// Edge case (Task 03 case 8): a 255-byte vector renders as exactly 510
/// lowercase hex characters in a string, untruncated.
BOOST_AUTO_TEST_CASE(JsonFormatterRendersMaximumByteFieldTestCase)
{
    std::vector<uint8_t> bytes;
    for (int b = 0x00; b <= 0xFE; ++b) {
        bytes.push_back(static_cast<uint8_t>(b));
    }
    const std::string out = JsonFormatter().render(single_field(bytes));
    BOOST_CHECK_EQUAL(out, single_field_json("\"" + hex_encode(bytes) + "\""));
    BOOST_CHECK_EQUAL(hex_encode(bytes).size(), static_cast<size_t>(510));
}

/// Edge case: a multi-entry document mixing real synthetic entries (BIOS
/// with a hostile vendor, InactiveEntry) and test doubles is valid.
BOOST_AUTO_TEST_CASE(JsonFormatterMixedDocumentIsValidTestCase)
{
    auto bios = make_bios_entry("Evil\x1b]0;x\x07 \"}],{\" \\ \xC0\xAF");
    SyntheticEntry inactive(static_cast<uint8_t>(SMBios::Inactive), {}, {});
    FieldsTestEntry fields("Test", {NamedField{"k", std::string("\"v\"")}, NamedField{"b", true}});
    BOOST_REQUIRE(bios->get() != nullptr);
    BOOST_REQUIRE(inactive.get() != nullptr);

    const std::string doc = render_document(JsonFormatter(), {bios->get(), inactive.get(), &fields});
    BOOST_CHECK(is_well_formed_json(doc));
}

/// Security case (Task 03 case 9): rendered through the registry exactly as
/// the CLI frames a document, (a) zero entries, (b) one real entry and (c)
/// the hostile multi-entry corpus (one representative per Story 01.0 batch
/// with hostile DMI strings, an InactiveEntry, every case 1-5 hostile
/// vendor, the case 6/8 boundary values, an empty double) are each one
/// well-formed JSON text; (c) has one top-level object per entry and
/// carries no raw C0 byte, ESC or U+2028/U+2029.
BOOST_AUTO_TEST_CASE(JsonFormatterRegistryDocumentsAreWellFormedTestCase)
{
    JsonFormatter linked; // names the class so its registration is linked
    const IOutputFormatter* formatter = get_output_formatter("json");
    BOOST_REQUIRE(formatter != nullptr);

    const std::string zero = render_document(*formatter, {});
    BOOST_CHECK(is_well_formed_json(zero));

    auto bios = make_bios_entry("Plain Vendor");
    BOOST_REQUIRE(bios->get() != nullptr);
    const std::string one = render_document(*formatter, {bios->get()});
    BOOST_CHECK(is_well_formed_json(one));
    BOOST_CHECK_EQUAL(count_occurrences(one, "\n  {\n    \"type\": "), static_cast<size_t>(1));

    const HostileDocumentCorpus corpus;
    const std::string many = render_document(*formatter, corpus.entries());
    BOOST_CHECK(is_well_formed_json(many));
    BOOST_CHECK_EQUAL(count_occurrences(many, "\n  {\n    \"type\": "), corpus.entries().size());
    BOOST_CHECK(many.find("\"type\": \"Inactive\",\n    \"fields\": {}") != std::string::npos);
    BOOST_CHECK(many.find("\"Max\": 18446744073709551615,") != std::string::npos);
    BOOST_CHECK(!contains_byte_in_range(many, 0x00, 0x08));
    BOOST_CHECK(!contains_byte_in_range(many, 0x0B, 0x1F)); // LF only as framing whitespace
    BOOST_CHECK(many.find('\t') == std::string::npos);
    BOOST_CHECK(many.find("\xE2\x80\xA8") == std::string::npos);
    BOOST_CHECK(many.find("\xE2\x80\xA9") == std::string::npos);
    BOOST_CHECK(is_valid_utf8(many));
}

/// Security case (Task 03 case 11, LOW-3 stance pin): a synthetic Memory
/// Channel entry declares 5 devices but its Length holds only 1 record.
/// get_fields() keeps text parity and emits all 5 (4 fabricated as 0), and
/// `json` renders exactly as many fields as get_fields() returns - neither
/// dropping nor adding. When the getter-side clamp lands, the expected 13
/// below moves with the getter.
BOOST_AUTO_TEST_CASE(JsonFormatterRendersPlaceholderRecordsUnclampedTestCase)
{
    SyntheticEntry channel(static_cast<uint8_t>(SMBios::MemoryChannel),
                           {0x04, 0x10, 0x05, /* record 1 */ 0x07, 0x34, 0x12}, {});
    BOOST_REQUIRE(channel.get() != nullptr);
    const std::vector<NamedField> fields = (*channel).get_fields();
    BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(3 + 2 * 5)); // stance: unclamped

    const std::string out = JsonFormatter().render(*channel);
    BOOST_CHECK(is_well_formed_json(out));
    BOOST_CHECK_EQUAL(count_occurrences(out, "\n      \""), fields.size());
    BOOST_CHECK(out.find("\"Memory Device Count\": 5,") != std::string::npos);
    BOOST_CHECK(out.find("\"Handle 1\": 4660,") != std::string::npos);  // the one real record
    BOOST_CHECK(out.find("\"Load 5\": 0,") != std::string::npos);       // fabricated placeholder
    BOOST_CHECK(out.find("\"Handle 5\": 0\n") != std::string::npos);    // ...rendered, not clamped
}

BOOST_AUTO_TEST_SUITE_END()
