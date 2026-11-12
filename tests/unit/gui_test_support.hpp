#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <smbios/gui/view_model.hpp>

#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

// Shared test-support helpers for the smbios_gui view-model unit tests
// (milestone 0011, Story 02.0 Task 04): gui_view_model_test.cpp,
// gui_field_text_test.cpp and gui_copy_as_test.cpp. Header-only and
// test-only - never included by smbios_gui/ or libsmbios/ production code.
//
// Every value these helpers build is synthetic (threat model
// docs/security/2026-11-11-gui-browsing-ui.md, requirement 20): no fixture
// in these suites carries a real machine's serial number, UUID or asset tag.

namespace smbios::test::gui {

/// @brief Feeds `buffer` through SMBios's real, bounds-checked table walk
/// via the SmbiosTableWalkTestAccess friend seam, replacing whatever the
/// instance's own constructor read from the live machine.
/// @param instance an already-constructed SMBios.
/// @param buffer the synthetic structure table (may be empty or malformed).
inline void load_table(SMBios& instance, std::vector<uint8_t> buffer)
{
    SmbiosTableWalkTestAccess::parse_synthetic_table(instance, std::move(buffer));
}

/// @brief Counts the structures [bios.begin(), bios.end()) yields, with a
/// manual loop: SMBios::iterator has no std::iterator_traits, so
/// std::distance() does not compile on it (Task 01's as-built correction).
/// @param bios the table to count.
/// @return the number of iterable structures.
inline size_t count_structures(SMBios& bios)
{
    size_t count = 0;
    for (auto it = bios.begin(); it != bios.end(); ++it) {
        ++count;
    }
    return count;
}

/// @brief One OEM Strings (type 11) structure's raw bytes, with an explicit
/// handle: header, a one-byte formatted area holding the string count, then
/// the string section and its double-NUL terminator.
///
/// Built by hand rather than via SyntheticTableBuilder::add_structure() so
/// callers can pick the handle (add_structure() assigns handles
/// sequentially). OEM Strings is used because its get_fields() returns every
/// DMI string verbatim, one field per string ("String 1", "String 2", ...).
/// @param handle structure handle, written little-endian.
/// @param strings the strings, in index order; at most 255.
/// @return the structure's bytes, ready for SyntheticTableBuilder::add_raw_bytes().
inline std::vector<uint8_t> oem_strings_structure(uint16_t handle, const std::vector<std::string>& strings)
{
    std::vector<uint8_t> bytes{
        static_cast<uint8_t>(SMBios::OemStrings), 5,
        static_cast<uint8_t>(handle & 0xFF), static_cast<uint8_t>(handle >> 8),
        static_cast<uint8_t>(strings.size())};
    for (const std::string& text : strings) {
        bytes.insert(bytes.end(), text.begin(), text.end());
        bytes.push_back(0);
    }
    if (strings.empty()) {
        bytes.push_back(0);
    }
    bytes.push_back(0);
    return bytes;
}

/// @brief The 4-byte EndOfTable marker plus its empty string section, for
/// tables assembled with SyntheticTableBuilder::add_raw_bytes() (which turns
/// off build()'s automatic marker).
inline std::vector<uint8_t> end_of_table_structure(uint16_t handle)
{
    return {static_cast<uint8_t>(SMBios::EndOfTable), 4,
            static_cast<uint8_t>(handle & 0xFF), static_cast<uint8_t>(handle >> 8), 0, 0};
}

/// @brief Strict UTF-8 validator (RFC 3629): rejects stray continuation
/// bytes, truncated sequences, overlong encodings, UTF-16 surrogates
/// (U+D800-U+DFFF) and code points above U+10FFFF.
/// @param text bytes to check.
/// @param[out] code_points every decoded code point, in order (only
/// meaningful when the result is true).
/// @return true iff `text` is entirely valid UTF-8.
inline bool decode_strict_utf8(const std::string& text, std::vector<uint32_t>& code_points)
{
    code_points.clear();
    size_t pos = 0;
    while (pos < text.size()) {
        const auto lead = static_cast<unsigned char>(text[pos]);
        size_t length = 0;
        uint32_t code_point = 0;
        uint32_t minimum = 0;
        if (lead < 0x80) {
            length = 1;
            code_point = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            length = 2;
            code_point = lead & 0x1F;
            minimum = 0x80;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            code_point = lead & 0x0F;
            minimum = 0x800;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            code_point = lead & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (pos + length > text.size()) {
            return false;
        }
        for (size_t i = 1; i < length; ++i) {
            const auto next = static_cast<unsigned char>(text[pos + i]);
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (next & 0x3F);
        }
        if (length > 1 && code_point < minimum) {
            return false;
        }
        if ((code_point >= 0xD800 && code_point <= 0xDFFF) || code_point > 0x10FFFF) {
            return false;
        }
        code_points.push_back(code_point);
        pos += length;
    }
    return true;
}

/// @brief True iff display_text() must never emit `code_point` (threat
/// model G-4): C0 (U+0000-U+001F), DEL, C1 (U+0080-U+009F), U+2028/U+2029,
/// U+202A-U+202E and U+2066-U+2069. Restated here independently of
/// view_model.cpp so the test does not trust the code under test.
inline bool is_forbidden_display_code_point(uint32_t code_point)
{
    return code_point <= 0x1F
        || (code_point >= 0x7F && code_point <= 0x9F)
        || (code_point >= 0x2028 && code_point <= 0x202E)
        || (code_point >= 0x2066 && code_point <= 0x2069);
}

/// @brief Asserts every display_text() guarantee on one *output* `shown`
/// (threat model requirement 15): valid UTF-8, no forbidden code point, at
/// most smbios::gui::kDisplayTextMaxBytes bytes, and a fixed point of
/// display_text() - which is exactly display_text(display_text(s)) ==
/// display_text(s) when `shown` is display_text(s).
/// Failure messages name `what` and offsets/sizes only, never the text
/// itself (it may be huge or contain terminal control bytes).
/// @param shown a display_text() / field_value_text() / entry_node_label() result.
/// @param what short description of the input, for failure messages.
inline void check_display_safe(const std::string& shown, const std::string& what)
{
    std::vector<uint32_t> code_points;
    BOOST_CHECK_MESSAGE(decode_strict_utf8(shown, code_points), what << ": output is not valid UTF-8");
    for (size_t i = 0; i < code_points.size(); ++i) {
        BOOST_CHECK_MESSAGE(!is_forbidden_display_code_point(code_points[i]),
                            what << ": forbidden code point U+" << std::hex << code_points[i]
                                 << std::dec << " at code point #" << i);
    }
    BOOST_CHECK_MESSAGE(shown.size() <= smbios::gui::kDisplayTextMaxBytes,
                        what << ": output is " << shown.size() << " bytes, cap is "
                             << smbios::gui::kDisplayTextMaxBytes);
    BOOST_CHECK_MESSAGE(smbios::gui::display_text(shown) == shown,
                        what << ": display_text() is not idempotent on this output");
}

} // namespace smbios::test::gui
