/// @file entry_browser_headless_test.cpp
/// @brief Headless widget tests of smbios::gui::render_entry_browser()
/// (milestone 0011, Story 04.0, Task 03): suite GuiEntryBrowserHeadlessTests.
///
/// Type: Unit, CI-safe tier (`smbios_gui_ci_test`, labels gui;gui-ci) -
/// hermetic: no window, no GL, no OS clipboard, synthetic tables or
/// hand-built nodes only. A case that cannot run fails; nothing skips.
/// Scenario: the real entry_tree_view.cpp (compiled into this executable,
/// never into libsmbios_gui_model - tests/gui/CMakeLists.txt asserts it) is
/// driven through Task 02's HeadlessImGuiContext. Oracles: S3 stack
/// balance (every render_entry_browser() call, via BrowserFixture), S4
/// captured text (only for text that must be *present*: logging turns
/// clipping off, so captured text never proves visibility), S5 widget
/// activation by ID path, and - for the two clipper cases - hit-testing:
/// the mouse is moved down the pane in a normal (non-logging) frame and
/// Dear ImGui's own HoveredId says which row is really submitted at each
/// position. S6's duplicate-ID signal is hover-scoped, so it is read while
/// the duplicate-handle rows are hovered.
/// Boundaries: empty table, out-of-range selection (-1, INT_MIN, size,
/// INT_MAX), unregistered and decode-failed nodes, hostile strings
/// (format specifiers, `##`/`###` ID syntax, C0/bidi controls, Actions
/// `::` commands, 10 000 bytes), duplicate handles, 10 000 nodes, field
/// tables either side of the 256-row clipper threshold, 1000 frames.
/// Determinism: each case owns a fresh HeadlessImGuiContext, so the Hex
/// toggle (ImGui per-ID state storage) never outlives a case. No case here
/// clicks Copy or changes the Copy-as format, so copy_as_action.cpp's
/// function-local CopyAsState is neither changed nor relied on (R-10).
/// Assertions are boolean/numeric with fixed messages and never print
/// captured or fixture text (threat model
/// docs/security/2026-11-11-gui-test-tiers.md, R-5).
/// On failure first check: smbios_gui/src/entry_tree_view.cpp, then
/// tests/gui/support/browser_fixture.cpp and headless_imgui.cpp, then
/// whether the imgui version changed (headless_imgui.hpp's symbol list;
/// this file additionally uses ImGuiContext::{HoveredId,
/// DebugDrawIdConflictsId}, ImGuiWindow::{ContentSize, ScrollMax, Scroll,
/// InnerClipRect}, ImGui::{SetScrollY(ImGuiWindow*), TableFindByID},
/// ImGuiTable::Columns and ImGuiTableColumn::WorkMinX).

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <libsmbios/smbios.hpp>
#include <smbios/gui/entry_tree_view.hpp>
#include <smbios/gui/field_text.hpp>
#include <smbios/gui/view_model.hpp>

#include "doc_sample_fixture.hpp"
#include "gui_test_support.hpp"
#include "support/browser_fixture.hpp"
#include "support/headless_imgui.hpp"
#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

using smbios::SMBios;
using smbios::gui::EntryNode;
using smbios::gui::FieldRow;
using smbios::test::SyntheticTableBuilder;
using smbios::test::gui::BrowserFixture;
using smbios::test::gui::count_occurrences;
using smbios::test::gui::count_rows_in_order;
using smbios::test::gui::HeadlessImGuiContext;
using smbios::test::gui::render_browser;
using smbios::test::gui::render_browser_capturing;

namespace {

/// Node count of the list-clipper fixture (G-3).
constexpr int kManyNodes = 10000;

/// Row count of the clipped field table: above the production threshold
/// (kFieldClipperThreshold = 256 in entry_tree_view.cpp).
constexpr int kManyFieldRows = 5000;

/// Row count of the unclipped field table: below that threshold.
constexpr int kFewFieldRows = 200;

/// Fixed explanation entry_tree_view.cpp shows for a decode-failed node.
constexpr const char* kDecodeFailedExplanation = "This structure could not be decoded; no fields are shown.";

/// Fixed explanation entry_tree_view.cpp shows for a node with no fields.
constexpr const char* kNoFieldsExplanation = "No decoded fields: this structure type has no registered decoder.";

/// @brief Style-derived metrics, read inside a frame (no pixel constants).
struct Metrics {
    /// ImGui::GetTextLineHeightWithSpacing(): one node-list row.
    float line_pitch = 0.0F;
    /// ImGui::GetFrameHeight(): one disabled bool checkbox.
    float frame_height = 0.0F;
    /// style.ItemSpacing.y.
    float item_spacing_y = 0.0F;
};

/// @brief Reads Metrics in a frame of its own. Call it before the frames a
/// case inspects (it does not submit the browser).
Metrics read_metrics(HeadlessImGuiContext& ctx)
{
    Metrics metrics;
    ctx.run_frame([&] {
        metrics.line_pitch = ImGui::GetTextLineHeightWithSpacing();
        metrics.frame_height = ImGui::GetFrameHeight();
        metrics.item_spacing_y = ImGui::GetStyle().ItemSpacing.y;
    });
    return metrics;
}

/// @brief What a hover sweep down a pane saw.
struct SweepResult {
    /// Row indices hovered, in sweep order, consecutive repeats removed.
    std::vector<int> rows;
    /// Mouse positions sampled.
    int samples = 0;
    /// Samples at which DebugDrawIdConflictsId was non-zero (S6).
    int conflict_signals = 0;
};

/// @brief Moves the mouse down `area` at column `x`, `step` pixels at a
/// time, rendering `frames_per_sample` normal (non-logging, so clipping is
/// on) browser frames per position, and maps Dear ImGui's HoveredId after
/// each position through `id_to_row`. IDs not in the map (window
/// background, table header, resize borders) are ignored.
SweepResult sweep_rows(HeadlessImGuiContext& ctx, BrowserFixture& fixture, const ImRect& area, float x, float step,
                       const std::unordered_map<ImGuiID, int>& id_to_row, int frames_per_sample)
{
    SweepResult result;
    for (float y = area.Min.y + 0.5F; y < area.Max.y; y += step) {
        for (int frame = 0; frame < frames_per_sample; ++frame) {
            ImGui::GetIO().AddMousePosEvent(x, y);
            render_browser(ctx, fixture);
        }
        ++result.samples;
        result.conflict_signals += GImGui->DebugDrawIdConflictsId != 0 ? 1 : 0;
        const auto found = id_to_row.find(GImGui->HoveredId);
        if (found != id_to_row.end() && (result.rows.empty() || result.rows.back() != found->second)) {
            result.rows.push_back(found->second);
        }
    }
    return result;
}

/// @brief True iff `rows` is non-empty and each element is the previous plus one.
bool is_contiguous(const std::vector<int>& rows)
{
    if (rows.empty()) {
        return false;
    }
    for (size_t i = 1; i < rows.size(); ++i) {
        if (rows[i] != rows[i - 1] + 1) {
            return false;
        }
    }
    return true;
}

/// @brief Maps each node-list Selectable's ID (`PushID(i)` + `"##node"`
/// under `##entry_list`) to its index, for indices [0, count).
std::unordered_map<ImGuiID, int> node_ids(const HeadlessImGuiContext& ctx, int count)
{
    std::unordered_map<ImGuiID, int> ids;
    const ImGuiWindow* list = ctx.find_child_window("##entry_list");
    if (list == nullptr) {
        return ids;
    }
    for (int i = 0; i < count; ++i) {
        ids.emplace(ImGui::GetIDWithSeed("##node", nullptr, ImGui::GetIDWithSeed(i, list->ID)), i);
    }
    return ids;
}

/// @brief A hand-built, registered-looking node with `field_count` bool rows
/// (alternating true/false). Bool rows are drawn as disabled checkboxes
/// `"##value"`, which are the only per-row items with an ID, so they make
/// every field-table row hit-testable; they also share one row height, as
/// the clipper assumes.
EntryNode bool_rows_node(uint16_t handle, int field_count)
{
    EntryNode node;
    node.type = SMBios::OemStrings;
    node.handle = handle;
    node.label = smbios::gui::entry_type_label(node.type);
    node.fields.reserve(static_cast<size_t>(field_count));
    for (int i = 0; i < field_count; ++i) {
        node.fields.push_back(FieldRow{"synthetic flag " + std::to_string(i), (i % 2) == 0});
    }
    return node;
}

/// @brief Scrolls `window` to its maximum Y and renders two frames, so the
/// scroll and the clipper's range settle.
void scroll_to_bottom(HeadlessImGuiContext& ctx, BrowserFixture& fixture, ImGuiWindow* window)
{
    ImGui::SetScrollY(window, window->ScrollMax.y);
    render_browser(ctx, fixture);
    render_browser(ctx, fixture);
}

/// @brief The fixed hostile strings (synthetic, test-defined - browsing-UI
/// threat model requirement 20). Never printed: some carry terminal escapes
/// and GitHub Actions `::` workflow commands.
std::vector<std::string> hostile_strings()
{
    return {
        "%n%s%x%p",
        "%%",
        "visible##x",
        "##x",
        "###id",
        std::string("ctl\x01\x07\x1B[2J\x7F\t\r\nend"),
        "bidi \xE2\x80\xAE" "desrever \xE2\x81\xA6isolate\xE2\x81\xA9",
        "::error title=hostile::injected\n::set-output name=x::y",
        std::string(10000, 'H'),
    };
}

} // namespace

BOOST_AUTO_TEST_SUITE(GuiEntryBrowserHeadlessTests)

/// Boundary: `nodes = {}` shows only the fixed no-table text - no Copy, no
/// Hex, no list or field pane - and resets an out-of-range selection to 0.
BOOST_AUTO_TEST_CASE(EmptyNodesShowsNoTableText)
{
    HeadlessImGuiContext ctx;
    BrowserFixture browser(SyntheticTableBuilder().build());
    BOOST_TEST_REQUIRE(browser.nodes.empty());
    browser.selected = 7;

    const std::string text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(text, smbios::gui::kNoTableText) == 1U, "no-table text: expected 1 occurrence");
    BOOST_TEST(count_occurrences(text, "Copy") == 0U, "empty table: Copy control captured");
    BOOST_TEST(count_occurrences(text, "Hex") == 0U, "empty table: Hex toggle captured");
    BOOST_TEST((ctx.find_child_window("##entry_list") == nullptr), "empty table: node list submitted");
    BOOST_TEST((ctx.find_child_window("##entry_fields") == nullptr), "empty table: field pane submitted");
    BOOST_TEST(browser.selected == 0, "empty table: selected expected 0 -> " << browser.selected);
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// Boundary (G-9): every out-of-range selection is clamped to 0 after one
/// frame; in-range selections are left unchanged.
BOOST_AUTO_TEST_CASE(SelectionIndexClampedEveryOutOfRangeValue)
{
    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    BOOST_TEST_REQUIRE(browser.nodes.size() == smbios::test::gui::kSmallTableNodes);
    const int size = static_cast<int>(browser.nodes.size());

    for (const int out_of_range : {-1, INT_MIN, size, size + 1, INT_MAX}) {
        browser.selected = out_of_range;
        render_browser(ctx, browser);
        BOOST_TEST(browser.selected == 0,
                   "selection " << out_of_range << ": expected 0 -> " << browser.selected);
    }
    for (const int valid : {0, 2, size - 1}) {
        browser.selected = valid;
        render_browser(ctx, browser);
        BOOST_TEST(browser.selected == valid, "selection " << valid << " changed -> " << browser.selected);
    }
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// Every node - registered or not - is listed with its entry_node_label();
/// the unregistered OEM type shows as `Unknown (0xC8)`. Selecting it (S5
/// node click) shows the fixed no-fields text, no field table and no Copy
/// controls.
BOOST_AUTO_TEST_CASE(RegisteredAndUnknownNodesRender)
{
    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    BOOST_TEST_REQUIRE(browser.nodes.size() == smbios::test::gui::kSmallTableNodes);
    const EntryNode& unknown = browser.nodes[static_cast<size_t>(smbios::test::gui::kSmallTableUnknownNode)];
    BOOST_TEST_REQUIRE(unknown.fields.empty());
    BOOST_TEST_REQUIRE(!unknown.decode_failed);
    for (int i = 0; i < smbios::test::gui::kSmallTableUnknownNode; ++i) {
        BOOST_TEST_REQUIRE(!browser.nodes[static_cast<size_t>(i)].fields.empty(),
                           "fixture node #" << i << " has no decoded fields");
    }

    render_browser(ctx, browser); // layout frame
    const std::string text = render_browser_capturing(ctx, browser);
    for (size_t i = 0; i < browser.nodes.size(); ++i) {
        const size_t expected = i == 0 ? 2U : 1U; // node 0 is also the field-pane heading
        BOOST_TEST(count_occurrences(text, smbios::gui::entry_node_label(browser.nodes[i])) == expected,
                   "node #" << i << ": label occurrences expected " << expected);
    }
    BOOST_TEST(count_occurrences(text, "Unknown (0xC8)") == 1U, "Unknown (0xC8) node label missing");

    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_list", {smbios::test::gui::kSmallTableUnknownNode, "##node"}));
    render_browser(ctx, browser);
    BOOST_TEST(browser.selected == smbios::test::gui::kSmallTableUnknownNode,
               "unknown node click: selected -> " << browser.selected);
    const std::string unknown_text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(unknown_text, "Unknown (0xC8)") == 2U, "unknown node: label + heading expected");
    BOOST_TEST(count_occurrences(unknown_text, kNoFieldsExplanation) == 1U, "unknown node: no-fields text missing");
    BOOST_TEST((ctx.find_child_window("##field_table") == nullptr), "unknown node: a field table was submitted");
    BOOST_TEST(count_occurrences(unknown_text, "Copy") == 0U, "unknown node: Copy controls captured");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// Every row of every node of the doc-sample fixture is captured as
/// `| name | field_value_text(value, false) |` (a `[x]`/`[ ]` checkbox for
/// bool), in view-model order; together the nodes cover all four FieldValue
/// alternatives (M-04, M-05).
BOOST_AUTO_TEST_CASE(FieldRowsMatchFieldValueText)
{
    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::build_doc_sample_table());
    BOOST_TEST_REQUIRE(browser.nodes.size() == 3U);

    bool saw_string = false;
    bool saw_uint64 = false;
    bool saw_bool = false;
    bool saw_bytes = false;
    for (int node = 0; node < static_cast<int>(browser.nodes.size()); ++node) {
        const std::vector<FieldRow>& rows = browser.nodes[static_cast<size_t>(node)].fields;
        BOOST_TEST_REQUIRE(!rows.empty(), "doc-sample node #" << node << " has no fields");
        for (const FieldRow& row : rows) {
            saw_string = saw_string || std::holds_alternative<std::string>(row.value);
            saw_uint64 = saw_uint64 || std::holds_alternative<uint64_t>(row.value);
            saw_bool = saw_bool || std::holds_alternative<bool>(row.value);
            saw_bytes = saw_bytes || std::holds_alternative<std::vector<uint8_t>>(row.value);
        }
        browser.selected = node;
        render_browser(ctx, browser);
        const std::string text = render_browser_capturing(ctx, browser);
        const size_t found = count_rows_in_order(text, rows, false);
        BOOST_TEST(found == rows.size(),
                   "node #" << node << ": rows captured in order: expected " << rows.size() << " -> " << found);
    }
    BOOST_TEST(saw_string, "doc-sample fixture: no std::string field");
    BOOST_TEST(saw_uint64, "doc-sample fixture: no uint64_t field");
    BOOST_TEST(saw_bool, "doc-sample fixture: no bool field");
    BOOST_TEST(saw_bytes, "doc-sample fixture: no byte-vector field");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// Error path (G-6): a decode-failed node is labelled with kDecodeFailedText
/// (list and heading) and shows the fixed explanation instead of a table,
/// with no Copy controls.
BOOST_AUTO_TEST_CASE(DecodeFailedNodeShowsMarker)
{
    EntryNode failed;
    failed.type = SMBios::BIOSInformation;
    failed.handle = 0x0007;
    failed.label = smbios::gui::entry_type_label(failed.type);
    failed.decode_failed = true;

    HeadlessImGuiContext ctx;
    BrowserFixture browser(SyntheticTableBuilder().build(), {failed});
    render_browser(ctx, browser);
    const std::string text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(text, smbios::gui::kDecodeFailedText) == 2U,
               "decode-failed marker: expected in list label and heading");
    BOOST_TEST(count_occurrences(text, kDecodeFailedExplanation) == 1U, "decode-failed explanation missing");
    BOOST_TEST(count_occurrences(text, kNoFieldsExplanation) == 0U, "decode-failed node shows the no-fields text");
    BOOST_TEST((ctx.find_child_window("##field_table") == nullptr), "decode-failed node: a field table was submitted");
    BOOST_TEST(count_occurrences(text, "Copy") == 0U, "decode-failed node: Copy controls captured");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// Security (G-1/G-2/G-4): hostile labels, field names and field values
/// are shown as display_text(s), verbatim - so `##x`/`###id` stay
/// *visible*, proving no firmware string was used as a label or ID - with
/// no crash and balanced stacks. Node selection by index still works for
/// the node whose label is `###id`.
BOOST_AUTO_TEST_CASE(HostileStringsRenderAsPlainText)
{
    const std::vector<std::string> hostile = hostile_strings();
    std::vector<EntryNode> nodes;
    for (size_t i = 0; i < hostile.size(); ++i) {
        EntryNode node;
        node.type = 0xC8;
        node.handle = static_cast<uint16_t>(0x0100 + i);
        node.label = hostile[i];
        node.fields.push_back(FieldRow{hostile[i], hostile[i]});
        nodes.push_back(std::move(node));
    }
    // The ID-syntax strings pass display_text() unchanged, so seeing them
    // verbatim below means ImGui never parsed them.
    for (const char* id_syntax : {"visible##x", "##x", "###id"}) {
        BOOST_TEST((smbios::gui::display_text(id_syntax) == id_syntax), "display_text() changed an ID-syntax string");
    }

    HeadlessImGuiContext ctx;
    BrowserFixture browser(SyntheticTableBuilder().build(), nodes);
    for (int i = 0; i < static_cast<int>(hostile.size()); ++i) {
        const EntryNode& node = browser.nodes[static_cast<size_t>(i)];
        browser.selected = i;
        render_browser(ctx, browser);
        const std::string text = render_browser_capturing(ctx, browser);
        BOOST_TEST(count_occurrences(text, smbios::gui::entry_node_label(node)) == 2U,
                   "hostile string #" << i << ": label expected in list and heading");
        const size_t rows = count_rows_in_order(text, node.fields, false);
        BOOST_TEST(rows == 1U, "hostile string #" << i << ": field row (name and value) not captured verbatim");
    }

    // Selecting by index is unaffected by a label that is pure ID syntax.
    const auto id_node = static_cast<int>(
        std::find(hostile.begin(), hostile.end(), std::string("###id")) - hostile.begin());
    browser.selected = 0;
    render_browser(ctx, browser);
    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_list", {id_node, "##node"}));
    render_browser(ctx, browser);
    BOOST_TEST(browser.selected == id_node, "click on the ###id node: selected -> " << browser.selected);
    BOOST_TEST(ctx.hooks_intact());
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// G-2: two structures with the same type and handle are two separate rows
/// with distinct IDs (scoped by index, never by handle), each selectable
/// by index. S6 limit: Dear ImGui's only duplicate-ID signal is
/// hover-scoped, so it is read while the mouse is over each of the two
/// rows in turn (both rows must actually be hovered for the check to
/// count); a whole-frame check is impossible without rebuilding imgui with
/// IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS.
BOOST_AUTO_TEST_CASE(DuplicateHandlesNoIdConflict)
{
    constexpr uint16_t kHandle = 0x0042;
    SyntheticTableBuilder builder;
    builder.add_raw_bytes(smbios::test::gui::oem_strings_structure(kHandle, {"synthetic duplicate a"}))
        .add_raw_bytes(smbios::test::gui::oem_strings_structure(kHandle, {"synthetic duplicate b"}))
        .add_raw_bytes(smbios::test::gui::end_of_table_structure(0x0043));

    HeadlessImGuiContext ctx;
    const float line_pitch = read_metrics(ctx).line_pitch;
    BrowserFixture browser(builder.build());
    BOOST_TEST_REQUIRE(browser.nodes.size() == 2U);
    BOOST_TEST_REQUIRE(browser.nodes[0].handle == browser.nodes[1].handle);
    BOOST_TEST_REQUIRE(browser.nodes[0].type == browser.nodes[1].type);
    const std::string label = smbios::gui::entry_node_label(browser.nodes[0]);
    BOOST_TEST_REQUIRE((label == smbios::gui::entry_node_label(browser.nodes[1])));

    render_browser(ctx, browser);
    const std::string text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(text, label) == 3U, "duplicate rows: expected two list labels + one heading");

    const std::unordered_map<ImGuiID, int> ids = node_ids(ctx, 2);
    BOOST_TEST(ids.size() == 2U, "duplicate rows share one widget ID");

    for (const int target : {1, 0}) {
        BOOST_TEST_REQUIRE(ctx.activate_item("##entry_list", {target, "##node"}));
        render_browser(ctx, browser);
        BOOST_TEST(browser.selected == target, "click on duplicate #" << target << ": selected -> " << browser.selected);
    }

    // Two frames per position: DebugDrawIdConflictsId is computed from the
    // previous frame's hover.
    const ImGuiWindow* list = ctx.find_child_window("##entry_list");
    BOOST_TEST_REQUIRE(list != nullptr);
    const ImRect area = list->InnerClipRect;
    const SweepResult sweep =
        sweep_rows(ctx, browser, area, (area.Min.x + area.Max.x) * 0.5F, line_pitch * 0.5F, ids, 2);
    BOOST_TEST((sweep.rows == std::vector<int>{0, 1}), "both duplicate rows must be hovered, in order: saw "
                                                           << sweep.rows.size() << " distinct row(s)");
    BOOST_TEST(sweep.conflict_signals == 0, "ID-conflict signal raised at " << sweep.conflict_signals << " of "
                                                                            << sweep.samples << " positions");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// S3 over every input shape, as one explicit case: empty, normal (every
/// node selected), decode-failed and hostile. (Every other case in this
/// suite also checks every call through BrowserFixture.)
BOOST_AUTO_TEST_CASE(IdAndStyleStacksBalancedEveryPath)
{
    HeadlessImGuiContext ctx;
    EntryNode failed;
    failed.type = SMBios::BIOSInformation;
    failed.label = smbios::gui::entry_type_label(failed.type);
    failed.decode_failed = true;
    std::vector<EntryNode> hostile_nodes;
    for (const std::string& s : hostile_strings()) {
        EntryNode node;
        node.type = 0xC9;
        node.label = s;
        node.fields.push_back(FieldRow{s, s});
        hostile_nodes.push_back(std::move(node));
    }

    BrowserFixture empty(SyntheticTableBuilder().build());
    BrowserFixture normal(smbios::test::gui::small_table());
    BrowserFixture decode_failed(SyntheticTableBuilder().build(), {failed});
    BrowserFixture hostile(SyntheticTableBuilder().build(), hostile_nodes);
    int expected_calls = 0;
    for (BrowserFixture* fixture : {&empty, &normal, &decode_failed, &hostile}) {
        const int node_count = std::max(1, static_cast<int>(fixture->nodes.size()));
        for (int node = 0; node < node_count; ++node) {
            fixture->selected = node;
            render_browser(ctx, *fixture);
            (void)render_browser_capturing(ctx, *fixture);
        }
        expected_calls += 2 * node_count;
    }
    const int calls = empty.balanced_calls + normal.balanced_calls + decode_failed.balanced_calls
                    + hostile.balanced_calls;
    const int unbalanced = empty.unbalanced_calls + normal.unbalanced_calls + decode_failed.unbalanced_calls
                         + hostile.unbalanced_calls;
    BOOST_TEST(calls == expected_calls);
    BOOST_TEST(unbalanced == 0, "unbalanced calls: expected 0 -> " << unbalanced << " of " << calls);
}

/// G-3, M-07 (S4 amendment): 10 000 hand-built nodes. Captured text cannot
/// prove visibility (logging disables clipping), so the oracle is
/// hit-testing in normal frames: the list's content height reserves all
/// 10 000 rows; hovering down the pane at the top hits rows 0, 1, 2, ...
/// with no gap, and after SetScrollY() to the bottom hits a contiguous run
/// ending at the last node - each run no longer than the pane can show.
/// Pixels/visual layout stay manual (M-07).
BOOST_AUTO_TEST_CASE(NodeListClipperShowsContiguousTail)
{
    std::vector<EntryNode> nodes(static_cast<size_t>(kManyNodes));
    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i].type = SMBios::OemStrings;
        nodes[i].handle = static_cast<uint16_t>(i + 1);
        nodes[i].label = smbios::gui::entry_type_label(nodes[i].type);
    }

    HeadlessImGuiContext ctx;
    const Metrics metrics = read_metrics(ctx);
    BrowserFixture browser(SyntheticTableBuilder().build(), std::move(nodes));
    render_browser(ctx, browser);
    render_browser(ctx, browser);
    ImGuiWindow* list = ctx.find_child_window("##entry_list");
    BOOST_TEST_REQUIRE(list != nullptr);
    const std::unordered_map<ImGuiID, int> ids = node_ids(ctx, kManyNodes);
    BOOST_TEST_REQUIRE(ids.size() == static_cast<size_t>(kManyNodes));

    // Scroll range: the clipper reserves the height of every row.
    const float expected_content = static_cast<float>(kManyNodes) * metrics.line_pitch - metrics.item_spacing_y;
    BOOST_TEST(std::fabs(list->ContentSize.y - expected_content) <= 1.0F,
               "list content height: expected " << expected_content << " -> " << list->ContentSize.y);

    const ImRect area = list->InnerClipRect;
    const float x = (area.Min.x + area.Max.x) * 0.5F;
    const float visible_rows = area.GetHeight() / metrics.line_pitch;
    const auto max_rows = static_cast<size_t>(std::ceil(visible_rows)) + 1U;
    const auto min_rows = static_cast<size_t>(std::floor(visible_rows)) - 1U;

    const SweepResult top = sweep_rows(ctx, browser, area, x, metrics.line_pitch * 0.5F, ids, 1);
    BOOST_TEST(is_contiguous(top.rows), "top of list: hovered rows are not one contiguous run ("
                                            << top.rows.size() << " distinct)");
    BOOST_TEST((!top.rows.empty() && top.rows.front() == 0), "top of list: first hovered row is not node 0");
    BOOST_TEST((top.rows.size() >= min_rows && top.rows.size() <= max_rows),
               "top of list: " << top.rows.size() << " rows hovered, expected " << min_rows << ".." << max_rows);

    scroll_to_bottom(ctx, browser, list);
    BOOST_TEST(list->ScrollMax.y > 0.0F);
    BOOST_TEST(std::fabs(list->Scroll.y - list->ScrollMax.y) <= 1.0F, "list did not scroll to the bottom");
    const SweepResult bottom = sweep_rows(ctx, browser, area, x, metrics.line_pitch * 0.5F, ids, 1);
    BOOST_TEST(is_contiguous(bottom.rows), "bottom of list: hovered rows are not one contiguous run ("
                                               << bottom.rows.size() << " distinct)");
    BOOST_TEST((!bottom.rows.empty() && bottom.rows.back() == kManyNodes - 1),
               "bottom of list: last hovered row is not the last node");
    BOOST_TEST((bottom.rows.size() >= min_rows && bottom.rows.size() <= max_rows),
               "bottom of list: " << bottom.rows.size() << " rows hovered, expected " << min_rows << ".."
                                  << max_rows);
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// G-3 (S4 amendment): a 5000-row field table (clipper path, above
/// kFieldClipperThreshold) and a 200-row one (plain path) both reserve the
/// height of every row and, hit-tested in normal frames through each row's
/// disabled `##value` checkbox, show one contiguous run of rows from row 0
/// at the top and ending at the last row after SetScrollY() to the bottom.
/// Pixels/visual layout stay manual (M-07).
BOOST_AUTO_TEST_CASE(FieldTableClipperAboveThreshold)
{
    HeadlessImGuiContext ctx;
    const Metrics metrics = read_metrics(ctx);
    BrowserFixture browser(SyntheticTableBuilder().build(),
                           {bool_rows_node(0x0001, kManyFieldRows), bool_rows_node(0x0002, kFewFieldRows)});

    for (int node = 0; node < 2; ++node) {
        const int row_count = node == 0 ? kManyFieldRows : kFewFieldRows;
        browser.selected = node;
        render_browser(ctx, browser);
        render_browser(ctx, browser);
        ImGuiWindow* table_window = ctx.find_child_window("##field_table");
        BOOST_TEST_REQUIRE(table_window != nullptr, "node #" << node << ": field table window not found");
        const std::optional<ImGuiID> table_id = ctx.find_item_id("##entry_fields", {node, "##field_table"});
        BOOST_TEST_REQUIRE(table_id.has_value());
        const ImGuiTable* table = ImGui::TableFindByID(*table_id);
        BOOST_TEST_REQUIRE(table != nullptr, "node #" << node << ": table not found by ID");
        BOOST_TEST_REQUIRE(table->ColumnsCount == 2);

        std::unordered_map<ImGuiID, int> ids;
        for (int row = 0; row < row_count; ++row) {
            ids.emplace(ImGui::GetIDWithSeed("##value", nullptr, ImGui::GetIDWithSeed(row, *table_id)), row);
        }
        BOOST_TEST_REQUIRE(ids.size() == static_cast<size_t>(row_count));

        // Scroll range: every row is at least one checkbox tall.
        BOOST_TEST(table_window->ContentSize.y >= static_cast<float>(row_count) * metrics.frame_height,
                   "node #" << node << ": table content height " << table_window->ContentSize.y
                            << " cannot hold " << row_count << " rows");
        const float row_pitch = table_window->ContentSize.y / static_cast<float>(row_count + 1);
        const ImRect area = table_window->InnerClipRect;
        const float visible_rows = area.GetHeight() / row_pitch;
        const auto max_rows = static_cast<size_t>(std::ceil(visible_rows)) + 1U;
        const auto min_rows = static_cast<size_t>(std::floor(visible_rows)) - 2U;
        const float x = table->Columns[1].WorkMinX + metrics.frame_height * 0.5F;
        const float step = metrics.frame_height * 0.5F;

        const SweepResult top = sweep_rows(ctx, browser, area, x, step, ids, 1);
        BOOST_TEST(is_contiguous(top.rows),
                   "node #" << node << " top: rows not contiguous (" << top.rows.size() << " distinct)");
        BOOST_TEST((!top.rows.empty() && top.rows.front() == 0), "node #" << node << " top: first row is not row 0");
        BOOST_TEST((top.rows.size() >= min_rows && top.rows.size() <= max_rows),
                   "node #" << node << " top: " << top.rows.size() << " rows, expected " << min_rows << ".."
                            << max_rows);

        scroll_to_bottom(ctx, browser, table_window);
        BOOST_TEST(std::fabs(table_window->Scroll.y - table_window->ScrollMax.y) <= 1.0F,
                   "node #" << node << ": table did not scroll to the bottom");
        const SweepResult bottom = sweep_rows(ctx, browser, area, x, step, ids, 1);
        BOOST_TEST(is_contiguous(bottom.rows),
                   "node #" << node << " bottom: rows not contiguous (" << bottom.rows.size() << " distinct)");
        BOOST_TEST((!bottom.rows.empty() && bottom.rows.back() == row_count - 1),
                   "node #" << node << " bottom: last row is not the table's last row");
        BOOST_TEST((bottom.rows.size() >= min_rows && bottom.rows.size() <= max_rows),
                   "node #" << node << " bottom: " << bottom.rows.size() << " rows, expected " << min_rows
                            << ".." << max_rows);
    }
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// M-06 (S5): activating `Hex` switches every uint64_t row to
/// field_value_text(v, true); activating it again restores decimal and the
/// toggle's stored state (left as found).
BOOST_AUTO_TEST_CASE(HexToggleSwitchesNumericRendering)
{
    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::build_doc_sample_table());
    int hex_node = -1;
    for (size_t i = 0; i < browser.nodes.size() && hex_node < 0; ++i) {
        for (const FieldRow& row : browser.nodes[i].fields) {
            if (std::holds_alternative<uint64_t>(row.value)
                && smbios::gui::field_value_text(row.value, true) != smbios::gui::field_value_text(row.value, false)) {
                hex_node = static_cast<int>(i);
            }
        }
    }
    BOOST_TEST_REQUIRE(hex_node >= 0, "fixture has no uint64_t field whose hex and decimal text differ");
    const std::vector<FieldRow>& rows = browser.nodes[static_cast<size_t>(hex_node)].fields;
    browser.selected = hex_node;
    render_browser(ctx, browser);

    const std::optional<ImGuiID> storage_id = ctx.find_item_id("##entry_fields", {"##hex_toggle"});
    BOOST_TEST_REQUIRE(storage_id.has_value());
    auto hex_stored = [&] {
        const ImGuiWindow* pane = ctx.find_child_window("##entry_fields");
        return pane != nullptr && pane->StateStorage.GetBool(*storage_id, false);
    };
    BOOST_TEST(!hex_stored(), "Hex toggle not initially off");

    const std::string decimal = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_rows_in_order(decimal, rows, false) == rows.size(), "decimal rows not captured");
    BOOST_TEST(count_occurrences(decimal, "[ ] Hex") == 1U, "unchecked Hex not captured");

    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"Hex"}));
    render_browser(ctx, browser);
    BOOST_TEST(hex_stored(), "Hex activation did not set the toggle");
    const std::string hex = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_rows_in_order(hex, rows, true) == rows.size(), "hex rows not captured after Hex on");
    BOOST_TEST(count_rows_in_order(hex, rows, false) < rows.size(), "decimal rows still captured after Hex on");
    BOOST_TEST(count_occurrences(hex, "[x] Hex") == 1U, "checked Hex not captured");

    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"Hex"}));
    render_browser(ctx, browser);
    BOOST_TEST(!hex_stored(), "second Hex activation did not clear the toggle");
    const std::string back = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_rows_in_order(back, rows, false) == rows.size(), "decimal rows not restored after Hex off");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// S5: activating node k's `##node` selects k and the field pane then
/// shows node k's heading and rows (or the no-fields text for the
/// unregistered node), for every node, starting from another selection.
BOOST_AUTO_TEST_CASE(NodeClickChangesSelection)
{
    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    BOOST_TEST_REQUIRE(browser.nodes.size() == smbios::test::gui::kSmallTableNodes);
    render_browser(ctx, browser);

    for (const int k : {2, 0, 3, 1}) {
        BOOST_TEST_REQUIRE(ctx.activate_item("##entry_list", {k, "##node"}));
        render_browser(ctx, browser);
        BOOST_TEST(browser.selected == k, "click on node #" << k << ": selected -> " << browser.selected);
        const EntryNode& node = browser.nodes[static_cast<size_t>(k)];
        const std::string text = render_browser_capturing(ctx, browser);
        BOOST_TEST(count_occurrences(text, smbios::gui::entry_node_label(node)) == 2U,
                   "node #" << k << ": label expected in list and heading");
        BOOST_TEST(count_rows_in_order(text, node.fields, false) == node.fields.size(),
                   "node #" << k << ": field rows not shown");
        const size_t no_fields = node.fields.empty() ? 1U : 0U;
        BOOST_TEST(count_occurrences(text, kNoFieldsExplanation) == no_fields,
                   "node #" << k << ": no-fields text expected " << no_fields);
    }
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// S1/S3: 1000 captured frames over a normal table: identical captured
/// length every frame (no per-frame accumulation), balanced stacks, hooks
/// intact, texture count bounded.
BOOST_AUTO_TEST_CASE(ThousandFramesStable)
{
    constexpr int kFrames = 1000;
    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    render_browser(ctx, browser); // layout frame
    const size_t first_length = render_browser_capturing(ctx, browser).size();
    BOOST_TEST_REQUIRE(first_length > 0U);
    int length_changes = 0;
    size_t previous = first_length;
    for (int frame = 1; frame < kFrames; ++frame) {
        const size_t length = render_browser_capturing(ctx, browser).size();
        length_changes += length != previous ? 1 : 0;
        previous = length;
    }
    BOOST_TEST(length_changes == 0, "captured length changed between frames " << length_changes << " time(s)");
    BOOST_TEST(browser.balanced_calls == kFrames + 1);
    BOOST_TEST(browser.unbalanced_calls == 0);
    BOOST_TEST(ctx.texture_count() <= 2, "tracked textures: " << ctx.texture_count());
    BOOST_TEST(ctx.pending_texture_requests() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
