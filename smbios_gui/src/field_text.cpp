/// @file field_text.cpp
/// @brief Implementation of smbios_gui's ImGui-free on-screen text
/// projections (field_text.hpp). Deliberately includes no Dear ImGui header.

#include <smbios/gui/field_text.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <variant>
#include <vector>

#include <libsmbios/formatters/formatter_escaping.hpp>

namespace smbios::gui {

namespace {

/// @brief The classic "overloaded lambdas" helper for std::visit.
template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/// @brief Formats `n` as `0x` + uppercase hex digits, no leading zeros.
std::string to_hex_text(uint64_t n)
{
    char buffer[2 + 16 + 1] = {};
    std::snprintf(buffer, sizeof(buffer), "0x%llX", static_cast<unsigned long long>(n));
    return std::string(buffer);
}

/// @brief Space-separated hex dump of at most kHexDumpMaxBytes of `bytes`,
/// reusing smbios::hex_encode() for the digit encoding.
std::string to_hex_dump(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty()) {
        return "(empty)";
    }
    const size_t shown = bytes.size() < kHexDumpMaxBytes ? bytes.size() : kHexDumpMaxBytes;
    const std::vector<uint8_t> head(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(shown));
    const std::string digits = smbios::hex_encode(head);

    std::string dump;
    dump.reserve(shown * 3 + 32);
    for (size_t i = 0; i + 1 < digits.size(); i += 2) {
        if (i != 0) {
            dump.push_back(' ');
        }
        dump.push_back(digits[i]);
        dump.push_back(digits[i + 1]);
    }
    if (shown < bytes.size()) {
        dump += " ... [";
        dump += std::to_string(bytes.size() - shown);
        dump += " more bytes]";
    }
    return dump;
}

} // namespace

std::string field_value_text(const smbios::FieldValue& value, bool hex)
{
    // Every branch funnels through display_text() so the result carries its
    // guarantees unconditionally (for the ASCII-only branches it is a cheap
    // no-op). The string branch passes the raw value by reference: no extra
    // copy of a possibly table-sized firmware string.
    return std::visit(overloaded{
                          [](const std::string& s) { return display_text(s); },
                          [hex](uint64_t n) { return display_text(hex ? to_hex_text(n) : std::to_string(n)); },
                          [](bool b) { return display_text(b ? "true" : "false"); },
                          [](const std::vector<uint8_t>& bytes) { return display_text(to_hex_dump(bytes)); },
                      },
                      value);
}

std::string entry_node_label(const EntryNode& node)
{
    char handle_text[sizeof(" (handle 0xFFFF)")] = {};
    std::snprintf(handle_text, sizeof(handle_text), " (handle 0x%04X)", static_cast<unsigned>(node.handle));

    std::string label = display_text(node.label);
    label += handle_text;
    if (node.decode_failed) {
        label += ' ';
        label += kDecodeFailedText;
    }
    return label;
}

} // namespace smbios::gui
