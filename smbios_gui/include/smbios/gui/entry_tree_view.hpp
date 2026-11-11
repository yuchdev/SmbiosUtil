#pragma once
#include <vector>

#include <smbios/gui/view_model.hpp>

/// @file entry_tree_view.hpp
/// @brief Dear ImGui rendering of smbios_gui's view model (milestone 0011,
/// Story 02.0 Task 02): a node list plus the selected node's field table.

namespace smbios {
class SMBios;
} // namespace smbios

namespace smbios::gui {

/// @brief Fixed text shown instead of the browser when no SMBIOS table could
/// be read (`nodes.empty()`). Never advises running as root (threat model
/// G-8).
inline constexpr const char* kNoTableText =
    "No SMBIOS table could be read (this source may need elevated privileges).";

/// @brief Renders `nodes` as a selectable list (left pane) plus the selected
/// node's field table (right pane), using ImGui calls only. `nodes` is the
/// only input to the display; `bios` is touched solely by the selected
/// node's "Copy as..." action (render_copy_as_action()), and only when the
/// user clicks Copy - never per frame (threat model G-7).
///
/// Must be called once per frame, inside an existing ImGui window of the
/// current frame (between ImGui::NewFrame() and ImGui::Render(), and between
/// that window's Begin()/End()). If `nodes` is empty, renders kNoTableText
/// and nothing else.
///
/// Safety (threat model `docs/security/2026-11-11-gui-browsing-ui.md`):
/// every firmware-derived string reaches ImGui only via
/// ImGui::TextUnformatted(), after display_text() (G-1, G-4); every node and
/// field row is scoped by ImGui::PushID(index), never by handle (G-2); the
/// node list and long field tables use ImGuiListClipper (G-3); no widget is
/// editable - booleans are disabled checkboxes (G-11).
///
/// noexcept: the only exception the body can raise is std::bad_alloc while
/// building a visible row's text. Unwinding out of a half-built ImGui frame
/// would leave the context inconsistent (G-6), so allocation failure here
/// terminates the process instead.
/// @param bios the table `nodes` was built from; must outlive this call.
/// @param nodes the current table's view model, from build_entry_tree(bios).
/// @param[in,out] selected_index index into `nodes` currently shown in the
/// field-table pane; reset to 0 if negative or not less than `nodes.size()`
/// (e.g. after a table refresh), and updated when the user clicks a node.
void render_entry_browser(smbios::SMBios& bios, const std::vector<EntryNode>& nodes,
                          int& selected_index) noexcept;

} // namespace smbios::gui
