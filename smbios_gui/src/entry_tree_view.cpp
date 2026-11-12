/// @file entry_tree_view.cpp
/// @brief Dear ImGui rendering of smbios_gui's view model: render_entry_browser()
/// (milestone 0011, Story 02.0 Task 02).
///
/// Firmware-derived text rule (threat model G-1): every string that came from
/// the table - node labels, field names, field values - is produced by the
/// ImGui-free helpers in field_text.hpp/view_model.hpp (display_text() and
/// friends) and is passed to ImGui *only* as ImGui::TextUnformatted()'s
/// argument. Every widget label/ID and every format string below is a string
/// literal from this file.

#include <smbios/gui/entry_tree_view.hpp>

#include <climits>
#include <cstddef>
#include <string>
#include <variant>

#include <imgui.h>

#include <smbios/gui/copy_as_action.hpp>
#include <smbios/gui/field_text.hpp>

namespace smbios::gui {

namespace {

/// Field tables longer than this use ImGuiListClipper (threat model G-3).
/// Clipped rows are not text-wrapped, so every row keeps the uniform height
/// the clipper assumes.
constexpr size_t kFieldClipperThreshold = 256;

/// Default width of the node-list pane, as a fraction of the window width.
constexpr float kListPaneFraction = 0.35F;

/// Minimum default width of the node-list pane, in pixels.
constexpr float kListPaneMinWidth = 220.0F;

/// Text colour of a node whose decoding failed (EntryNode::decode_failed).
constexpr ImVec4 kDecodeFailedColor(1.00F, 0.55F, 0.35F, 1.00F);

/// @brief Clamps a container size to the int range ImGui's index APIs use.
int clamp_to_int(size_t n)
{
    return n > static_cast<size_t>(INT_MAX) ? INT_MAX : static_cast<int>(n);
}

/// @brief Renders one node of the list pane, scoped by its index.
/// @return true if the user clicked it this frame.
bool render_node_item(const EntryNode& node, int index, bool selected)
{
    ImGui::PushID(index);
    const float start_x = ImGui::GetCursorPosX();
    // The Selectable's visible label is empty: its ID is "##node" under the
    // index scope, and the (sanitized) text is drawn separately below, so no
    // firmware-derived byte is ever parsed as a label or ID.
    const bool clicked = ImGui::Selectable("##node", selected, ImGuiSelectableFlags_AllowOverlap);
    ImGui::SameLine(start_x);
    const std::string label = entry_node_label(node);
    if (node.decode_failed) {
        ImGui::PushStyleColor(ImGuiCol_Text, kDecodeFailedColor);
    }
    ImGui::TextUnformatted(label.c_str(), label.c_str() + label.size());
    if (node.decode_failed) {
        ImGui::PopStyleColor();
    }
    ImGui::PopID();
    return clicked;
}

/// @brief Renders the left pane: one Selectable per node, clipped.
void render_node_list(const std::vector<EntryNode>& nodes, int& selected_index)
{
    const float width_default = ImGui::GetContentRegionAvail().x * kListPaneFraction;
    const float width = width_default > kListPaneMinWidth ? width_default : kListPaneMinWidth;
    ImGui::BeginChild("##entry_list", ImVec2(width, 0.0F), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

    ImGuiListClipper clipper;
    clipper.Begin(clamp_to_int(nodes.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            if (render_node_item(nodes[static_cast<size_t>(i)], i, i == selected_index)) {
                selected_index = i;
            }
        }
    }
    clipper.End();

    ImGui::EndChild();
}

/// @brief Renders one field-table row, scoped by its index.
/// @param wrap true to wrap the value text at the column edge.
void render_field_row(const FieldRow& row, int index, bool hex, bool wrap)
{
    ImGui::PushID(index);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    const std::string name = display_text(row.name);
    ImGui::TextUnformatted(name.c_str(), name.c_str() + name.size());

    ImGui::TableSetColumnIndex(1);
    if (const bool* flag = std::get_if<bool>(&row.value)) {
        // Read-only glyph (G-11): a disabled checkbox bound to a local copy,
        // so the view model's value can never be modified.
        bool shown = *flag;
        ImGui::BeginDisabled();
        ImGui::Checkbox("##value", &shown);
        ImGui::EndDisabled();
    } else {
        const std::string text = field_value_text(row.value, hex);
        if (wrap) {
            ImGui::PushTextWrapPos(0.0F);
        }
        ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
        if (wrap) {
            ImGui::PopTextWrapPos();
        }
    }

    ImGui::PopID();
}

/// @brief Renders the field table of one node.
void render_field_table(const std::vector<FieldRow>& fields, bool hex)
{
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders
                                     | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##field_table", 2, kFlags)) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 0.35F);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65F);
    ImGui::TableHeadersRow();

    const int count = clamp_to_int(fields.size());
    if (fields.size() > kFieldClipperThreshold) {
        ImGuiListClipper clipper;
        clipper.Begin(count);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                render_field_row(fields[static_cast<size_t>(i)], i, hex, false);
            }
        }
        clipper.End();
    } else {
        for (int i = 0; i < count; ++i) {
            render_field_row(fields[static_cast<size_t>(i)], i, hex, true);
        }
    }

    ImGui::EndTable();
}

/// @brief Renders the right pane: the selected node's heading, the hex
/// toggle, the "Copy as..." action (only for a node with decoded fields) and
/// its field table (or a fixed explanation if it has none).
void render_field_pane(SMBios& bios, const EntryNode& node, int index)
{
    ImGui::BeginChild("##entry_fields", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders);

    const std::string heading = entry_node_label(node);
    ImGui::TextUnformatted(heading.c_str(), heading.c_str() + heading.size());

    // The decimal/hex toggle lives in Dear ImGui's own per-ID state storage
    // (ADR 0003 DD5): no state of this file's own persists across frames.
    ImGuiStorage* const storage = ImGui::GetStateStorage();
    const ImGuiID hex_id = ImGui::GetID("##hex_toggle");
    bool hex = storage->GetBool(hex_id, false);
    if (ImGui::Checkbox("Hex", &hex)) {
        storage->SetBool(hex_id, hex);
    }
    // A node without decoded fields has no entry to render (unregistered
    // type or decode failure), so the action is not shown at all. `index`
    // is non-negative here: render_entry_browser() clamped it.
    if (!node.decode_failed && !node.fields.empty()) {
        ImGui::SameLine();
        render_copy_as_action(bios, node, static_cast<size_t>(index));
    }
    ImGui::Separator();

    if (node.decode_failed) {
        ImGui::PushStyleColor(ImGuiCol_Text, kDecodeFailedColor);
        ImGui::TextUnformatted("This structure could not be decoded; no fields are shown.");
        ImGui::PopStyleColor();
    } else if (node.fields.empty()) {
        ImGui::TextUnformatted("No decoded fields: this structure type has no registered decoder.");
    } else {
        // Scope the table by node index so each node keeps its own scroll
        // position and column state.
        ImGui::PushID(index);
        render_field_table(node.fields, hex);
        ImGui::PopID();
    }

    ImGui::EndChild();
}

} // namespace

void render_entry_browser(SMBios& bios, const std::vector<EntryNode>& nodes,
                          int& selected_index) noexcept
{
    if (nodes.empty()) {
        selected_index = 0;
        ImGui::TextUnformatted(kNoTableText);
        return;
    }
    // Clamp (threat model G-9): check the sign before comparing as size_t.
    if (selected_index < 0 || static_cast<size_t>(selected_index) >= nodes.size()) {
        selected_index = 0;
    }

    render_node_list(nodes, selected_index);
    ImGui::SameLine();
    render_field_pane(bios, nodes[static_cast<size_t>(selected_index)], selected_index);
}

} // namespace smbios::gui
