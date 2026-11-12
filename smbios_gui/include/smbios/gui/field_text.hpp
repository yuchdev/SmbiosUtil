#pragma once
#include <cstddef>
#include <string>

#include <libsmbios/field_value.hpp>
#include <smbios/gui/view_model.hpp>

/// @file field_text.hpp
/// @brief Pure, Dear-ImGui-free text projections used by smbios_gui's
/// renderer (milestone 0011, Story 02.0 Task 02).
///
/// Every function here returns on-screen text that is already safe to hand
/// to ImGui::TextUnformatted(): valid UTF-8, free of the control and bidi
/// code points display_text() removes, and at most kDisplayTextMaxBytes
/// bytes (threat model `docs/security/2026-11-11-gui-browsing-ui.md`, G-3 and
/// G-4). None of the results may ever be used as an ImGui format string,
/// widget label or widget ID (G-1). Lives in the GUI-free
/// `libsmbios_gui_model` library so the unit suite (Task 04) can test it.

namespace smbios::gui {

/// @brief Maximum number of bytes field_value_text() hex-dumps from one
/// std::vector<uint8_t> value before appending a fixed truncation notice.
///
/// Each byte becomes three characters ("xx "), so this keeps the dump plus
/// its notice well inside kDisplayTextMaxBytes.
inline constexpr size_t kHexDumpMaxBytes = 1024;

/// @brief Fixed status text for a node whose entry class threw while
/// decoding (EntryNode::decode_failed); contains no firmware-derived data.
inline constexpr const char* kDecodeFailedText = "[decode failed]";

/// @brief Renders one FieldValue as on-screen text, handling every
/// alternative explicitly via std::visit.
///
/// - `std::string` -> display_text() of the raw value;
/// - `uint64_t` -> exact decimal digits, or, when `hex` is true, `0x`
///   followed by uppercase hex digits (no leading zeros, e.g. `0x1F`);
/// - `bool` -> `"true"` or `"false"` (the renderer normally shows a disabled
///   checkbox instead; this text is the non-widget fallback);
/// - `std::vector<uint8_t>` -> smbios::hex_encode() digit pairs separated by
///   single spaces (e.g. `"de ad be ef"`), `"(empty)"` for no bytes; beyond
///   kHexDumpMaxBytes bytes the dump is cut and ` ... [N more bytes]` is
///   appended.
///
/// The result always satisfies display_text()'s guarantees (valid UTF-8, no
/// control/bidi code points, at most kDisplayTextMaxBytes bytes).
/// @param value the raw field value from a FieldRow.
/// @param hex true to show `uint64_t` values in hex; ignored otherwise.
/// @return safe on-screen text for `value`.
/// @throws std::bad_alloc only.
std::string field_value_text(const smbios::FieldValue& value, bool hex);

/// @brief Builds a node's visible list label:
/// `"{display_text(label)} (handle 0xHHHH)"`, with ` [decode failed]`
/// appended when EntryNode::decode_failed is set.
///
/// The handle is not unique on a hostile table, so this text is for display
/// only - renderers must key widget IDs on the node index (threat model G-2).
/// @param node the node to label.
/// @return safe on-screen text; the handle is four uppercase hex digits.
/// @throws std::bad_alloc only.
std::string entry_node_label(const EntryNode& node);

} // namespace smbios::gui
