/// @file browser_fixture.cpp
/// @brief BrowserFixture and helpers (milestone 0011, Story 04.0, Task 03).
/// See browser_fixture.hpp.

#include "support/browser_fixture.hpp"

#include <utility>
#include <variant>

#include <imgui.h>
#include <imgui_internal.h>

#include <smbios/gui/entry_tree_view.hpp>
#include <smbios/gui/field_text.hpp>

#include "gui_test_support.hpp"
#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

namespace smbios::test::gui {

namespace {

/// Formatted-area length of small_table()'s BIOS/System Information
/// structures; every byte is 0x02, so their numeric fields are non-zero.
constexpr size_t kFormattedAreaLength = 0x60;

/// @brief One render_entry_browser() call between two stack snapshots;
/// records the outcome in `fixture`. Runs inside a frame, so it never
/// asserts itself (a BOOST_TEST_REQUIRE throw would leave the frame open).
void render_checked(BrowserFixture& fixture)
{
    ImGuiErrorRecoveryState before;
    ImGuiErrorRecoveryState after;
    ImGui::ErrorRecoveryStoreState(&before);
    smbios::gui::render_entry_browser(fixture.bios, fixture.nodes, fixture.selected);
    ImGui::ErrorRecoveryStoreState(&after);
    ++fixture.balanced_calls;
    if (!stack_sizes_equal(before, after)) {
        ++fixture.unbalanced_calls;
    }
}

/// @brief Fixed-message assertion that the last checked call was balanced.
void check_balanced(const BrowserFixture& fixture, int unbalanced_before)
{
    const int new_unbalanced = fixture.unbalanced_calls - unbalanced_before;
    BOOST_TEST(new_unbalanced == 0, "render_entry_browser() left unbalanced ImGui stacks (call #"
                                        << fixture.balanced_calls << ")");
}

} // namespace

BrowserFixture::BrowserFixture(std::vector<uint8_t> table)
{
    load_table(bios, std::move(table));
    nodes = smbios::gui::build_entry_tree(bios);
}

BrowserFixture::BrowserFixture(std::vector<uint8_t> table, std::vector<smbios::gui::EntryNode> hand_built)
    : nodes(std::move(hand_built))
{
    load_table(bios, std::move(table));
}

void render_browser(HeadlessImGuiContext& ctx, BrowserFixture& fixture)
{
    const int unbalanced_before = fixture.unbalanced_calls;
    ctx.run_frame([&] { render_checked(fixture); });
    check_balanced(fixture, unbalanced_before);
}

std::string render_browser_capturing(HeadlessImGuiContext& ctx, BrowserFixture& fixture)
{
    const int unbalanced_before = fixture.unbalanced_calls;
    std::string text = ctx.run_frame_capturing_text([&] { render_checked(fixture); });
    check_balanced(fixture, unbalanced_before);
    return text;
}

std::string expected_row_line(const smbios::gui::FieldRow& row, bool hex)
{
    std::string value;
    if (const bool* flag = std::get_if<bool>(&row.value)) {
        value = *flag ? "[x]" : "[ ]";
    } else {
        value = smbios::gui::field_value_text(row.value, hex);
    }
    return "| " + smbios::gui::display_text(row.name) + " | " + value + " |";
}

size_t count_rows_in_order(std::string_view text, const std::vector<smbios::gui::FieldRow>& rows, bool hex)
{
    size_t found = 0;
    size_t pos = 0;
    for (const smbios::gui::FieldRow& row : rows) {
        const std::string line = expected_row_line(row, hex);
        const size_t at = text.find(line, pos);
        if (at == std::string_view::npos) {
            break;
        }
        pos = at + line.size();
        ++found;
    }
    return found;
}

std::vector<uint8_t> small_table()
{
    const std::vector<uint8_t> area(kFormattedAreaLength, 0x02);
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BIOSInformation, area, {"Synthetic Vendor", "SYN-BIOS-0001"})
        .add_structure(SMBios::SystemInformation, area, {"Synthetic Maker", "SYN-SYS-0001"})
        .add_structure(SMBios::OemStrings, {2}, {"synthetic oem one", "synthetic oem two"})
        .add_structure(0xC8, {0xAA, 0xBB});
    return builder.build();
}

} // namespace smbios::test::gui
