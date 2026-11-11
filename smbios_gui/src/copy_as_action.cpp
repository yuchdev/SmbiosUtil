/// @file copy_as_action.cpp
/// @brief Dear ImGui "Copy as..." widget: render_copy_as_action() (milestone
/// 0011, Story 02.0 Task 03).
///
/// Label rule (threat model G-1): every ImGui label/ID and format string in
/// this file is a string literal. Format names come from the formatter
/// registry (developer-controlled, never firmware data) and are still only
/// drawn via ImGui::TextUnformatted() or as the combo's verbatim preview.
/// Rendered output reaches ImGui only through SetClipboardText().

#include <smbios/gui/copy_as_action.hpp>

#include <climits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
// AbstractSMBiosEntry's destructor needs the complete DMIHeader type.
#include <libsmbios/smbios.hpp>
#include <smbios/gui/copy_as.hpp>

namespace smbios::gui {

namespace {

/// @brief Outcome of the last Copy click; each value maps to fixed text.
enum class CopyStatus {
    None,
    Copied,
    NothingToCopy,
    TooLarge,
    Failed,
};

/// @brief Fixed status text for `status`; never firmware-derived.
const char* status_text(CopyStatus status) noexcept
{
    switch (status) {
    case CopyStatus::Copied: return "Copied to clipboard.";
    case CopyStatus::NothingToCopy: return "Nothing to copy.";
    case CopyStatus::TooLarge: return "Entry too large to copy (over 1 MiB).";
    case CopyStatus::Failed: return "Copy failed.";
    case CopyStatus::None: break;
    }
    return "";
}

/// @brief Transient display state remembered across frames (ADR 0003 DD5:
/// the same kind of local UI state Dear ImGui's own examples keep).
struct CopyAsState {
    /// Index of the selected format into format_names(); never a pointer.
    size_t format_index = 0;
    /// Outcome of the last click, shown only while `status_node` is selected.
    CopyStatus status = CopyStatus::None;
    /// Node index the status belongs to.
    size_t status_node = 0;
};

/// @brief The registered format names, copied once: the registry is filled
/// at static-init time and only read afterwards, so the copy never goes
/// stale, and no per-frame allocation is needed.
const std::vector<std::string>& format_names()
{
    static const std::vector<std::string> names = registered_output_formatter_names();
    return names;
}

/// @brief The click handler: re-creates the node's entry, renders it and
/// copies the result to the clipboard. The entry lives only inside this
/// function, while `bios` is alive (G-7).
/// @throws std::bad_alloc only; every other exception becomes Failed.
CopyStatus copy_node_as(SMBios& bios, const EntryNode& node, size_t node_index,
                        const std::string& format_name)
{
    try {
        // Null-check even though the dropdown comes from the same registry
        // (G-10); a null formatter is "nothing to copy", not an error.
        if (get_output_formatter(format_name) == nullptr) {
            return CopyStatus::NothingToCopy;
        }
        const std::unique_ptr<AbstractSMBiosEntry> entry =
            create_entry_for_node(bios, node_index, node);
        if (!entry) {
            return CopyStatus::NothingToCopy;
        }
        // The formatter exists, so nullopt here means the size cap (G-5).
        const std::optional<std::string> rendered = render_entry_as(*entry, format_name);
        if (!rendered) {
            return CopyStatus::TooLarge;
        }
        // SetClipboardText() takes a C string; an embedded NUL would be a
        // silent truncation, so refuse rather than copy a non-exact prefix.
        if (rendered->find('\0') != std::string::npos) {
            return CopyStatus::Failed;
        }
        ImGui::SetClipboardText(rendered->c_str());
        return CopyStatus::Copied;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        // Deliberately no e.what(): it may carry firmware-influenced text.
        return CopyStatus::Failed;
    }
}

} // namespace

void render_copy_as_action(SMBios& bios, const EntryNode& node, size_t node_index) noexcept
{
    static CopyAsState state{default_copy_format_index(format_names())};

    const std::vector<std::string>& names = format_names();
    if (names.empty()) {
        ImGui::TextUnformatted("No output formats are registered.");
        return;
    }
    if (state.format_index >= names.size()) {
        state.format_index = default_copy_format_index(names);
    }

    ImGui::PushID("##copy_as");

    ImGui::TextUnformatted("Copy as");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0F);
    // The preview is a registry key, drawn verbatim by ImGui (no format
    // parsing, not part of any ID).
    if (ImGui::BeginCombo("##format", names[state.format_index].c_str())) {
        const int count = names.size() > static_cast<size_t>(INT_MAX)
                        ? INT_MAX : static_cast<int>(names.size());
        for (int i = 0; i < count; ++i) {
            const size_t index = static_cast<size_t>(i);
            const bool selected = index == state.format_index;
            ImGui::PushID(i);
            const float start_x = ImGui::GetCursorPosX();
            if (ImGui::Selectable("##format_item", selected, ImGuiSelectableFlags_AllowOverlap)) {
                state.format_index = index;
            }
            ImGui::SameLine(start_x);
            ImGui::TextUnformatted(names[index].c_str(), names[index].c_str() + names[index].size());
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        state.status = copy_node_as(bios, node, node_index, names[state.format_index]);
        state.status_node = node_index;
    }

    if (!is_terminal_safe_format(names[state.format_index])) {
        ImGui::TextUnformatted(kCopyAsUnsafeFormatNote);
    }
    if (state.status != CopyStatus::None && state.status_node == node_index) {
        ImGui::TextUnformatted(status_text(state.status));
    }

    ImGui::PopID();
}

} // namespace smbios::gui
