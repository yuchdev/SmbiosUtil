#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>
#include <libsmbios/formatters/yaml_formatter.hpp>

#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using namespace smbios::test;

// Unit tests (milestone 0009, Story 03.0, Task 01)
//
// Type: Unit - deterministic, no hardware/privileges, no YAML library.
// Scenario: YamlFormatter (`yaml`) renders get_type()/get_fields() only
// (R4) as `- type: .. / fields: {name: value, ...}` items of one block
// sequence inside one `---` document: exact output for every FieldValue
// alternative (uint64_t as exact plain digits, bool plain, bytes as a
// double-quoted hex string, every string double-quoted), YAML
// implicit-typing traps in values and keys, every C0 control, DEL, C1
// (CSI), ANSI SGR, U+0085/U+2028/U+2029, U+FEFF/U+FFFE/U+FFFF and bidi,
// invalid UTF-8 replaced by U+FFFD, empty-field entries, document framing
// for zero, one and many entries, the hostile multi-entry corpus through
// the registry.
// Boundaries: UINT64_MAX / 0, a 0-byte and a 255-byte vector, every
// C0 control plus DEL, U+0085 (the only C1 with a short form), U+FFFE.
// Well-formedness is asserted by exact expected text plus
// is_well_formed_flat_yaml() (structured_output_validators.hpp, self-tested
// in structured_output_validators_test.cpp); the real-parser round trip is
// the dev-only PyYAML `yaml.safe_load` check recorded in the task's PR.
// On failure first check: libsmbios/src/yaml_formatter.cpp's yaml_value()
// dispatch and render() layout; for escaping, formatter_escaping.cpp's
// yaml_double_quoted_escape(); for a null registry lookup, that this TU
// still constructs YamlFormatter.

BOOST_AUTO_TEST_SUITE(YamlFormatterUnitTests)

namespace {

/// @brief A FieldsTestEntry holding one field named "F".
FieldsTestEntry single_field(FieldValue value)
{
    return FieldsTestEntry("T", {NamedField{"F", std::move(value)}});
}

/// @brief The exact render() of single_field() whose YAML value is `value`.
std::string single_field_yaml(const std::string& value)
{
    return "- type: \"T\"\n  fields:\n    \"F\": " + value;
}

/// @brief A complete one-entry document around `rendered`.
std::string as_document(const std::string& rendered)
{
    return "---\n" + rendered + "\n";
}

} // namespace

/// Normal case: `yaml` is registered, resolves to a YamlFormatter and is
/// named "yaml". Constructing YamlFormatter here links its registration.
BOOST_AUTO_TEST_CASE(YamlFormatterResolvesThroughRegistryTestCase)
{
    YamlFormatter direct;
    BOOST_CHECK_EQUAL(direct.format_name(), "yaml");

    const IOutputFormatter* formatter = get_output_formatter("yaml");
    BOOST_REQUIRE(formatter != nullptr);
    BOOST_CHECK(dynamic_cast<const YamlFormatter*>(formatter) != nullptr);
}

/// Normal and boundary case: exact output for every FieldValue alternative
/// in get_fields() order; UINT64_MAX and 0 are plain exact digits; bools
/// are plain; bytes are a double-quoted hex string (a hex string that looks
/// numeric, "1e10", stays quoted); escaping in the type label, a key and a
/// value.
BOOST_AUTO_TEST_CASE(YamlFormatterRendersEveryAlternativeTestCase)
{
    FieldsTestEntry entry("Type \"Q\" \\", {
        NamedField{"Vendor", std::string("Acme \"Firmware\" \\ & <x> 'a' /")},
        NamedField{"Empty", std::string()},
        NamedField{"ROM Size", uint64_t{65536}},
        NamedField{"Max", std::numeric_limits<uint64_t>::max()},
        NamedField{"Zero", uint64_t{0}},
        NamedField{"ISA is supported", true},
        NamedField{"Off", false},
        NamedField{"Processor ID", std::vector<uint8_t>{0xA1, 0x06, 0x03, 0x00, 0xFF}},
        NamedField{"Numeric Hex", std::vector<uint8_t>{0x1E, 0x10}},
        NamedField{"No Bytes", std::vector<uint8_t>{}},
        NamedField{"Key \"q\"\n", std::string("x")},
    });
    const std::string expected =
        "- type: \"Type \\\"Q\\\" \\\\\"\n"
        "  fields:\n"
        "    \"Vendor\": \"Acme \\\"Firmware\\\" \\\\ & <x> 'a' /\"\n"
        "    \"Empty\": \"\"\n"
        "    \"ROM Size\": 65536\n"
        "    \"Max\": 18446744073709551615\n"
        "    \"Zero\": 0\n"
        "    \"ISA is supported\": true\n"
        "    \"Off\": false\n"
        "    \"Processor ID\": \"a1060300ff\"\n"
        "    \"Numeric Hex\": \"1e10\"\n"
        "    \"No Bytes\": \"\"\n"
        "    \"Key \\\"q\\\"\\n\": \"x\"";
    const std::string out = YamlFormatter().render(entry);
    BOOST_CHECK_EQUAL(out, expected);
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(out)));
}

/// Security/edge case (ratified quoting rule): every YAML implicit-typing
/// trap, as a value and as a key, is emitted double-quoted - never plain -
/// so it can only load back as a string: bool-likes, numbers, null-likes,
/// `": "`, a leading `-`, anchor/alias/tag/directive/comment indicators,
/// an embedded `"`/`\` and a newline.
BOOST_AUTO_TEST_CASE(YamlFormatterQuotesImplicitTypingTrapsTestCase)
{
    const std::vector<std::pair<std::string, std::string>> traps{
        {"NO", "\"NO\""},
        {"yes", "\"yes\""},
        {"1.10", "\"1.10\""},
        {"0x0A", "\"0x0A\""},
        {"~", "\"~\""},
        {"null", "\"null\""},
        {"", "\"\""},
        {"a: b", "\"a: b\""},
        {"- item", "\"- item\""},
        {"&anchor", "\"&anchor\""},
        {"*alias", "\"*alias\""},
        {"!tag x", "\"!tag x\""},
        {"%YAML 1.2", "\"%YAML 1.2\""},
        {"# comment", "\"# comment\""},
        {"x # y", "\"x # y\""},
        {"{a: 1}", "\"{a: 1}\""},
        {"say \"hi\" \\o/", "\"say \\\"hi\\\" \\\\o/\""},
        {"line1\nline2", "\"line1\\nline2\""},
        {"---", "\"---\""},
    };
    for (const auto& t : traps) {
        BOOST_TEST_CONTEXT("trap [" << t.first << "]") {
            const std::string as_value = YamlFormatter().render(single_field(std::string(t.first)));
            BOOST_CHECK_EQUAL(as_value, single_field_yaml(t.second));
            BOOST_CHECK(is_well_formed_flat_yaml(as_document(as_value)));

            FieldsTestEntry keyed("T", {NamedField{t.first, uint64_t{1}}});
            const std::string as_key = YamlFormatter().render(keyed);
            BOOST_CHECK_EQUAL(as_key, "- type: \"T\"\n  fields:\n    " + t.second + ": 1");
            BOOST_CHECK(is_well_formed_flat_yaml(as_document(as_key)));

            FieldsTestEntry typed(t.first, {});
            BOOST_CHECK_EQUAL(YamlFormatter().render(typed), "- type: " + t.second + "\n  fields: {}");
        }
    }
}

/// Edge case: an entry with no fields - a real synthetic InactiveEntry -
/// renders `fields: {}`, so `fields` is always a mapping.
BOOST_AUTO_TEST_CASE(YamlFormatterRendersEmptyFieldsTestCase)
{
    SyntheticEntry inactive(static_cast<uint8_t>(SMBios::Inactive), {}, {});
    BOOST_REQUIRE(inactive.get() != nullptr);
    BOOST_REQUIRE((*inactive).get_fields().empty());
    const std::string out = YamlFormatter().render(*inactive);
    BOOST_CHECK_EQUAL(out, "- type: \"Inactive\"\n  fields: {}");
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(out)));
}

/// Edge case: exact framing for zero, one and two entries; each document
/// is one valid YAML document (zero entries: an empty root node).
BOOST_AUTO_TEST_CASE(YamlFormatterDocumentFramingTestCase)
{
    YamlFormatter formatter;
    BOOST_CHECK_EQUAL(formatter.begin_document(), "---\n");
    BOOST_CHECK_EQUAL(formatter.entry_separator(), "\n");
    BOOST_CHECK_EQUAL(formatter.end_document(), "\n");

    FieldsTestEntry a("A", {NamedField{"n", uint64_t{1}}});
    FieldsTestEntry b("B", {});

    const std::string zero = render_document(formatter, {});
    BOOST_CHECK_EQUAL(zero, "---\n\n");
    BOOST_CHECK(is_well_formed_flat_yaml(zero));

    const std::string one = render_document(formatter, {&a});
    BOOST_CHECK_EQUAL(one, "---\n- type: \"A\"\n  fields:\n    \"n\": 1\n");
    BOOST_CHECK(is_well_formed_flat_yaml(one));

    const std::string two = render_document(formatter, {&a, &b});
    BOOST_CHECK_EQUAL(two,
        "---\n"
        "- type: \"A\"\n"
        "  fields:\n"
        "    \"n\": 1\n"
        "- type: \"B\"\n"
        "  fields: {}\n");
    BOOST_CHECK(is_well_formed_flat_yaml(two));
    BOOST_CHECK(two.find('\r') == std::string::npos); // always \n, never CRLF
}

/// Security case: a synthetic BIOS Information entry with each invalid-UTF-8
/// vendor string renders that vendor with one U+FFFD per maximal invalid
/// subpart, double-quoted, and the whole document stays valid.
BOOST_AUTO_TEST_CASE(YamlFormatterReplacesInvalidUtf8VendorTestCase)
{
    YamlFormatter formatter;
    for (const auto& c : invalid_utf8_cases()) {
        BOOST_TEST_CONTEXT("vendor size " << c.first.size()) {
            auto entry = make_bios_entry(c.first);
            BOOST_REQUIRE(entry->get() != nullptr);
            const std::string out = formatter.render(**entry);
            BOOST_CHECK(out.find("- type: \"BIOS Information\"\n") == 0);
            BOOST_CHECK(out.find("\n    \"Vendor\": \"" + c.second + "\"\n") != std::string::npos);
            BOOST_CHECK(is_well_formed_flat_yaml(render_document(formatter, {entry->get()})));
        }
    }
}

/// Security case (INFO-6): every byte 0x01-0x1F and DEL is escaped - YAML
/// short forms where they exist (\a \b \t \n \v \f \r \e), lowercase \xXX
/// otherwise, DEL as \x7f - so no raw C0 byte or DEL appears; the ANSI SGR
/// sequence's ESC becomes \e.
BOOST_AUTO_TEST_CASE(YamlFormatterEscapesControlCharactersTestCase)
{
    const std::string controls = YamlFormatter().render(single_field(std::string(all_c0_controls_and_del())));
    BOOST_CHECK_EQUAL(controls, single_field_yaml(
        "\"\\x01\\x02\\x03\\x04\\x05\\x06\\a\\b\\t\\n\\v\\f\\r\\x0e\\x0f"
        "\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1a\\e"
        "\\x1c\\x1d\\x1e\\x1f\\x7f\""));
    BOOST_CHECK(!contains_byte_in_range(controls, 0x00, 0x09));
    BOOST_CHECK(!contains_byte_in_range(controls, 0x0B, 0x1F)); // LF only as framing
    BOOST_CHECK(!contains_byte_in_range(controls, 0x7F, 0x7F));
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(controls)));

    const std::string ansi = YamlFormatter().render(single_field(std::string("\x1b[31mRED\x1b[0m")));
    BOOST_CHECK_EQUAL(ansi, single_field_yaml("\"\\e[31mRED\\e[0m\""));
    BOOST_CHECK(!contains_byte_in_range(ansi, 0x1B, 0x1B));
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(ansi)));
}

/// Regression case (threat model SR-1/INFO-6), at formatter level: the C1
/// pair `C2 9B` (8-bit CSI) followed by `2J` renders as \x9b, never the raw
/// pair; the last C1 control U+009F is escaped too, and U+0085 NEXT LINE
/// becomes \N. No raw C1 code point remains.
BOOST_AUTO_TEST_CASE(YamlFormatterEscapesC1ControlsTestCase)
{
    const std::string out = YamlFormatter().render(single_field(std::string("\xC2\x9B" "2J\xC2\x9F\xC2\x85" "x")));
    BOOST_CHECK_EQUAL(out, single_field_yaml("\"\\x9b2J\\x9f\\Nx\""));
    BOOST_CHECK(out.find("\xC2\x9B") == std::string::npos);
    BOOST_CHECK(out.find("\xC2\x85") == std::string::npos);
    BOOST_CHECK(!contains_byte_in_range(out, 0x7F, 0xFF));
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(out)));
}

/// Security case (INFO-6, SR-2): U+2028/U+2029 become \L/\P (YAML 1.1 line
/// breaks), U+FEFF becomes \ufeff, U+FFFE/U+FFFF become \ufffe/\uffff, and
/// the bidi overrides U+202E/U+2066/U+2069 pass through raw as printable
/// data.
BOOST_AUTO_TEST_CASE(YamlFormatterLineSeparatorsBomAndNoncharactersTestCase)
{
    const std::string bidi = "\xE2\x80\xAE" "\xE2\x81\xA6" "\xE2\x81\xA9";
    const std::string out = YamlFormatter().render(single_field(
        std::string(bidi + "\xE2\x80\xA8" "\xE2\x80\xA9" "\xEF\xBB\xBF" "\xEF\xBF\xBE" "\xEF\xBF\xBF")));
    BOOST_CHECK_EQUAL(out, single_field_yaml("\"" + bidi + "\\L\\P\\ufeff\\ufffe\\uffff\""));
    for (const char* raw : {"\xE2\x80\xA8", "\xE2\x80\xA9", "\xEF\xBB\xBF", "\xEF\xBF\xBE", "\xEF\xBF\xBF"}) {
        BOOST_CHECK(out.find(raw) == std::string::npos);
    }
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(out)));
}

/// Edge case: a 255-byte vector renders as exactly 510 lowercase hex
/// characters inside double quotes, untruncated.
BOOST_AUTO_TEST_CASE(YamlFormatterRendersMaximumByteFieldTestCase)
{
    std::vector<uint8_t> bytes;
    for (int b = 0x00; b <= 0xFE; ++b) {
        bytes.push_back(static_cast<uint8_t>(b));
    }
    const std::string out = YamlFormatter().render(single_field(bytes));
    BOOST_CHECK_EQUAL(out, single_field_yaml("\"" + hex_encode(bytes) + "\""));
    BOOST_CHECK_EQUAL(hex_encode(bytes).size(), static_cast<size_t>(510));
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(out)));
}

/// Security case: rendered through the registry exactly as the CLI frames
/// a document, (a) zero entries, (b) one real entry and (c) the hostile
/// multi-entry corpus (formatter_test_support.hpp) are each one well-formed
/// document; (c) has one sequence item per entry and carries no raw C0
/// (other than framing LF), DEL, C1, U+2028/U+2029 or U+FEFF, and no
/// anchor/alias/tag/directive indicator outside a quoted scalar.
BOOST_AUTO_TEST_CASE(YamlFormatterRegistryDocumentsAreWellFormedTestCase)
{
    YamlFormatter linked; // names the class so its registration is linked
    const IOutputFormatter* formatter = get_output_formatter("yaml");
    BOOST_REQUIRE(formatter != nullptr);

    const std::string zero = render_document(*formatter, {});
    BOOST_CHECK(is_well_formed_flat_yaml(zero));

    auto bios = make_bios_entry("Plain Vendor");
    BOOST_REQUIRE(bios->get() != nullptr);
    const std::string one = render_document(*formatter, {bios->get()});
    BOOST_CHECK(is_well_formed_flat_yaml(one));
    BOOST_CHECK_EQUAL(count_occurrences(one, "\n- type: "), static_cast<size_t>(1));

    const HostileDocumentCorpus corpus;
    const std::string many = render_document(*formatter, corpus.entries());
    BOOST_CHECK(is_well_formed_flat_yaml(many));
    BOOST_CHECK_EQUAL(count_occurrences(many, "\n- type: "), corpus.entries().size());
    BOOST_CHECK(many.find("- type: \"Inactive\"\n  fields: {}\n") != std::string::npos);
    BOOST_CHECK(many.find("\n    \"Max\": 18446744073709551615\n") != std::string::npos);
    BOOST_CHECK(many.find("\n    \"Flag\": true\n") != std::string::npos);
    BOOST_CHECK(!contains_byte_in_range(many, 0x00, 0x09));
    BOOST_CHECK(!contains_byte_in_range(many, 0x0B, 0x1F));
    BOOST_CHECK(!contains_byte_in_range(many, 0x7F, 0x7F));
    for (unsigned char c1 = 0x80; c1 <= 0x9F; ++c1) {
        BOOST_CHECK(many.find(std::string{static_cast<char>(0xC2), static_cast<char>(c1)}) == std::string::npos);
    }
    BOOST_CHECK(many.find("\xE2\x80\xA8") == std::string::npos);
    BOOST_CHECK(many.find("\xE2\x80\xA9") == std::string::npos);
    BOOST_CHECK(many.find("\xEF\xBB\xBF") == std::string::npos);
    BOOST_CHECK(is_valid_utf8(many));
}

/// Security case (LOW-3 stance pin): a synthetic Memory Channel entry
/// declares 5 devices but its Length holds only 1 record; `yaml` renders
/// exactly as many fields as get_fields() returns - neither dropping nor
/// adding. When the getter-side clamp lands, the expected 13 moves with it.
BOOST_AUTO_TEST_CASE(YamlFormatterRendersPlaceholderRecordsUnclampedTestCase)
{
    SyntheticEntry channel(static_cast<uint8_t>(SMBios::MemoryChannel),
                           {0x04, 0x10, 0x05, /* record 1 */ 0x07, 0x34, 0x12}, {});
    BOOST_REQUIRE(channel.get() != nullptr);
    const std::vector<NamedField> fields = (*channel).get_fields();
    BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(3 + 2 * 5)); // stance: unclamped

    const std::string out = YamlFormatter().render(*channel);
    BOOST_CHECK(is_well_formed_flat_yaml(as_document(out)));
    BOOST_CHECK_EQUAL(count_occurrences(out, "\n    \""), fields.size());
    BOOST_CHECK(out.find("\n    \"Memory Device Count\": 5\n") != std::string::npos);
    BOOST_CHECK(out.find("\n    \"Handle 1\": 4660\n") != std::string::npos); // the one real record
    BOOST_CHECK(out.find("\n    \"Handle 5\": 0") != std::string::npos);      // fabricated placeholder
}

BOOST_AUTO_TEST_SUITE_END()
