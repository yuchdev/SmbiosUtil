#pragma once
#include <cstddef>

#include <smbios/gui/view_model.hpp>

/// @file copy_as_action.hpp
/// @brief Dear ImGui "Copy as..." widget of smbios_gui's field-table pane
/// (milestone 0011, Story 02.0 Task 03). Executable-only; its ImGui-free
/// logic lives in copy_as.hpp.

namespace smbios {
class SMBios;
} // namespace smbios

namespace smbios::gui {

/// @brief Fixed note shown next to the Copy button whenever the selected
/// format is not terminal-safe (threat model G-5).
inline constexpr const char* kCopyAsUnsafeFormatNote =
    "Output may contain raw control characters - paste into terminals with care.";

/// @brief Renders a format dropdown (exactly
/// smbios::registered_output_formatter_names(), sorted, defaulting to
/// kDefaultCopyFormat) plus a "Copy" button for one node.
///
/// Only on a click, the node's entry is re-created from `bios`
/// (create_entry_for_node(), scoped to the click handler - never per frame,
/// threat model G-7), rendered with render_entry_as(), and the result is
/// passed byte-exact to ImGui::SetClipboardText(). Output over
/// kCopyAsMaxBytes is refused. Outcomes are reported with fixed status text
/// only ("Copied to clipboard.", "Nothing to copy.", "Entry too large to
/// copy.", "Copy failed."); no field value, rendered output or exception
/// text is ever displayed, logged or printed (G-10).
///
/// The remembered format is an index into a function-local copy of the
/// (immutable after static init) registry names - never a pointer into a
/// temporary (G-10). Every ImGui label/ID here is a string literal; format
/// names are drawn via ImGui::TextUnformatted() (and as the combo preview,
/// which ImGui draws verbatim and never parses as a format string or ID).
///
/// Must be called inside an ImGui window of the current frame.
///
/// noexcept: every exception from the click handler is caught and reported
/// as "Copy failed." except std::bad_alloc, which terminates the process
/// rather than unwind out of a half-built ImGui frame (G-6).
/// @param bios the table `node` was built from; must outlive this call.
/// @param node the selected node.
/// @param node_index `node`'s position in build_entry_tree()'s result.
void render_copy_as_action(smbios::SMBios& bios, const EntryNode& node, size_t node_index) noexcept;

} // namespace smbios::gui
