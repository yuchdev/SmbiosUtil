#include <cstddef>
#include <ctime>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <libsmbios/field_value.hpp>
#include <smbios/gui/field_text.hpp>
#include <smbios/gui/view_model.hpp>

#include "gui_test_support.hpp"

#include <boost/test/unit_test.hpp>

using smbios::FieldValue;
using smbios::gui::display_text;
using smbios::gui::EntryNode;
using smbios::gui::kDisplayTextMaxBytes;

// Unit tests (milestone 0011, Story 02.0 Task 04)
//
// Type: Unit - deterministic, pure functions, no SMBIOS table, no
// hardware/privileges/display; no Dear ImGui linked.
// Scenario: the on-screen text projections smbios_gui's renderer hands to
// ImGui::TextUnformatted() - display_text() (view_model.hpp; threat model
// docs/security/2026-11-11-gui-browsing-ui.md G-3/G-4), field_value_text()
// and entry_node_label() (field_text.hpp). Requirement 15's end-to-end
// hostile-table case lives in gui_view_model_test.cpp; this file pins the
// exact boundaries: each edge of every replaced code-point range, the
// 4096-byte cap (including suffix) with and without replacement growth,
// and each FieldValue alternative's rendering.
// Boundaries: replaced ranges U+0000-U+001F, U+007F-U+009F, U+2028-U+202E,
// U+2066-U+2069 - each first/last member replaced, each neighbour kept;
// inputs of exactly kDisplayTextMaxBytes and one byte more; hex dumps of
// exactly kHexDumpMaxBytes and one byte more; uint64_t 0 and max; 16 MiB
// inputs (bounded per-frame cost, security review P-1).
// On failure first check: smbios_gui/src/view_model.cpp's
// is_display_unsafe()/truncation, smbios_gui/src/field_text.cpp.

namespace {

/// U+FFFD REPLACEMENT CHARACTER, UTF-8 encoded.
const std::string kFffd = "\xEF\xBF\xBD";

/// @brief UTF-8 encoding of one code point (test-local, independent of the
/// code under test).
std::string utf8(uint32_t code_point)
{
    std::string out;
    if (code_point < 0x80) {
        out.push_back(static_cast<char>(code_point));
    } else if (code_point < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    return out;
}

/// @brief Splits a truncated display_text() result into its kept prefix
/// length and the N of " ... [N bytes truncated]"; requires the suffix.
void split_truncated(const std::string& shown, size_t& kept, size_t& dropped)
{
    const std::string marker = " ... [";
    const std::string tail = " bytes truncated]";
    BOOST_REQUIRE(shown.size() > tail.size());
    BOOST_REQUIRE(shown.compare(shown.size() - tail.size(), tail.size(), tail) == 0);
    kept = shown.rfind(marker);
    BOOST_REQUIRE(kept != std::string::npos);
    const size_t digits_begin = kept + marker.size();
    dropped = std::stoull(shown.substr(digits_begin, shown.size() - tail.size() - digits_begin));
}

/// @brief Very generous CPU-time ceiling for the bounded-cost cases below:
/// orders of magnitude above the bounded implementation's cost even under
/// Debug + ASan, and far below what re-reading 16 MiB per call costs.
/// Process CPU time (std::clock), not wall-clock, so a loaded CI host does
/// not make the check flaky.
constexpr double kGenerousCpuSeconds = 5.0;

/// @brief Process CPU seconds spent running `work`.
template <typename Work>
double cpu_seconds_for(Work&& work)
{
    const std::clock_t start = std::clock();
    work();
    return static_cast<double>(std::clock() - start) / CLOCKS_PER_SEC;
}

} // namespace

BOOST_AUTO_TEST_SUITE(GuiFieldTextUnitTests)

//////////////////////////////////////////////////////////////////////////
// display_text()

/// Boundary case: the empty string and plain printable ASCII (every byte
/// 0x20-0x7E) pass through unchanged.
BOOST_AUTO_TEST_CASE(DisplayTextKeepsPrintableAsciiTestCase)
{
    BOOST_CHECK_EQUAL(display_text(""), "");
    std::string printable;
    for (int c = 0x20; c <= 0x7E; ++c) {
        printable.push_back(static_cast<char>(c));
    }
    BOOST_CHECK_EQUAL(display_text(printable), printable);
}

/// Boundary case: the first and last code point of every replaced range
/// become U+FFFD; the code point just outside each range is kept.
BOOST_AUTO_TEST_CASE(DisplayTextReplacedRangeEdgesTestCase)
{
    for (uint32_t replaced : {0x00u, 0x1Fu, 0x7Fu, 0x80u, 0x9Fu, 0x2028u, 0x2029u, 0x202Au, 0x202Eu, 0x2066u, 0x2069u}) {
        BOOST_TEST_CONTEXT("U+" << std::hex << replaced)
        {
            BOOST_CHECK(display_text("a" + utf8(replaced) + "b") == "a" + kFffd + "b");
        }
    }
    for (uint32_t kept : {0x20u, 0x7Eu, 0xA0u, 0x2027u, 0x202Fu, 0x2065u, 0x206Au, 0xFFFDu, 0x1F600u}) {
        BOOST_TEST_CONTEXT("U+" << std::hex << kept)
        {
            const std::string text = "a" + utf8(kept) + "b";
            BOOST_CHECK(display_text(text) == text);
        }
    }
}

/// Boundary case: an embedded NUL (impossible in a DMI string, possible in a
/// std::string) is C0 and is replaced, not treated as an end of string.
BOOST_AUTO_TEST_CASE(DisplayTextReplacesEmbeddedNulTestCase)
{
    const std::string with_nul("a\0b", 3);
    BOOST_CHECK(display_text(with_nul) == "a" + kFffd + "b");
}

/// Boundary case: exactly kDisplayTextMaxBytes bytes are kept whole; one
/// more byte triggers truncation, and the result (suffix included) never
/// exceeds the cap; kept + dropped == the input size.
BOOST_AUTO_TEST_CASE(DisplayTextCapBoundaryTestCase)
{
    const std::string at_cap(kDisplayTextMaxBytes, 'a');
    BOOST_CHECK(display_text(at_cap) == at_cap);

    const std::string over_cap(kDisplayTextMaxBytes + 1, 'a');
    const std::string shown = display_text(over_cap);
    smbios::test::gui::check_display_safe(shown, "cap + 1");
    size_t kept = 0;
    size_t dropped = 0;
    split_truncated(shown, kept, dropped);
    BOOST_CHECK_EQUAL(kept + dropped, over_cap.size());
    BOOST_CHECK(shown.compare(0, kept, over_cap, 0, kept) == 0);
    BOOST_CHECK(shown.size() <= kDisplayTextMaxBytes);
}

/// Boundary case: replacement growth past the cap - 4096 control bytes
/// project to 12288 bytes of U+FFFD, which must be cut on a code-point
/// boundary (a whole number of U+FFFD kept) and still fit the cap. N counts
/// *input* bytes not shown (view_model.hpp, "Bounded cost"): each kept
/// U+FFFD stands for one input newline, so kept / 3 + N == the input size.
BOOST_AUTO_TEST_CASE(DisplayTextCapAfterReplacementGrowthTestCase)
{
    const std::string controls(kDisplayTextMaxBytes, '\n');
    const std::string shown = display_text(controls);
    smbios::test::gui::check_display_safe(shown, "4096 newlines");
    size_t kept = 0;
    size_t dropped = 0;
    split_truncated(shown, kept, dropped);
    BOOST_CHECK_EQUAL(kept % kFffd.size(), static_cast<size_t>(0));
    BOOST_CHECK_EQUAL(kept / kFffd.size() + dropped, controls.size());
    for (size_t pos = 0; pos < kept; pos += kFffd.size()) {
        BOOST_CHECK(shown.compare(pos, kFffd.size(), kFffd) == 0);
    }
}

/// Boundary case: a 4-byte code point straddling the cut is dropped whole,
/// never split, for every alignment of the cut within it.
BOOST_AUTO_TEST_CASE(DisplayTextNeverSplitsCodePointTestCase)
{
    for (size_t pad = 0; pad < 4; ++pad) {
        std::string text(pad, 'p');
        while (text.size() < kDisplayTextMaxBytes * 2) {
            text += utf8(0x1F600);
        }
        BOOST_TEST_CONTEXT("pad " << pad)
        {
            const std::string shown = display_text(text);
            smbios::test::gui::check_display_safe(shown, "emoji run");
            size_t kept = 0;
            size_t dropped = 0;
            split_truncated(shown, kept, dropped);
            BOOST_CHECK_EQUAL((kept - pad) % 4, static_cast<size_t>(0));
            BOOST_CHECK_EQUAL(kept + dropped, text.size());
        }
    }
}

/// Regression case (security review P-1, CWE-400): display_text() runs per
/// visible row per frame, so its cost must not grow with the input. A
/// 16 MiB hostile string (C0, bidi, invalid UTF-8, 4-byte code points)
/// must (a) satisfy every output invariant, (b) produce exactly the same
/// kept text as its 64 KiB prefix - only N differs, by the 16 MiB - 64 KiB
/// input bytes appended - i.e. the tail is never needed, and (c) finish
/// 50 calls well inside a very generous CPU-time bound (an unbounded
/// implementation sanitises 800 MiB here and takes far longer, even in a
/// Release build; the bounded one reads about 4 KiB per call).
BOOST_AUTO_TEST_CASE(DisplayTextBoundedCostOnHugeInputTestCase)
{
    const std::string pattern = std::string("A\x1B[2J\n\xC3\xA9\xFF\x80\xED\xA0\x80", 13) + utf8(0x202E)
                              + utf8(0x1F600) + std::string("\0z", 2) + "\xF0\x9F";
    const size_t huge_size = size_t{16} * 1024 * 1024;
    std::string huge;
    huge.reserve(huge_size + pattern.size());
    while (huge.size() < huge_size) {
        huge += pattern;
    }
    huge.resize(huge_size);
    const std::string prefix = huge.substr(0, 64 * 1024);

    const std::string shown = display_text(huge);
    smbios::test::gui::check_display_safe(shown, "16 MiB hostile string");
    size_t kept = 0;
    size_t dropped = 0;
    split_truncated(shown, kept, dropped);
    BOOST_CHECK(dropped < huge.size());

    const std::string shown_prefix = display_text(prefix);
    size_t kept_prefix = 0;
    size_t dropped_prefix = 0;
    split_truncated(shown_prefix, kept_prefix, dropped_prefix);
    BOOST_CHECK_EQUAL(kept, kept_prefix);
    BOOST_CHECK(shown.compare(0, kept, shown_prefix, 0, kept_prefix) == 0);
    BOOST_CHECK_EQUAL(dropped - dropped_prefix, huge.size() - prefix.size());

    constexpr int kCalls = 50;
    const double cpu_seconds = cpu_seconds_for([&] {
        for (int i = 0; i < kCalls; ++i) {
            BOOST_REQUIRE(display_text(huge).size() == shown.size());
        }
    });
    BOOST_TEST_MESSAGE("display_text() x " << kCalls << " on 16 MiB: " << cpu_seconds << " s CPU");
    BOOST_CHECK_MESSAGE(cpu_seconds < kGenerousCpuSeconds,
                        kCalls << " display_text() calls on 16 MiB took " << cpu_seconds << " s CPU");
}

//////////////////////////////////////////////////////////////////////////
// field_value_text()

/// Normal case: std::string goes through display_text(), raw stays raw.
BOOST_AUTO_TEST_CASE(FieldValueTextStringUsesDisplayTextTestCase)
{
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{std::string("Synthetic Vendor")}, false),
                      "Synthetic Vendor");
    const std::string hostile = "\x1B[2J\n%s";
    BOOST_CHECK(smbios::gui::field_value_text(FieldValue{hostile}, false) == display_text(hostile));
    BOOST_CHECK(smbios::gui::field_value_text(FieldValue{hostile}, true) == display_text(hostile));
}

/// Normal + boundary case: uint64_t in decimal and in hex (0x, uppercase,
/// no leading zeros), at 0, a typical value and the maximum.
BOOST_AUTO_TEST_CASE(FieldValueTextNumberDecimalAndHexTestCase)
{
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{uint64_t{31}}, false), "31");
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{uint64_t{31}}, true), "0x1F");
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{uint64_t{0}}, false), "0");
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{uint64_t{0}}, true), "0x0");
    const uint64_t max = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{max}, false), "18446744073709551615");
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{max}, true), "0xFFFFFFFFFFFFFFFF");
}

/// Normal case: bool renders "true"/"false"; the hex toggle is ignored.
BOOST_AUTO_TEST_CASE(FieldValueTextBoolTestCase)
{
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{true}, false), "true");
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{false}, false), "false");
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{true}, true), "true");
}

/// Normal + boundary case: bytes render as space-separated hex pairs;
/// empty is "(empty)"; exactly kHexDumpMaxBytes bytes dump whole; one more
/// appends " ... [1 more bytes]"; the result always fits the display cap.
BOOST_AUTO_TEST_CASE(FieldValueTextBytesHexDumpTestCase)
{
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}}, false),
                      "de ad be ef");
    BOOST_CHECK_EQUAL(smbios::gui::field_value_text(FieldValue{std::vector<uint8_t>{}}, false), "(empty)");

    const std::vector<uint8_t> at_cap(smbios::gui::kHexDumpMaxBytes, 0x0A);
    const std::string dump_at_cap = smbios::gui::field_value_text(FieldValue{at_cap}, false);
    BOOST_CHECK_EQUAL(dump_at_cap.size(), smbios::gui::kHexDumpMaxBytes * 3 - 1);
    BOOST_CHECK(dump_at_cap.find("more bytes") == std::string::npos);
    smbios::test::gui::check_display_safe(dump_at_cap, "hex dump at cap");

    std::vector<uint8_t> over_cap(smbios::gui::kHexDumpMaxBytes + 1, 0x0A);
    const std::string dump_over_cap = smbios::gui::field_value_text(FieldValue{over_cap}, false);
    BOOST_CHECK(dump_over_cap == dump_at_cap + " ... [1 more bytes]");
    smbios::test::gui::check_display_safe(dump_over_cap, "hex dump over cap");

    const std::vector<uint8_t> huge(64 * 1024, 0xFF);
    smbios::test::gui::check_display_safe(smbios::gui::field_value_text(FieldValue{huge}, true), "64 KiB hex dump");
}

/// Regression case (security review P-1, CWE-400): field_value_text() runs
/// per visible row per frame; a 16 MiB byte vector must only ever encode
/// the first kHexDumpMaxBytes (same text as a kHexDumpMaxBytes-long vector
/// plus the " ... [N more bytes]" suffix, N exact) and 50 calls must stay
/// well inside a very generous CPU-time bound. A 16 MiB string value
/// takes the display_text() path covered by
/// DisplayTextBoundedCostOnHugeInputTestCase; checked here end to end too.
BOOST_AUTO_TEST_CASE(FieldValueTextBoundedCostOnHugeInputTestCase)
{
    const size_t huge_size = size_t{16} * 1024 * 1024;
    const FieldValue huge_bytes{std::vector<uint8_t>(huge_size, 0x1B)};
    const FieldValue capped_bytes{std::vector<uint8_t>(smbios::gui::kHexDumpMaxBytes, 0x1B)};
    const std::string dump = smbios::gui::field_value_text(huge_bytes, false);
    smbios::test::gui::check_display_safe(dump, "16 MiB hex dump");
    BOOST_CHECK(dump == smbios::gui::field_value_text(capped_bytes, false) + " ... ["
                            + std::to_string(huge_size - smbios::gui::kHexDumpMaxBytes) + " more bytes]");

    const FieldValue huge_string{std::string(huge_size, '\x1B')};
    const std::string shown = smbios::gui::field_value_text(huge_string, false);
    smbios::test::gui::check_display_safe(shown, "16 MiB ESC string");
    size_t kept = 0;
    size_t dropped = 0;
    split_truncated(shown, kept, dropped);
    BOOST_CHECK_EQUAL(kept / kFffd.size() + dropped, huge_size);

    constexpr int kCalls = 50;
    const double cpu_seconds = cpu_seconds_for([&] {
        for (int i = 0; i < kCalls; ++i) {
            BOOST_REQUIRE(smbios::gui::field_value_text(huge_bytes, false).size() == dump.size());
            BOOST_REQUIRE(smbios::gui::field_value_text(huge_string, true).size() == shown.size());
        }
    });
    BOOST_TEST_MESSAGE("field_value_text() x 2 x " << kCalls << " on 16 MiB: " << cpu_seconds << " s CPU");
    BOOST_CHECK_MESSAGE(cpu_seconds < kGenerousCpuSeconds,
                        kCalls << " x 2 field_value_text() calls on 16 MiB took " << cpu_seconds << " s CPU");
}

//////////////////////////////////////////////////////////////////////////
// entry_node_label()

/// Normal case: "<label> (handle 0xHHHH)", four uppercase hex digits, at
/// both handle extremes.
BOOST_AUTO_TEST_CASE(EntryNodeLabelFormatTestCase)
{
    EntryNode node;
    node.label = "BIOS Information";
    node.handle = 0x001F;
    BOOST_CHECK_EQUAL(smbios::gui::entry_node_label(node), "BIOS Information (handle 0x001F)");
    node.handle = 0x0000;
    BOOST_CHECK_EQUAL(smbios::gui::entry_node_label(node), "BIOS Information (handle 0x0000)");
    node.handle = 0xFFFF;
    BOOST_CHECK_EQUAL(smbios::gui::entry_node_label(node), "BIOS Information (handle 0xFFFF)");
}

/// Error-state case: decode_failed appends " [decode failed]".
BOOST_AUTO_TEST_CASE(EntryNodeLabelDecodeFailedTestCase)
{
    EntryNode node;
    node.label = "Unknown (0xF3)";
    node.handle = 0x0002;
    node.decode_failed = true;
    BOOST_CHECK_EQUAL(smbios::gui::entry_node_label(node),
                      std::string("Unknown (0xF3) (handle 0x0002) ") + smbios::gui::kDecodeFailedText);
}

/// Security case: even a (normally fixed) label is passed through
/// display_text(), so the result stays safe.
BOOST_AUTO_TEST_CASE(EntryNodeLabelSanitizesLabelTestCase)
{
    EntryNode node;
    node.label = "a\nb";
    node.handle = 0x0001;
    const std::string shown = smbios::gui::entry_node_label(node);
    BOOST_CHECK(shown == "a" + kFffd + "b (handle 0x0001)");
    smbios::test::gui::check_display_safe(shown, "label with newline");
}

BOOST_AUTO_TEST_SUITE_END()
