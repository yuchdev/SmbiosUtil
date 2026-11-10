#include <string>

#include "structured_output_validators.hpp"

#include <boost/test/unit_test.hpp>

using smbios::test::is_valid_utf8;
using smbios::test::is_well_formed_flat_yaml;
using smbios::test::is_well_formed_json;
using smbios::test::is_well_formed_xml;

// Unit tests (milestone 0009, Story 02.0, Task 03, security-auditor INFO-6)
//
// Type: Unit - deterministic, no hardware/privileges, no parser dependency.
// Scenario: self-tests for the test-local validators every formatter test
// relies on (structured_output_validators.hpp/.cpp). Each validator must
// accept representative well-formed input AND reject every structural break
// an escaping bug could produce - otherwise a validator that accepts
// everything would let every `BOOST_CHECK(is_well_formed_*(...))` in the
// formatter tests pass silently. (These cases absorb the checker sanity
// tests Task 02 kept in xml_formatter_test.cpp / json_formatter_test.cpp.)
// Boundaries: empty input, a lone scalar, the deepest/longest escapes,
// each invalid-UTF-8 class, each XML 1.0-forbidden code point class, and
// (Story 03.0 Task 01) each non-printable YAML code point class plus
// UINT64_MAX / 2^64 for the flat YAML subset.
// On failure first check: structured_output_validators.cpp - the failing
// production's recogniser (JsonValidator / XmlValidator / FlatYamlValidator) or
// decode_utf8_at() for any UTF-8 case.

BOOST_AUTO_TEST_SUITE(StructuredOutputValidatorsUnitTests)

// ---------------------------------------------------------------- UTF-8 ---

/// Normal case: valid UTF-8 of every length, including U+10FFFF and the
/// last scalar before the surrogates, is accepted; empty is valid.
BOOST_AUTO_TEST_CASE(Utf8ValidatorAcceptsWellFormedTestCase)
{
    BOOST_CHECK(is_valid_utf8(""));
    BOOST_CHECK(is_valid_utf8("ASCII \x7F"));
    BOOST_CHECK(is_valid_utf8("\xC3\xA9" "\xE4\xB8\xAD" "\xF0\x9F\x98\x80" "\xF4\x8F\xBF\xBF" "\xED\x9F\xBF"));
    BOOST_CHECK(is_valid_utf8(std::string("a\0b", 3))); // U+0000 is a scalar value
}

/// Invalid-input case: each case-1 hostile class is rejected - lone
/// continuation, truncated, overlong, CESU surrogate, above U+10FFFF,
/// Latin-1, and the F5-FF bytes that never lead.
BOOST_AUTO_TEST_CASE(Utf8ValidatorRejectsMalformedTestCase)
{
    for (const char* bad : {"\x80", "\xC3", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "Caf\xE9",
                            "\xE0\x80\x80", "\xF5\x80\x80\x80", "\xFF", "\xE2\x82"}) {
        BOOST_TEST_CONTEXT("input " << std::string(bad).size() << " bytes") {
            BOOST_CHECK(!is_valid_utf8(bad));
        }
    }
}

// ----------------------------------------------------------------- JSON ---

/// Normal case: every value kind, nested structures, every legal escape
/// (including a surrogate-pair \u escape), empty containers, scalars at the
/// top level and surrounding whitespace are accepted.
BOOST_AUTO_TEST_CASE(JsonValidatorAcceptsWellFormedTestCase)
{
    BOOST_CHECK(is_well_formed_json("{\"a\": [1, -2.5e3, 0, 0.5, 1E+2, true, false, null, \"\\u00e9\"]}"));
    BOOST_CHECK(is_well_formed_json("[]"));
    BOOST_CHECK(is_well_formed_json("{}"));
    BOOST_CHECK(is_well_formed_json(" \t\r\n[\n\n]\n"));
    BOOST_CHECK(is_well_formed_json("18446744073709551615"));
    BOOST_CHECK(is_well_formed_json("\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t \\u001b \\u2028 \\ud83d\\ude00\""));
    BOOST_CHECK(is_well_formed_json("\"\xE2\x80\xAE \xEF\xBF\xBF \x7F\"")); // raw bidi, U+FFFF, DEL are legal
    BOOST_CHECK(is_well_formed_json("[{\"a\": 1}, {\"a\": 2}]"));           // same key, different objects
    BOOST_CHECK(is_well_formed_json("{\"a\": {\"a\": 1}}"));                // same key, nested object
}

/// Invalid-input case: every structural break an escaping or framing bug
/// could cause is rejected - unescaped quote, raw C0 control, bad escape,
/// truncated \u, invalid UTF-8, bad numbers, trailing comma, missing comma
/// or colon, unterminated input, trailing garbage, empty input.
BOOST_AUTO_TEST_CASE(JsonValidatorRejectsMalformedTestCase)
{
    const char* const bad_documents[] = {
        "",                          // empty
        "   ",                       // whitespace only
        "{\"a\": \"x\"y\"}",         // unescaped quote
        "{\"a\": \"\x1b\"}",         // raw ESC
        "[\"\x01\"]",                // raw U+0001
        "[\"\t\"]",                  // raw TAB in a string
        "[\"\\x41\"]",               // invalid escape
        "[\"\\u12\"]",               // truncated \u
        "[\"\\u12G4\"]",             // non-hex \u
        "[\"\\",                     // backslash at end
        "{\"a\": \"\xC0\xAF\"}",     // invalid UTF-8 inside a string
        "[1]\xFF",                   // invalid UTF-8 outside a string
        "[01]",                      // leading zero
        "[-]",                       // bare minus
        "[1.]",                      // empty fraction
        "[.5]",                      // missing integer part
        "[1e]",                      // empty exponent
        "[+1]",                      // leading plus
        "[NaN]",                     // not a JSON number
        "[1,]",                      // trailing comma (array)
        "{\"a\": 1,}",               // trailing comma (object)
        "[1 2]",                     // missing comma
        "{\"a\" 1}",                 // missing colon
        "{a: 1}",                    // unquoted key
        "{1: 1}",                    // non-string key
        "[1",                        // unterminated array
        "{\"a\": 1",                 // unterminated object
        "\"abc",                     // unterminated string
        "[] x",                      // trailing garbage
        "[][]",                      // two top-level values
        "tru",                       // truncated literal
        "'a'",                       // single quotes
    };
    for (const char* doc : bad_documents) {
        BOOST_TEST_CONTEXT("document: " << doc) {
            BOOST_CHECK(!is_well_formed_json(doc));
        }
    }
}

/// Invalid-input case (status.md duplicate-key advisory): a key repeated in
/// the same object is rejected, including when one spelling is escaped, so
/// a writer that ever emitted duplicate names would fail every JSON test.
BOOST_AUTO_TEST_CASE(JsonValidatorRejectsDuplicateKeysTestCase)
{
    BOOST_CHECK(!is_well_formed_json("{\"a\": 1, \"a\": 2}"));
    BOOST_CHECK(!is_well_formed_json("{\"a\": 1, \"b\": 2, \"a\": 3}"));
    BOOST_CHECK(!is_well_formed_json("{\"A\": 1, \"\\u0041\": 2}"));          // same key, escaped
    BOOST_CHECK(!is_well_formed_json("[{\"x\": {\"k\": 1, \"k\": 1}}]"));     // nested duplicate
    BOOST_CHECK(!is_well_formed_json("{\"\\ud83d\\ude00\": 1, \"\xF0\x9F\x98\x80\": 2}")); // pair vs raw
}

/// Boundary case: nesting is bounded (256), so a hostile depth cannot
/// overflow the validator's own stack; 200 levels is still accepted.
BOOST_AUTO_TEST_CASE(JsonValidatorBoundsNestingDepthTestCase)
{
    BOOST_CHECK(is_well_formed_json(std::string(200, '[') + std::string(200, ']')));
    BOOST_CHECK(!is_well_formed_json(std::string(100000, '[') + std::string(100000, ']')));
}

// ------------------------------------------------------------------ XML ---

/// Normal case: the writers' own document shape, an XML declaration,
/// self-closing tags, single-quoted attributes, every predefined entity,
/// legal character references and raw non-ASCII text are accepted.
BOOST_AUTO_TEST_CASE(XmlValidatorAcceptsWellFormedTestCase)
{
    BOOST_CHECK(is_well_formed_xml("<a/>"));
    BOOST_CHECK(is_well_formed_xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<a b=\"&amp;\">&lt;<c/></a>\n"));
    BOOST_CHECK(is_well_formed_xml(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<smbios>\n"
        "  <structure type=\"T\">\n    <field name=\"F\">x &amp; &lt;y&gt; &quot;&apos;</field>\n  </structure>\n"
        "  <structure type=\"Inactive\"></structure>\n</smbios>\n"));
    BOOST_CHECK(is_well_formed_xml("<a b='x' c = \"y\" ></a >"));
    BOOST_CHECK(is_well_formed_xml("<a>&#65;&#x41;&#x9;&#xFFFD;&#x10FFFF;</a>"));
    BOOST_CHECK(is_well_formed_xml("<a>\t\n\r \xE2\x80\xAE\xE2\x80\xA8\xEF\xBF\xBD\x7F</a>")); // legal raw chars
    BOOST_CHECK(is_well_formed_xml("<a>]]</a>"));
    BOOST_CHECK(is_well_formed_xml("<a>></a>"));
}

/// Invalid-input case: every structural break an escaping or framing bug
/// could cause is rejected - misnesting, bare `&`, unknown entity, broken
/// or duplicate attributes, two roots, text outside the root, `]]>`,
/// DOCTYPE/ENTITY/comment/CDATA/PI, unterminated constructs, empty input.
BOOST_AUTO_TEST_CASE(XmlValidatorRejectsMalformedTestCase)
{
    const char* const bad_documents[] = {
        "",                                       // empty
        "\n",                                     // no root
        "<a><b></a></b>",                         // misnested
        "<a>",                                    // unclosed
        "</a>",                                   // close without open
        "<a></b>",                                // mismatched close
        "<a>x & y</a>",                           // bare &
        "<a>&amp</a>",                            // unterminated entity
        "<a>&nbsp;</a>",                          // undeclared entity
        "<a>&#;</a>",                             // empty character reference
        "<a>&#xZZ;</a>",                          // bad hex reference
        "<a>&#99999999999;</a>",                  // reference above U+10FFFF
        "<a b=\"x\"y\">z</a>",                    // broken attribute
        "<a b=x>z</a>",                           // unquoted attribute
        "<a b=\"x\" b=\"y\"/>",                   // duplicate attribute
        "<a b=\"x\"c=\"y\"/>",                    // attributes without whitespace
        "<a b=\"<\"/>",                           // `<` in an attribute value
        "<a b=\"x & y\"/>",                       // bare & in an attribute value
        "<a b=\"x/>",                             // unterminated attribute value
        "<a/><b/>",                               // two roots
        "text<a/>",                               // text before the root
        "<a/>text",                               // text after the root
        "<a>]]></a>",                             // `]]>` in character data
        "<!DOCTYPE a><a/>",                       // DOCTYPE
        "<!DOCTYPE a [<!ENTITY x \"y\">]><a>&x;</a>", // entity declaration
        "<a><!ENTITY x \"y\"></a>",               // ENTITY inside the root
        "<a><!-- c --></a>",                      // comment (never emitted)
        "<a><![CDATA[x]]></a>",                   // CDATA (never emitted)
        "<a><?pi x?></a>",                        // processing instruction
        "<a/><?xml version=\"1.0\"?>",            // declaration not at the start
        "<?xml version=\"2.0\"?><a/>",            // unsupported version
        "<?xml-stylesheet x?><a/>",               // PI posing as a declaration
        "<1a/>",                                  // name starts with a digit
        "< a/>",                                  // space before the name
    };
    for (const char* doc : bad_documents) {
        BOOST_TEST_CONTEXT("document: " << doc) {
            BOOST_CHECK(!is_well_formed_xml(doc));
        }
    }
}

/// Security case (INFO-6): every XML 1.0-forbidden code point is rejected
/// both raw and as a character reference - each C0 byte other than
/// TAB/LF/CR, NUL, U+FFFE, U+FFFF - and invalid UTF-8 is rejected.
BOOST_AUTO_TEST_CASE(XmlValidatorRejectsForbiddenCodePointsTestCase)
{
    for (int b = 0x00; b <= 0x1F; ++b) {
        if (b == 0x09 || b == 0x0A || b == 0x0D) continue;
        BOOST_TEST_CONTEXT("C0 byte " << b) {
            BOOST_CHECK(!is_well_formed_xml("<a>" + std::string(1, static_cast<char>(b)) + "</a>"));
            BOOST_CHECK(!is_well_formed_xml("<a b=\"" + std::string(1, static_cast<char>(b)) + "\"/>"));
            BOOST_CHECK(!is_well_formed_xml("<a>&#" + std::to_string(b) + ";</a>"));
        }
    }
    BOOST_CHECK(!is_well_formed_xml("<a>&#x1B;</a>"));
    BOOST_CHECK(!is_well_formed_xml("<a>&#x1b;</a>"));
    BOOST_CHECK(!is_well_formed_xml("<a>\xEF\xBF\xBE</a>"));   // raw U+FFFE
    BOOST_CHECK(!is_well_formed_xml("<a>\xEF\xBF\xBF</a>"));   // raw U+FFFF
    BOOST_CHECK(!is_well_formed_xml("<a>&#xFFFE;</a>"));
    BOOST_CHECK(!is_well_formed_xml("<a>&#xFFFF;</a>"));
    BOOST_CHECK(!is_well_formed_xml("<a>&#xD800;</a>"));       // surrogate reference
    BOOST_CHECK(!is_well_formed_xml("<a>\xC0\xAF</a>"));       // overlong
    BOOST_CHECK(!is_well_formed_xml("<a>\xED\xA0\x80</a>"));   // CESU surrogate
    BOOST_CHECK(!is_well_formed_xml("<a>Caf\xE9</a>"));        // Latin-1
}

// ----------------------------------------------------------------- YAML ---

/// Normal case: zero entries, one- and two-entry documents, every value
/// kind (dq, 0, UINT64_MAX, true, false), `fields: {}`, every YAML 1.2
/// escape and raw printable non-ASCII (bidi, U+FFFD, emoji, NBSP) plus YAML
/// indicator characters inside quotes are accepted.
BOOST_AUTO_TEST_CASE(FlatYamlValidatorAcceptsWellFormedTestCase)
{
    BOOST_CHECK(is_well_formed_flat_yaml("---\n\n"));
    BOOST_CHECK(is_well_formed_flat_yaml("---\n- type: \"T\"\n  fields: {}\n"));
    BOOST_CHECK(is_well_formed_flat_yaml(
        "---\n"
        "- type: \"A\"\n"
        "  fields:\n"
        "    \"s\": \"NO\"\n"
        "    \"z\": 0\n"
        "    \"max\": 18446744073709551615\n"
        "    \"t\": true\n"
        "    \"f\": false\n"
        "- type: \"B\"\n"
        "  fields: {}\n"));
    BOOST_CHECK(is_well_formed_flat_yaml(
        "---\n- type: \"\\0\\a\\b\\t\\v\\n\\f\\r\\e\\ \\\"\\/\\\\\\N\\_\\L\\P\\x7f\\u00E9\\U0001F600\"\n  fields: {}\n"));
    BOOST_CHECK(is_well_formed_flat_yaml(
        "---\n- type: \"\xE2\x80\xAE \xEF\xBF\xBD \xF0\x9F\x98\x80 \xC2\xA0 # : - & * ! % ~\"\n  fields: {}\n"));
    // Same key in two different mappings is not a duplicate.
    BOOST_CHECK(is_well_formed_flat_yaml("---\n- type: \"T\"\n  fields:\n    \"a\": 1\n- type: \"T\"\n  fields:\n    \"a\": 2\n"));
}

/// Invalid-input case: every structural or typing break an emitter bug
/// could cause is rejected - unquoted strings (implicit-typing traps),
/// anchors/aliases/tags, bad numbers, bad or truncated escapes, unterminated
/// quotes, wrong indentation, CRLF, missing marker or final LF, tabs,
/// trailing garbage, duplicate keys.
BOOST_AUTO_TEST_CASE(FlatYamlValidatorRejectsMalformedTestCase)
{
    const std::string head = "---\n- type: \"T\"\n  fields:\n    \"k\": ";
    const char* const bad_values[] = {
        "NO", "yes", "~", "null", "0x0A", "1.10", "-1", "+1", "01", "1e3",
        "18446744073709551616",         // 2^64
        "99999999999999999999",         // 20 digits, above 2^64-1
        "True", "&a \"x\"", "*a", "!!str x", "\"x", "\"x\\\"", "\"\\q\"", "\"\\x4\"",
        "\"\\u12G4\"", "\"\\ud800\"", "\"\\U00110000\"", "", "\"a\" #c", "\"a\"  ",
    };
    for (const char* v : bad_values) {
        BOOST_TEST_CONTEXT("value [" << v << "]") {
            BOOST_CHECK(!is_well_formed_flat_yaml(head + v + "\n"));
        }
    }
    const char* const bad_documents[] = {
        "",                                                   // empty
        "---\n",                                              // no root line
        "---",                                                // no LF
        "- type: \"T\"\n  fields: {}\n",                      // no document marker
        "---\n- type: \"T\"\n  fields: {}",                   // no final LF
        "---\n- type: \"T\"\r\n  fields: {}\r\n",             // CRLF
        "---\n- type: T\n  fields: {}\n",                     // plain type
        "---\n- type: \"T\"\n  fields:\n",                    // empty block mapping
        "---\n- type: \"T\"\n fields: {}\n",                  // wrong indentation
        "---\n- type: \"T\"\n  fields:\n   \"k\": 1\n",       // wrong field indentation
        "---\n- type: \"T\"\n  fields:\n    k: 1\n",          // plain key
        "---\n- type: \"T\"\n  fields:\n    \"k\":1\n",       // no space after colon
        "---\n- type: \"T\"\n  fields:\n\t\"k\": 1\n",        // tab indentation
        "---\n- type: \"T\"\n  fields: {}\n\n",               // trailing blank line
        "---\n- type: \"T\"\n  fields: {}\n...\n",            // trailing content
        "%YAML 1.2\n---\n\n",                                 // directive
        "---\n- type: !t \"T\"\n  fields: {}\n",              // tag
        "---\n- type: \"T\"\n  fields:\n    \"k\": 1\n    \"k\": 2\n",     // duplicate key
        "---\n- type: \"T\"\n  fields:\n    \"A\": 1\n    \"\\x41\": 2\n", // duplicate after decoding
        "---\n- type: \"a\nb\"\n  fields: {}\n",              // raw LF inside a scalar
    };
    for (const char* doc : bad_documents) {
        BOOST_TEST_CONTEXT("document [" << doc << "]") {
            BOOST_CHECK(!is_well_formed_flat_yaml(doc));
        }
    }
}

/// Security case (INFO-6, SR-2): every raw non-printable code point inside a
/// scalar is rejected - each C0 byte (including TAB), DEL, each C1 code
/// point (U+0085 included), U+2028, U+2029, U+FEFF, U+FFFE, U+FFFF - as are
/// invalid UTF-8 and a leading BOM.
BOOST_AUTO_TEST_CASE(FlatYamlValidatorRejectsNonPrintableTestCase)
{
    const auto doc = [](const std::string& body) { return "---\n- type: \"" + body + "\"\n  fields: {}\n"; };
    BOOST_REQUIRE(is_well_formed_flat_yaml(doc("ok")));
    for (int b = 0x00; b <= 0x1F; ++b) {
        BOOST_TEST_CONTEXT("C0 byte " << b) {
            BOOST_CHECK(!is_well_formed_flat_yaml(doc(std::string(1, static_cast<char>(b)))));
        }
    }
    BOOST_CHECK(!is_well_formed_flat_yaml(doc("\x7F")));
    for (int c = 0x80; c <= 0x9F; ++c) {
        BOOST_TEST_CONTEXT("C1 code point " << c) {
            const std::string utf8{static_cast<char>(0xC2), static_cast<char>(c)};
            BOOST_CHECK(!is_well_formed_flat_yaml(doc(utf8)));
        }
    }
    for (const char* raw : {"\xE2\x80\xA8", "\xE2\x80\xA9", "\xEF\xBB\xBF", "\xEF\xBF\xBE", "\xEF\xBF\xBF",
                            "\xC0\xAF", "\xED\xA0\x80", "Caf\xE9"}) {
        BOOST_TEST_CONTEXT("raw " << std::string(raw).size() << " bytes") {
            BOOST_CHECK(!is_well_formed_flat_yaml(doc(raw)));
        }
    }
    BOOST_CHECK(!is_well_formed_flat_yaml("\xEF\xBB\xBF---\n\n")); // leading BOM
}

BOOST_AUTO_TEST_SUITE_END()
