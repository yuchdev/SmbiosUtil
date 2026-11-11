#pragma once
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <smbios/gui/view_model.hpp>

/// @file copy_as.hpp
/// @brief Dear-ImGui-free logic behind smbios_gui's "Copy as..." action
/// (milestone 0011, Story 02.0 Task 03).
///
/// The ImGui widget (copy_as_action.hpp, executable-only) is a thin shell
/// over these functions, which live in the GUI-free `libsmbios_gui_model`
/// library so the unit suite (Task 04) can test them with no window.
///
/// No second renderer: every byte render_entry_as() returns comes from the
/// registered smbios::IOutputFormatter::render(), unmodified (threat model
/// `docs/security/2026-11-11-gui-browsing-ui.md`, G-5).
///
/// Linking: this library does not force-load libsmbios. The formatter and
/// entry registries are filled by REGISTER_OUTPUT_FORMATTER /
/// REGISTER_SMBIOS_ENTRY static initializers that only run if their object
/// files are linked in, so the *final executable* (smbios_gui, and the
/// Task 04 unit-test target) must whole-archive-link libsmbios - otherwise
/// render_entry_as() returns std::nullopt for every name and
/// create_entry_for_node() returns nullptr for every node.

namespace smbios {
class AbstractSMBiosEntry;
class SMBios;
} // namespace smbios

namespace smbios::gui {

/// @brief Largest rendered entry, in bytes, that "Copy as..." puts on the
/// clipboard (1 MiB, threat model G-5). Larger output is refused, never cut.
inline constexpr size_t kCopyAsMaxBytes = size_t{1} << 20;

/// @brief The format "Copy as..." selects by default: the only built-in
/// format whose output is terminal-safe (G-5).
inline constexpr std::string_view kDefaultCopyFormat = "json";

/// @brief Renders one entry through the registered formatter `format_name`,
/// exactly as smbios::get_output_formatter(format_name)->render(entry)
/// returns it - no framing, trimming, wrapping or re-escaping.
/// @param entry the entry to render; only borrowed for the call (and, like
/// every entry, must not outlive the SMBios it was created from).
/// @param format_name exact, case-sensitive registry key (e.g. `"json"`).
/// @return the rendered bytes; std::nullopt if `format_name` is not
/// registered, or if the output exceeds kCopyAsMaxBytes.
/// @throws std::bad_alloc; otherwise only what a non-conforming formatter
/// throws (conforming formatters never throw, see IOutputFormatter).
std::optional<std::string> render_entry_as(const smbios::AbstractSMBiosEntry& entry,
                                           std::string_view format_name);

/// @brief Index of the dropdown's default format within `names`: the
/// position of kDefaultCopyFormat, or 0 if it is absent.
/// @param names the format names, e.g. registered_output_formatter_names().
/// @return a valid index into `names`, or 0 if `names` is empty.
size_t default_copy_format_index(const std::vector<std::string>& names) noexcept;

/// @brief True iff `format_name`'s output is known to be safe to paste into
/// a terminal (no raw control bytes): currently `json` only (G-5).
///
/// Fail-safe: every other name - `text`, `xml`, `yaml` and any formatter
/// registered later - is reported unsafe, so the UI shows its warning note.
/// @param format_name a registry key.
/// @return true only for kDefaultCopyFormat.
bool is_terminal_safe_format(std::string_view format_name) noexcept;

/// @brief Re-creates the AbstractSMBiosEntry behind one view-model node, for
/// a single explicit "Copy as..." click (threat model G-7: never per frame).
///
/// build_entry_tree() emits exactly one node per structure in SMBios
/// iteration order, so `node_index` is the structure's position in
/// `[bios.begin(), bios.end())`. As a consistency guard, the structure found
/// there must also match `node`'s type and handle.
/// @param bios the table the nodes were built from; must outlive the
/// returned entry, which points into its table buffer.
/// @param node_index position of the node in build_entry_tree()'s result.
/// @param node that node; its type/handle are cross-checked.
/// @return the new entry; nullptr if `node_index` is past the end, the
/// structure does not match `node`, or its type has no registered entry
/// class.
/// @throws std::bad_alloc, or whatever the entry class's constructor throws.
std::unique_ptr<smbios::AbstractSMBiosEntry> create_entry_for_node(smbios::SMBios& bios,
                                                                   size_t node_index,
                                                                   const EntryNode& node);

} // namespace smbios::gui
