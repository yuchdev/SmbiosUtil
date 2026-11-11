/// @file headless_spike_test.cpp
/// @brief Headless Dear ImGui feasibility spike, items S1-S6 (milestone
/// 0011, Story 04.0, Task 02), as real `gui-ci` cases.
///
/// Type: Unit, CI-safe tier (`smbios_gui_ci_test`, labels gui;gui-ci) -
/// hermetic: no window, no GL, no OS clipboard, synthetic tables only. A
/// case that cannot run fails; nothing here skips.
/// Scenario: one case per spike item. S1 runs 1000 frames on the null
/// renderer; S2 proves every OS hook is replaced and no settings/log file
/// appears; S3 checks the stack-balance oracle around the real
/// smbios::gui::render_entry_browser(); S4 captures rendered text through
/// LogToBuffer(); S5 activates real widgets through ActivateItemByID() with
/// IDs computed from their ID path (no pixel coordinates); S6 records the
/// one duplicate-ID signal 1.92.8 has and what it needs. S7 (process start
/// on the hosted runners) has no case of its own: it is this executable
/// starting at all under CI's `ctest -L gui-ci --timeout 60`.
/// Boundaries: every SMBios is constructed (which reads the live table)
/// and then replaced through load_table() BEFORE any frame, so no live data
/// ever reaches a frame. All fixture strings are synthetic (browsing-UI
/// threat model requirement 20). Assertions on captured text or clipboard
/// bytes use the boolean form with counts only and never print the text
/// (gui-test-tiers threat model R-5): a hostile string could carry `::`
/// workflow commands into the public Actions log.
/// Determinism: each case owns a fresh HeadlessImGuiContext. The one state
/// that outlives a context is copy_as_action.cpp's function-local
/// CopyAsState: S5's Copy click leaves its status at "Copied" for node 0,
/// and no case depends on that status, so the suite passes with
/// `--random=1` (R-10).
/// On failure first check: tests/gui/support/headless_imgui.cpp (hooks,
/// null renderer, ID computation), then whether the imgui version changed
/// (vcpkg baseline bump - see headless_imgui.hpp's symbol list), then
/// smbios_gui/src/entry_tree_view.cpp / copy_as_action.cpp.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/gui/copy_as.hpp>
#include <smbios/gui/entry_tree_view.hpp>
#include <smbios/gui/view_model.hpp>

#include "gui_test_support.hpp"
#include "support/headless_imgui.hpp"
#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

using smbios::SMBios;
using smbios::gui::EntryNode;
using smbios::test::SyntheticTableBuilder;
using smbios::test::gui::count_occurrences;
using smbios::test::gui::HeadlessImGuiContext;

namespace {

/// Formatted-area length of the synthetic BIOS/System Information
/// structures; every byte is 0x02, so numeric fields are non-zero and their
/// decimal and hex renderings differ.
constexpr size_t kFormattedAreaLength = 0x60;

/// Number of nodes in the clipping fixture: far more rows than a 720 px
/// list pane shows, so a normal frame clips most of them.
constexpr int kManyNodes = 200;

/// Synthetic OEM string shown in S4's captured text (a test-defined literal).
constexpr const char* kSyntheticOemString = "synthetic oem one";

/// @brief A formatted area of `kFormattedAreaLength` bytes of 0x02.
std::vector<uint8_t> filled_area()
{
    return std::vector<uint8_t>(kFormattedAreaLength, 0x02);
}

/// @brief The small fixture: BIOS Information (node 0), System Information
/// (node 1), OEM Strings (node 2) and one unregistered OEM type (node 3).
std::vector<uint8_t> small_table()
{
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BIOSInformation, filled_area(), {"Synthetic Vendor", "SYN-BIOS-0001"})
        .add_structure(SMBios::SystemInformation, filled_area(), {"Synthetic Maker", "SYN-SYS-0001"})
        .add_structure(SMBios::OemStrings, {2}, {kSyntheticOemString, "synthetic oem two"})
        .add_structure(0xC8, {0xAA, 0xBB});
    return builder.build();
}

/// Index of the OEM Strings node in small_table().
constexpr int kOemNodeIndex = 2;

/// @brief The clipping fixture: kManyNodes OEM Strings structures.
std::vector<uint8_t> many_nodes_table()
{
    SyntheticTableBuilder builder;
    for (int i = 0; i < kManyNodes; ++i) {
        builder.add_structure(SMBios::OemStrings, {1}, {"synthetic oem row"});
    }
    return builder.build();
}

/// @brief The real browser's inputs over a synthetic table: an SMBios whose
/// live table is replaced through load_table() before anything renders,
/// its view model, and the selection the browser updates.
struct SyntheticBrowser {
    /// Constructed against the live machine, then immediately replaced.
    SMBios bios;
    /// build_entry_tree(bios) of the synthetic table.
    std::vector<EntryNode> nodes;
    /// The browser's selected node index.
    int selected = 0;

    /// @brief Loads `table` into `bios` and builds the view model from it.
    explicit SyntheticBrowser(std::vector<uint8_t> table)
    {
        smbios::test::gui::load_table(bios, std::move(table));
        nodes = smbios::gui::build_entry_tree(bios);
    }

    /// @brief One call of the real smbios::gui::render_entry_browser().
    void render() { smbios::gui::render_entry_browser(bios, nodes, selected); }
};

/// @brief Whether a file or directory `name` exists in the current (CTest
/// working) directory; errors count as "exists", so the check fails closed.
bool exists_in_working_directory(const char* name)
{
    std::error_code ec;
    const bool exists = std::filesystem::exists(std::filesystem::current_path(ec) / name, ec);
    return exists || ec;
}

} // namespace

BOOST_AUTO_TEST_SUITE(GuiHeadlessSpikeTests)

/// S1: 1000 complete frames with a window and text on the null renderer:
/// no assert, every texture request acknowledged, a bounded texture count.
BOOST_AUTO_TEST_CASE(S1HeadlessFrameLoopRuns1000Frames)
{
    constexpr int kFrames = 1000;
    HeadlessImGuiContext ctx;
    int max_textures = 0;
    for (int i = 0; i < kFrames; ++i) {
        ctx.run_frame([] { ImGui::TextUnformatted("headless frame"); });
        max_textures = ctx.texture_count() > max_textures ? ctx.texture_count() : max_textures;
        BOOST_TEST_REQUIRE(ctx.pending_texture_requests() == 0,
                           "S1: frame " << i << ": texture requests left pending: expected 0 -> "
                                        << ctx.pending_texture_requests());
    }
    BOOST_TEST(ctx.frames_run() == kFrames);
    // The font atlas texture is created once (at least one request), and
    // the tracked set does not grow with the frame count.
    BOOST_TEST(ctx.texture_requests_handled() >= 1);
    BOOST_TEST(max_textures >= 1);
    BOOST_TEST(max_textures <= 2, "S1: tracked textures: expected <= 2 -> " << max_textures);
    const ImDrawData* draw_data = ImGui::GetDrawData();
    BOOST_TEST_REQUIRE(draw_data != nullptr);
    BOOST_TEST(draw_data->Valid);
    BOOST_TEST(draw_data->CmdListsCount >= 1);
    BOOST_TEST(draw_data->TotalVtxCount > 0);
}

/// S2: every OS hook is the harness's own; clipboard traffic stays in
/// memory; OpenInShell is refused; no imgui.ini / imgui_log.txt appears in
/// the CTest working directory even after more than io.IniSavingRate (5 s)
/// of simulated frames with logging.
BOOST_AUTO_TEST_CASE(S2NoOsSideEffects)
{
    BOOST_TEST(!exists_in_working_directory("imgui.ini"), "S2: stray imgui.ini before the case");
    BOOST_TEST(!exists_in_working_directory("imgui_log.txt"), "S2: stray imgui_log.txt before the case");
    {
        HeadlessImGuiContext ctx;
        BOOST_TEST(ctx.hooks_intact());
        BOOST_TEST(ImGui::GetIO().IniFilename == nullptr);
        BOOST_TEST(ImGui::GetIO().LogFilename == nullptr);
        BOOST_TEST((ImGui::GetPlatformIO().Platform_SetImeDataFn == nullptr));

        // A test-defined literal through the public clipboard API.
        constexpr const char* kLiteral = "synthetic clipboard literal";
        bool read_back_matches = false;
        ctx.run_frame([&] {
            ImGui::SetClipboardText(kLiteral);
            const char* read_back = ImGui::GetClipboardText();
            read_back_matches = read_back != nullptr && std::string(read_back) == kLiteral;
        });
        BOOST_TEST(ctx.clipboard_set_count() == 1);
        BOOST_TEST((ctx.clipboard() == kLiteral), "S2: in-memory clipboard bytes differ");
        BOOST_TEST(read_back_matches, "S2: GetClipboardText() did not return the in-memory bytes");

        // kIniSavingFrames frames at 1/60 s = 6.7 s > io.IniSavingRate (5 s),
        // logging to the buffer each frame: nothing may reach the disk.
        constexpr int kIniSavingFrames = 400;
        SyntheticBrowser browser(small_table());
        for (int i = 0; i < kIniSavingFrames; ++i) {
            (void)ctx.run_frame_capturing_text([&] { browser.render(); });
        }

        // The OpenInShell hook refuses and only counts; call it directly
        // (nothing in the browser can reach it). Last, because it ends the
        // "never called" invariant run_frame() checks.
        const bool opened = ImGui::GetPlatformIO().Platform_OpenInShellFn(ImGui::GetCurrentContext(),
                                                                          "https://example.invalid/");
        BOOST_TEST(!opened);
        BOOST_TEST(ctx.open_in_shell_count() == 1);
    }
    BOOST_TEST((ImGui::GetCurrentContext() == nullptr), "S2: the destructor left a current context");
    BOOST_TEST(!exists_in_working_directory("imgui.ini"), "S2: imgui.ini written to the working directory");
    BOOST_TEST(!exists_in_working_directory("imgui_log.txt"), "S2: imgui_log.txt written to the working directory");
}

/// S3: ErrorRecoveryStoreState() snapshots before and after the real
/// render_entry_browser() are equal for every selected node and for the
/// empty table, and the oracle does detect a deliberate imbalance. The
/// oracle compares stack sizes directly, so it works whether or not
/// IM_ASSERT is compiled in.
BOOST_AUTO_TEST_CASE(S3BalancedStacksAroundRenderEntryBrowser)
{
    HeadlessImGuiContext ctx;
    SyntheticBrowser browser(small_table());
    BOOST_TEST_REQUIRE(browser.nodes.size() == 4U);

    for (int node = 0; node < static_cast<int>(browser.nodes.size()); ++node) {
        // Two frames per node: the first lays out, the second is steady state.
        for (int frame = 0; frame < 2; ++frame) {
            ImGuiErrorRecoveryState before;
            ImGuiErrorRecoveryState after;
            browser.selected = node;
            ctx.run_frame([&] {
                ImGui::ErrorRecoveryStoreState(&before);
                browser.render();
                ImGui::ErrorRecoveryStoreState(&after);
            });
            BOOST_TEST(smbios::test::gui::stack_sizes_equal(before, after),
                       "S3: node #" << node << " frame " << frame << ": unbalanced stacks: expected 0 -> "
                                    << smbios::test::gui::stack_size_mismatches(before, after));
        }
    }

    SyntheticBrowser empty(SyntheticTableBuilder().build());
    BOOST_TEST_REQUIRE(empty.nodes.empty());
    ImGuiErrorRecoveryState before;
    ImGuiErrorRecoveryState after;
    ctx.run_frame([&] {
        ImGui::ErrorRecoveryStoreState(&before);
        empty.render();
        ImGui::ErrorRecoveryStoreState(&after);
    });
    BOOST_TEST(smbios::test::gui::stack_sizes_equal(before, after), "S3: empty table: unbalanced stacks");

    // Negative control: the oracle sees one extra ID-stack entry. Popped
    // again before End(), so ImGui's own error recovery never triggers.
    int control_mismatches = 0;
    ctx.run_frame([&] {
        ImGuiErrorRecoveryState control_before;
        ImGuiErrorRecoveryState control_after;
        ImGui::ErrorRecoveryStoreState(&control_before);
        ImGui::PushID("s3_negative_control");
        ImGui::ErrorRecoveryStoreState(&control_after);
        ImGui::PopID();
        control_mismatches = smbios::test::gui::stack_size_mismatches(control_before, control_after);
    });
    BOOST_TEST(control_mismatches == 1, "S3: negative control: expected 1 mismatch -> " << control_mismatches);
}

/// S4: LogToBuffer() captures the text the real browser draws (node labels,
/// field names and values, fixed widget text). Finding: logging disables
/// clipping, so rows a normal frame clips away ARE in the capture.
BOOST_AUTO_TEST_CASE(S4TextCaptureSeesRenderedText)
{
    HeadlessImGuiContext ctx;
    SyntheticBrowser browser(small_table());
    browser.selected = kOemNodeIndex;
    ctx.run_frame([&] { browser.render(); }); // layout frame
    const std::string text = ctx.run_frame_capturing_text([&] { browser.render(); });

    BOOST_TEST(!text.empty(), "S4: nothing captured");
    // One list label per node, plus the right pane's heading.
    const size_t labels = count_occurrences(text, "(handle 0x");
    BOOST_TEST(labels == browser.nodes.size() + 1,
               "S4: node labels: expected " << browser.nodes.size() + 1 << " -> " << labels);
    BOOST_TEST(count_occurrences(text, "OEM Strings") >= 2U, "S4: OEM Strings label/heading missing");
    BOOST_TEST(count_occurrences(text, kSyntheticOemString) == 1U,
               "S4: synthetic OEM string #1: expected 1 -> " << count_occurrences(text, kSyntheticOemString));
    BOOST_TEST(count_occurrences(text, "Copy as") == 1U, "S4: 'Copy as' label missing");
    BOOST_TEST(count_occurrences(text, "Hex") >= 1U, "S4: 'Hex' checkbox label missing");

    // Clipping: kManyNodes rows cannot fit the 720 px pane, but the capture
    // still holds every label - LogToBuffer() disables ImGuiListClipper.
    SyntheticBrowser many(many_nodes_table());
    BOOST_TEST_REQUIRE(many.nodes.size() == static_cast<size_t>(kManyNodes));
    ctx.run_frame([&] { many.render(); }); // layout frame
    const std::string many_text = ctx.run_frame_capturing_text([&] { many.render(); });
    const size_t many_labels = count_occurrences(many_text, "(handle 0x");
    BOOST_TEST(many_labels == static_cast<size_t>(kManyNodes) + 1,
               "S4: logged labels with clipping: expected " << kManyNodes + 1 << " -> " << many_labels);
}

/// S5: the ID-path locator activates real widgets with no pixel
/// coordinates: a node click, the Hex checkbox, the Copy button (into the
/// in-memory clipboard) and the format combo.
BOOST_AUTO_TEST_CASE(S5LocatorActivatesWidgetsWithoutPixels)
{
    HeadlessImGuiContext ctx;
    SyntheticBrowser browser(small_table());
    BOOST_TEST_REQUIRE(browser.nodes.size() == 4U);
    browser.selected = 0;
    ctx.run_frame([&] { browser.render(); });

    BOOST_TEST_REQUIRE(ctx.find_child_window("##entry_list") != nullptr, "S5: ##entry_list child not found");
    BOOST_TEST_REQUIRE(ctx.find_child_window("##entry_fields") != nullptr, "S5: ##entry_fields child not found");

    // Node click: node #1's Selectable, PushID(1) + "##node".
    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_list", {1, "##node"}));
    ctx.run_frame([&] { browser.render(); });
    BOOST_TEST(browser.selected == 1, "S5: node click: selected expected 1 -> " << browser.selected);

    // Hex checkbox, on the first node with uint64_t fields (only those
    // change with Hex: each gains exactly one "0x" prefix). The toggle
    // lives in the ##entry_fields child's state storage.
    int hex_node = -1;
    size_t uint64_fields = 0;
    for (size_t i = 0; i < browser.nodes.size() && hex_node < 0; ++i) {
        for (const smbios::gui::FieldRow& row : browser.nodes[i].fields) {
            uint64_fields += std::holds_alternative<uint64_t>(row.value) ? size_t{1} : size_t{0};
        }
        if (uint64_fields > 0) {
            hex_node = static_cast<int>(i);
        }
    }
    BOOST_TEST_REQUIRE(hex_node >= 0, "S5: fixture has no node with uint64_t fields");
    browser.selected = hex_node;
    ctx.run_frame([&] { browser.render(); });
    const std::string decimal_text = ctx.run_frame_capturing_text([&] { browser.render(); });
    const std::optional<ImGuiID> hex_storage_id = ctx.find_item_id("##entry_fields", {"##hex_toggle"});
    BOOST_TEST_REQUIRE(hex_storage_id.has_value());
    BOOST_TEST(!ctx.find_child_window("##entry_fields")->StateStorage.GetBool(*hex_storage_id, false));
    BOOST_TEST(count_occurrences(decimal_text, "[ ] Hex") == 1U, "S5: unchecked Hex checkbox not logged");
    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"Hex"}));
    ctx.run_frame([&] { browser.render(); });
    BOOST_TEST(ctx.find_child_window("##entry_fields")->StateStorage.GetBool(*hex_storage_id, false),
               "S5: Hex checkbox did not toggle");
    const std::string hex_text = ctx.run_frame_capturing_text([&] { browser.render(); });
    BOOST_TEST(count_occurrences(hex_text, "[x] Hex") == 1U, "S5: checked Hex checkbox not logged");
    const size_t decimal_hex_prefixes = count_occurrences(decimal_text, "0x");
    const size_t hex_hex_prefixes = count_occurrences(hex_text, "0x");
    BOOST_TEST(hex_hex_prefixes == decimal_hex_prefixes + uint64_fields,
               "S5: node #" << hex_node << ": '0x' count after Hex: expected "
                            << decimal_hex_prefixes + uint64_fields << " -> " << hex_hex_prefixes);

    // Copy button (default format) on node #0: the bytes must equal
    // render_entry_as() of node #0's entry in exactly one registered format.
    browser.selected = 0;
    ctx.run_frame([&] { browser.render(); });
    const int sets_before = ctx.clipboard_set_count();
    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"##copy_as", "Copy"}));
    ctx.run_frame([&] { browser.render(); });
    BOOST_TEST(ctx.clipboard_set_count() == sets_before + 1,
               "S5: Copy: clipboard writes expected " << sets_before + 1 << " -> " << ctx.clipboard_set_count());
    {
        const std::unique_ptr<smbios::AbstractSMBiosEntry> entry =
            smbios::gui::create_entry_for_node(browser.bios, 0, browser.nodes[0]);
        BOOST_TEST_REQUIRE(entry != nullptr);
        int matching_formats = 0;
        for (const std::string& name : smbios::registered_output_formatter_names()) {
            const std::optional<std::string> rendered = smbios::gui::render_entry_as(*entry, name);
            if (rendered && *rendered == ctx.clipboard()) {
                ++matching_formats;
            }
        }
        BOOST_TEST(matching_formats == 1,
                   "S5: Copy: formats whose output equals the clipboard: expected 1 -> " << matching_formats);
    }

    // Format combo: activation opens its popup (BeginCombo's popup ID is
    // "##ComboPopup" hashed with the combo's own ID).
    const std::optional<ImGuiID> combo_id = ctx.find_item_id("##entry_fields", {"##copy_as", "##format"});
    BOOST_TEST_REQUIRE(combo_id.has_value());
    const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, *combo_id);
    auto popup_open = [&] {
        for (const ImGuiPopupData& popup : GImGui->OpenPopupStack) {
            if (popup.PopupId == popup_id) {
                return true;
            }
        }
        return false;
    };
    BOOST_TEST(!popup_open());
    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"##copy_as", "##format"}));
    ctx.run_frame([&] { browser.render(); });
    ctx.run_frame([&] { browser.render(); });
    BOOST_TEST(popup_open(), "S5: format combo did not open");
}

/// S6: Dear ImGui 1.92.8 has one duplicate-ID signal,
/// ImGuiContext::DebugDrawIdConflictsId (io.ConfigDebugHighlightIdConflicts,
/// default on). It is hover-scoped: it is only computed for the item under
/// the mouse in the previous frame, so it can only check an item whose
/// position the test knows. Shown here on test-owned buttons, positioned
/// from their own item rectangle (no pixel constants).
BOOST_AUTO_TEST_CASE(S6IdConflictSignalIsHoverScoped)
{
    // Returns DebugDrawIdConflictsId after hovering the first of two
    // buttons for a few frames.
    auto conflict_signal = [](const char* second_label) {
        HeadlessImGuiContext ctx;
        BOOST_TEST(ImGui::GetIO().ConfigDebugHighlightIdConflicts);
        ImVec2 first_center(0.0F, 0.0F);
        auto body = [&] {
            ImGui::Button("s6_button");
            first_center = ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5F,
                                  (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5F);
            ImGui::Button(second_label);
        };
        ctx.run_frame(body);
        ImGuiID signal = 0;
        for (int frame = 0; frame < 4 && signal == 0; ++frame) {
            ImGui::GetIO().AddMousePosEvent(first_center.x, first_center.y);
            ctx.run_frame(body);
            signal = GImGui->DebugDrawIdConflictsId;
        }
        return signal;
    };

    BOOST_TEST(conflict_signal("s6_button") != 0U, "S6: duplicate IDs under the mouse not signalled");
    BOOST_TEST(conflict_signal("s6_other_button") == 0U, "S6: unique IDs signalled as a conflict");
}

BOOST_AUTO_TEST_SUITE_END()
