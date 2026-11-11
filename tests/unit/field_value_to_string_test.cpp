#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <libsmbios/field_value.hpp>
#include <libsmbios/formatters/formatter_escaping.hpp>

#include <boost/test/unit_test.hpp>

using namespace smbios;

// Unit tests (milestone 0009, Story 02.0 review fixes)
//
// Type: Unit - deterministic, no hardware/privileges needed.
// Scenario: field_value_to_string() (libsmbios/field_value.hpp), the shared
// FieldValue stringifier behind the xml/json writers: decimal digits for
// uint64_t, "true"/"false" for bool, hex_encode() for bytes, and the string
// itself, unescaped.
// Boundaries: 0, 2^53+1 and UINT64_MAX; empty string/vector; a string full
// of markup, quotes, controls and invalid UTF-8 (must come back untouched);
// all 256 byte values.
// On failure first check: libsmbios/src/field_value.cpp's visitor, then
// hex_encode() in formatter_escaping.cpp. A change here also changes
// xml/json output - re-run xml_formatter_test.cpp / json_formatter_test.cpp.

BOOST_AUTO_TEST_SUITE(FieldValueToStringUnitTests)

/// Normal case: one value of each alternative.
BOOST_AUTO_TEST_CASE(FieldValueToStringEachAlternativeTestCase)
{
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{std::string("Acme")}), "Acme");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{uint64_t{65536}}), "65536");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{true}), "true");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{false}), "false");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{std::vector<uint8_t>{0xA1, 0x06, 0x03, 0x00}}), "a1060300");
}

/// Edge cases: integer extremes as exact digits (never exponent form, never
/// rounded through a double); empty string and empty byte vector are empty.
BOOST_AUTO_TEST_CASE(FieldValueToStringBoundariesTestCase)
{
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{uint64_t{0}}), "0");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{uint64_t{9007199254740993ull}}), "9007199254740993");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{std::numeric_limits<uint64_t>::max()}),
                      "18446744073709551615");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{std::string()}), "");
    BOOST_CHECK_EQUAL(field_value_to_string(FieldValue{std::vector<uint8_t>{}}), "");

    std::vector<uint8_t> all_bytes;
    for (int b = 0x00; b <= 0xFF; ++b) {
        all_bytes.push_back(static_cast<uint8_t>(b));
    }
    const std::string hex = field_value_to_string(FieldValue{all_bytes});
    BOOST_CHECK_EQUAL(hex, hex_encode(all_bytes));
    BOOST_CHECK_EQUAL(hex.size(), 512u);
}

/// Security-contract case: strings are returned as-is, *unescaped* -
/// markup, quotes, ESC/C1 controls, an embedded NUL and invalid UTF-8 all
/// come back byte-for-byte, so escaping stays the formatter's job.
BOOST_AUTO_TEST_CASE(FieldValueToStringLeavesStringsUnescapedTestCase)
{
    const std::string hostile = std::string("<a & \"b\"> \\ \x1b[31m \xC2\x9B Caf\xE9 ") + std::string("\0", 1) + "end";
    BOOST_CHECK(field_value_to_string(FieldValue{hostile}) == hostile);
}

BOOST_AUTO_TEST_SUITE_END()
