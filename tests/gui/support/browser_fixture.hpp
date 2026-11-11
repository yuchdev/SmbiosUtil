#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <smbios/gui/view_model.hpp>

#include "support/headless_imgui.hpp"

/// @file browser_fixture.hpp
/// @brief Shared fixture for the `gui-ci` widget suites (milestone 0011,
/// Story 04.0, Task 03): the real smbios::gui::render_entry_browser() over a
/// synthetic table (or hand-built nodes), with the ID/style stack-balance
/// oracle (spike item S3) wrapped around every call. Test code only.
///
/// Used by ci/entry_browser_headless_test.cpp (GuiEntryBrowserHeadlessTests)
/// and ci/copy_as_headless_test.cpp (GuiCopyAsHeadlessTests).
///
/// Every failure message this file emits is a fixed string plus counts or
/// indices: never captured text, clipboard bytes or fixture text (threat
/// model docs/security/2026-11-11-gui-test-tiers.md, R-5).

namespace smbios::test::gui {

/// @brief The browser's three inputs: an SMBios whose live table has been
/// replaced by a synthetic one before any frame, the view-model nodes, and
/// the selection index the browser reads and updates.
///
/// `bios` is declared first so it outlives everything that could borrow
/// from it (browsing-UI threat model G-7).
struct BrowserFixture {
    /// Constructed against the live machine, then immediately replaced via
    /// load_table(); no live data ever reaches a frame.
    SMBios bios;
    /// The display input of render_entry_browser().
    std::vector<smbios::gui::EntryNode> nodes;
    /// The browser's selected node index.
    int selected = 0;
    /// How many render_entry_browser() calls were checked for stack balance.
    int balanced_calls = 0;
    /// How many of those calls left unbalanced stacks (expected 0).
    int unbalanced_calls = 0;

    /// @brief Loads `table` and builds the nodes from it with
    /// smbios::gui::build_entry_tree().
    /// @param table a SyntheticTableBuilder buffer.
    explicit BrowserFixture(std::vector<uint8_t> table);

    /// @brief Loads `table` (usually empty) into `bios` but uses `hand_built`
    /// as the nodes: for node shapes no table can produce (decode failure,
    /// 10 000 nodes, 5000-row tables). Legal because `nodes` is the browser's
    /// only display input; `bios` is only read on a Copy click.
    /// @param table a SyntheticTableBuilder buffer.
    /// @param hand_built the nodes to display.
    BrowserFixture(std::vector<uint8_t> table, std::vector<smbios::gui::EntryNode> hand_built);
};

/// @brief One frame: the real render_entry_browser(fixture.bios,
/// fixture.nodes, fixture.selected) between two ErrorRecoveryStoreState()
/// snapshots. Records the result in `fixture` and asserts (fixed message,
/// counts only) that every stack is balanced.
/// @param ctx the current headless context.
/// @param fixture the browser inputs.
void render_browser(HeadlessImGuiContext& ctx, BrowserFixture& fixture);

/// @brief Like render_browser(), inside run_frame_capturing_text().
/// @return the captured text; compare it in memory only, never print it.
std::string render_browser_capturing(HeadlessImGuiContext& ctx, BrowserFixture& fixture);

/// @brief The line the field table logs for one row (spike item S4 format):
/// `| <display_text(name)> | <value> |`, where `<value>` is
/// field_value_text(value, hex), or the disabled checkbox's `[x]`/`[ ]` for
/// a `bool` value.
/// @param row the view-model row.
/// @param hex the Hex toggle state.
/// @return the expected captured line, without a newline.
std::string expected_row_line(const smbios::gui::FieldRow& row, bool hex);

/// @brief Counts how many of `rows`' expected_row_line()s occur in `text`
/// in order (each searched after the end of the previous match), stopping
/// at the first one missing.
/// @param text captured text.
/// @param rows the selected node's rows.
/// @param hex the Hex toggle state.
/// @return rows.size() iff every row is present, in order.
size_t count_rows_in_order(std::string_view text, const std::vector<smbios::gui::FieldRow>& rows, bool hex);

/// @brief A small synthetic table: BIOS Information (node 0), System
/// Information (node 1), OEM Strings (node 2) and one unregistered OEM-range
/// type 0xC8 (node 3). Every string is a synthetic literal.
std::vector<uint8_t> small_table();

/// Index of the unregistered (Unknown (0xC8)) node in small_table().
inline constexpr int kSmallTableUnknownNode = 3;

/// Number of nodes small_table() yields.
inline constexpr size_t kSmallTableNodes = 4;

} // namespace smbios::test::gui
