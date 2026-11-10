#include <cstdint>
#include <string>
#include <vector>

#include <libsmbios/formatters/formatter_escaping.hpp>

#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using smbios::test::kReplacement;

// Unit tests (milestone 0009, Story 02.0, Task 02)
//
// Type: Unit - deterministic, no hardware/privileges, no new dependency.
// Scenario: the shared escaping helpers in
// libsmbios/include/libsmbios/formatters/formatter_escaping.hpp, driven by
// table-driven input/expected-output pairs: the strict UTF-8 sanitising
// step (maximal-subpart U+FFFD replacement), xml_escape() (replace the
// XML 1.0-forbidden set, escape the five specials), json_escape() (short
// forms, \u00XX, U+2028/U+2029), yaml_double_quoted_escape() (YAML short
// forms, \xXX for C0/DEL/C1, \N \L \P, \ufffe \uffff) and hex_encode().
// Boundaries: empty input; every byte 0x00-0xFF on its own; a seeded
// pseudo-random sweep asserting the output invariants (valid UTF-8, no
// forbidden raw bytes, sanitize idempotence) for arbitrary bytes.
// On failure first check: libsmbios/src/formatter_escaping.cpp's
// for_each_code_point() (the single shared decoder) for any UTF-8 case, or
// the per-escaper switch for a single-character mapping.

namespace {

/// @brief One escaper input and its exact expected output.
struct EscapeCase {
    std::string input;
    std::string expected;
};

/// @brief Runs `escaper` over every case and checks the exact output.
template <typename Escaper>
void check_cases(Escaper escaper, const std::vector<EscapeCase>& cases)
{
    for (size_t i = 0; i < cases.size(); ++i) {
        BOOST_TEST_CONTEXT("case " << i) {
            BOOST_CHECK_EQUAL(escaper(cases[i].input), cases[i].expected);
        }
    }
}

/// @brief Deterministic 32-bit LCG (Numerical Recipes constants), so the
/// pseudo-random sweep is identical on every run and platform.
struct Lcg {
    uint32_t state;
    uint32_t next() { state = state * 1664525u + 1013904223u; return state; }
};

/// @brief `count` deterministic pseudo-random byte strings of length 0-15,
/// biased towards the bytes that matter for UTF-8 (C0, 0x80-0xBF, leads).
std::vector<std::string> random_byte_strings(size_t count)
{
    static const unsigned char interesting[] = {
        0x00, 0x09, 0x0A, 0x0D, 0x1B, 0x22, 0x26, 0x27, 0x3C, 0x3E, 0x5C, 0x41, 0x7F,
        0x80, 0x85, 0x8F, 0x90, 0x9F, 0xA0, 0xAF, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xDF,
        0xE0, 0xE1, 0xE2, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xFE, 0xFF,
    };
    Lcg rng{0x5EED1234u};
    std::vector<std::string> strings;
    strings.reserve(count);
    for (size_t n = 0; n < count; ++n) {
        std::string s;
        const size_t length = (rng.next() >> 16) % 16;
        for (size_t i = 0; i < length; ++i) {
            const uint32_t r = rng.next() >> 8;
            s.push_back(static_cast<char>((r & 1u) ? interesting[(r >> 1) % sizeof(interesting)]
                                                  : static_cast<unsigned char>(r >> 1)));
        }
        strings.push_back(s);
    }
    return strings;
}

const std::string kEmoji = "\xF0\x9F\x98\x80";     // U+1F600 (4 bytes)
const std::string kCjk = "\xE4\xB8\xAD";           // U+4E2D (3 bytes)
const std::string kEAcute = "\xC3\xA9";            // U+00E9 (2 bytes)
const std::string kLineSep = "\xE2\x80\xA8";       // U+2028
const std::string kParaSep = "\xE2\x80\xA9";       // U+2029
const std::string kNel = "\xC2\x85";               // U+0085
const std::string kCsi = "\xC2\x9B";               // U+009B (C1 CSI)
const std::string kFFFE = "\xEF\xBF\xBE";          // U+FFFE
const std::string kFFFF = "\xEF\xBF\xBF";          // U+FFFF
const std::string kRlo = "\xE2\x80\xAE";           // U+202E bidi override
const std::string kLri = "\xE2\x81\xA6";           // U+2066 bidi isolate
const std::string kPdi = "\xE2\x81\xA9";           // U+2069 bidi isolate end
const std::string kMaxScalar = "\xF4\x8F\xBF\xBF"; // U+10FFFF

} // namespace

BOOST_AUTO_TEST_SUITE(FormatterEscapingUnitTests)

// ---------------------------------------------------------------- UTF-8 ---

/// Normal case: valid UTF-8 of every length (1-4 bytes, up to U+10FFFF)
/// passes through byte-for-byte.
BOOST_AUTO_TEST_CASE(SanitizeUtf8PassesValidInputThroughTestCase)
{
    const std::string valid = "ASCII " + kEAcute + kCjk + kEmoji + kMaxScalar + kFFFF + "\x7F" + kCsi;
    BOOST_CHECK_EQUAL(sanitize_utf8(valid), valid);
    BOOST_CHECK_EQUAL(sanitize_utf8(""), "");
    // U+0000 is a valid code point; sanitising keeps it (escapers decide).
    BOOST_CHECK_EQUAL(sanitize_utf8(std::string("a\0b", 3)), std::string("a\0b", 3));
}

/// Invalid-input case (security, INFO-6 / Task 03 case 1): each maximal
/// invalid subpart becomes exactly one U+FFFD - lone continuation,
/// truncated, overlong, CESU surrogate, above U+10FFFF, Latin-1.
BOOST_AUTO_TEST_CASE(SanitizeUtf8ReplacesEachMaximalSubpartOnceTestCase)
{
    for (const auto& c : smbios::test::invalid_utf8_cases()) {
        BOOST_TEST_CONTEXT("input size " << c.first.size()) {
            BOOST_CHECK_EQUAL(sanitize_utf8(c.first), c.second);
        }
    }
}

/// Edge case: further maximal-subpart boundaries - a sequence truncated by
/// a following valid byte resumes *at* that byte; a truncated 3/4-byte
/// prefix is one unit; E0/F0 overlongs and F5-FF never start a sequence.
/// Expected values cross-checked against Python's
/// `bytes.decode("utf-8", "replace")`.
BOOST_AUTO_TEST_CASE(SanitizeUtf8MaximalSubpartBoundariesTestCase)
{
    const std::string& r = kReplacement;
    check_cases(sanitize_utf8, {
        {"\xE2\x82", r},                          // truncated 3-byte prefix: one unit
        {"\xE2\x82" "A", r + "A"},                // resumes at the breaking byte
        {"\xF0\x9F\x98", r},                      // truncated 4-byte prefix: one unit
        {"\xF0\x9F\x98" + kEAcute, r + kEAcute},  // ...followed by a valid sequence
        {"\xE0\x80\x80", r + r + r},              // overlong 3-byte NUL
        {"\xF0\x80\x80\x80", r + r + r + r},      // overlong 4-byte NUL
        {"\xC1\xBF", r + r},                      // overlong 2-byte
        {"\xF5\x80\x80\x80", r + r + r + r},      // F5 is never a lead
        {"\xFF\xFE", r + r},                      // UTF-16 BOM bytes
        {kEAcute + "\x80", kEAcute + r},          // extra continuation after valid
        {"\xED\x9F\xBF", "\xED\x9F\xBF"},         // U+D7FF, last before surrogates
        {"\xED\xBF\xBF", r + r + r},              // U+DFFF, low surrogate
        {"\xEE\x80\x80", "\xEE\x80\x80"},         // U+E000, first after surrogates
    });
}

/// Edge case: every single byte 0x00-0xFF on its own - ASCII passes,
/// every byte >= 0x80 alone is one U+FFFD.
BOOST_AUTO_TEST_CASE(SanitizeUtf8EverySingleByteTestCase)
{
    for (int b = 0; b <= 0xFF; ++b) {
        const std::string in(1, static_cast<char>(b));
        BOOST_TEST_CONTEXT("byte " << b) {
            BOOST_CHECK_EQUAL(sanitize_utf8(in), b < 0x80 ? in : kReplacement);
        }
    }
}

/// Edge case (property): for arbitrary bytes the result is valid UTF-8,
/// sanitising is idempotent, and every escaper's output is valid UTF-8.
BOOST_AUTO_TEST_CASE(SanitizeUtf8PropertySweepTestCase)
{
    for (const std::string& s : random_byte_strings(20000)) {
        const std::string clean = sanitize_utf8(s);
        BOOST_REQUIRE(smbios::test::is_valid_utf8(clean));
        BOOST_REQUIRE_EQUAL(sanitize_utf8(clean), clean);
        BOOST_REQUIRE_EQUAL(smbios::test::is_valid_utf8(s), clean == s);
        BOOST_REQUIRE(smbios::test::is_valid_utf8(xml_escape(s)));
        BOOST_REQUIRE(smbios::test::is_valid_utf8(json_escape(s)));
        BOOST_REQUIRE(smbios::test::is_valid_utf8(yaml_double_quoted_escape(s)));
    }
}

// ------------------------------------------------------------------ XML ---

/// Normal case: the five specials become the predefined entities; `\` and
/// other ASCII pass through; empty stays empty.
BOOST_AUTO_TEST_CASE(XmlEscapeSpecialCharactersTestCase)
{
    check_cases(xml_escape, {
        {"", ""},
        {"Plain Vendor 1.0", "Plain Vendor 1.0"},
        {"&", "&amp;"},
        {"<", "&lt;"},
        {">", "&gt;"},
        {"\"", "&quot;"},
        {"'", "&apos;"},
        {"\\", "\\"},
        {"A&B <x> \"q\" 'a' \\", "A&amp;B &lt;x&gt; &quot;q&quot; &apos;a&apos; \\"},
        {"&amp;", "&amp;amp;"}, // never double-decoded or skipped
        {"]]>", "]]&gt;"},
    });
}

/// Security case (Task 03 case 2): TAB/LF/CR survive, every other C0 byte
/// and NUL becomes U+FFFD, DEL passes - and no character reference appears.
BOOST_AUTO_TEST_CASE(XmlEscapeReplacesForbiddenControlsTestCase)
{
    std::string expected;
    for (int b = 0x01; b <= 0x1F; ++b) {
        expected += (b == 0x09 || b == 0x0A || b == 0x0D) ? std::string(1, static_cast<char>(b)) : kReplacement;
    }
    expected += '\x7F';
    const std::string out = xml_escape(smbios::test::all_c0_controls_and_del());
    BOOST_CHECK_EQUAL(out, expected);
    BOOST_CHECK(out.find("&#") == std::string::npos);
    BOOST_CHECK_EQUAL(xml_escape(std::string("\0", 1)), kReplacement);
}

/// Security cases (Task 03 cases 1, 3, 4, 5): invalid UTF-8 and ESC become
/// U+FFFD; U+FFFE/U+FFFF become U+FFFD; C1, bidi, U+2028/U+2029 and valid
/// multi-byte characters pass through as data.
BOOST_AUTO_TEST_CASE(XmlEscapeUnicodeAndHostileInputTestCase)
{
    const std::string& r = kReplacement;
    check_cases(xml_escape, {
        {kEAcute + kCjk + kEmoji, kEAcute + kCjk + kEmoji},
        {"Caf\xE9 & Co", "Caf" + r + " &amp; Co"},
        {"\xED\xA0\x80<", r + r + r + "&lt;"},
        {"\x1b[31mRED\x1b[0m", r + "[31mRED" + r + "[0m"},
        {kFFFE + kFFFF, r + r},
        {kRlo + kLri + kPdi + kLineSep + kParaSep, kRlo + kLri + kPdi + kLineSep + kParaSep},
        {kCsi + kNel, kCsi + kNel},
    });
    for (const auto& c : smbios::test::invalid_utf8_cases()) {
        BOOST_CHECK_EQUAL(xml_escape(c.first), c.second);
    }
}

// ----------------------------------------------------------------- JSON ---

/// Normal case: `"` and `\` are escaped; `/`, `&`, `<`, `>`, `'` pass
/// through raw; empty stays empty.
BOOST_AUTO_TEST_CASE(JsonEscapeSpecialCharactersTestCase)
{
    check_cases(json_escape, {
        {"", ""},
        {"Plain Vendor 1.0", "Plain Vendor 1.0"},
        {"\"", "\\\""},
        {"\\", "\\\\"},
        {"&<>'/", "&<>'/"},
        {"say \"hi\" \\o/", "say \\\"hi\\\" \\\\o/"},
    });
}

/// Security case (Task 03 case 2): every C0 byte is escaped - short forms
/// for \b \t \n \f \r, lowercase \u00XX otherwise (ESC -> \u001b); DEL
/// passes raw; NUL -> \u0000.
BOOST_AUTO_TEST_CASE(JsonEscapeEscapesEveryControlTestCase)
{
    const std::string expected =
        "\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\b\\t\\n\\u000b\\f\\r\\u000e\\u000f"
        "\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017\\u0018\\u0019\\u001a\\u001b"
        "\\u001c\\u001d\\u001e\\u001f\x7F";
    const std::string out = json_escape(smbios::test::all_c0_controls_and_del());
    BOOST_CHECK_EQUAL(out, expected);
    BOOST_CHECK(!smbios::test::contains_byte_in_range(out, 0x00, 0x1F));
    BOOST_CHECK_EQUAL(json_escape(std::string("\0", 1)), "\\u0000");
}

/// Security cases (Task 03 cases 1, 3, 4, 5): invalid UTF-8 -> U+FFFD;
/// ANSI ESC escaped; U+2028/U+2029 -> \u2028/\u2029; bidi, C1, U+FFFE and
/// U+FFFF pass through raw; valid multi-byte passes through.
BOOST_AUTO_TEST_CASE(JsonEscapeUnicodeAndHostileInputTestCase)
{
    const std::string& r = kReplacement;
    check_cases(json_escape, {
        {kEAcute + kCjk + kEmoji, kEAcute + kCjk + kEmoji},
        {"Caf\xE9\"", "Caf" + r + "\\\""},
        {"\x1b[31mRED\x1b[0m", "\\u001b[31mRED\\u001b[0m"},
        {kLineSep + "x" + kParaSep, "\\u2028x\\u2029"},
        {kRlo + kLri + kPdi, kRlo + kLri + kPdi},
        {kFFFE + kFFFF, kFFFE + kFFFF},
        {kCsi + kNel, kCsi + kNel},
    });
    for (const auto& c : smbios::test::invalid_utf8_cases()) {
        BOOST_CHECK_EQUAL(json_escape(c.first), c.second);
    }
}

// ----------------------------------------------------------------- YAML ---

/// Normal case: `"` and `\` are escaped; YAML-significant ASCII (`: `,
/// `#`, `&`, `*`, `!`, `%`, leading `-`) is left for the double quotes to
/// neutralise; empty stays empty.
BOOST_AUTO_TEST_CASE(YamlEscapeSpecialCharactersTestCase)
{
    check_cases(yaml_double_quoted_escape, {
        {"", ""},
        {"\"", "\\\""},
        {"\\", "\\\\"},
        {"- a: b # &x *y !t %d", "- a: b # &x *y !t %d"},
        {"NO", "NO"},
    });
}

/// Security case: every C0 byte uses a YAML short form where one exists
/// (\0 \a \b \t \n \v \f \r \e) and lowercase \xXX otherwise; DEL -> \x7f.
BOOST_AUTO_TEST_CASE(YamlEscapeEscapesEveryControlTestCase)
{
    const std::string expected =
        "\\x01\\x02\\x03\\x04\\x05\\x06\\a\\b\\t\\n\\v\\f\\r\\x0e\\x0f"
        "\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1a\\e\\x1c\\x1d\\x1e\\x1f\\x7f";
    const std::string out = yaml_double_quoted_escape(smbios::test::all_c0_controls_and_del());
    BOOST_CHECK_EQUAL(out, expected);
    BOOST_CHECK(!smbios::test::contains_byte_in_range(out, 0x00, 0x1F));
    BOOST_CHECK(!smbios::test::contains_byte_in_range(out, 0x7F, 0x7F));
    BOOST_CHECK_EQUAL(yaml_double_quoted_escape(std::string("\0", 1)), "\\0");
}

/// Security case: C1 -> \xXX (CSI -> \x9b), U+0085/U+2028/U+2029 -> \N \L
/// \P, U+FFFE/U+FFFF -> \ufffe \uffff, invalid UTF-8 -> U+FFFD; bidi and
/// valid multi-byte pass through.
BOOST_AUTO_TEST_CASE(YamlEscapeUnicodeAndHostileInputTestCase)
{
    const std::string& r = kReplacement;
    check_cases(yaml_double_quoted_escape, {
        {kEAcute + kCjk + kEmoji, kEAcute + kCjk + kEmoji},
        {kCsi, "\\x9b"},
        {"\xC2\x80" "\xC2\x9F", "\\x80\\x9f"},
        {kNel + kLineSep + kParaSep, "\\N\\L\\P"},
        {kFFFE + kFFFF, "\\ufffe\\uffff"},
        {"\x1b[31m", "\\e[31m"},
        {kRlo, kRlo},
        {"\xC2\xA0", "\xC2\xA0"}, // U+00A0, first printable after C1
        {"Caf\xE9", "Caf" + r},
    });
    for (const auto& c : smbios::test::invalid_utf8_cases()) {
        BOOST_CHECK_EQUAL(yaml_double_quoted_escape(c.first), c.second);
    }
}

// ------------------------------------------------------------------ hex ---

/// Normal and edge cases: empty -> empty; lowercase, no separators.
BOOST_AUTO_TEST_CASE(HexEncodeTestCase)
{
    BOOST_CHECK_EQUAL(hex_encode({}), "");
    BOOST_CHECK_EQUAL(hex_encode({0x00}), "00");
    BOOST_CHECK_EQUAL(hex_encode({0xDE, 0xAD, 0xBE, 0xEF, 0x0A, 0xFF}), "deadbeef0aff");
}

/// Edge case (Task 03 case 8): a 255-byte vector, the maximum one
/// formatted area can hold, encodes untruncated to 510 lowercase hex
/// characters, in order.
BOOST_AUTO_TEST_CASE(HexEncodeMaximumFormattedAreaTestCase)
{
    std::vector<uint8_t> bytes;
    std::string expected;
    const char* digits = "0123456789abcdef";
    for (int b = 0x00; b <= 0xFE; ++b) {
        bytes.push_back(static_cast<uint8_t>(b));
        expected += digits[b >> 4];
        expected += digits[b & 0xF];
    }
    const std::string out = hex_encode(bytes);
    BOOST_CHECK_EQUAL(out.size(), static_cast<size_t>(510));
    BOOST_CHECK_EQUAL(out, expected);
}

BOOST_AUTO_TEST_SUITE_END()
