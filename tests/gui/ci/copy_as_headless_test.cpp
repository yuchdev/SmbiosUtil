/// @file copy_as_headless_test.cpp
/// @brief Headless widget tests of smbios::gui::render_copy_as_action()
/// through the real render_entry_browser() (milestone 0011, Story 04.0,
/// Task 03): suite GuiCopyAsHeadlessTests.
///
/// Type: Unit, CI-safe tier (`smbios_gui_ci_test`, labels gui;gui-ci) -
/// hermetic: no window, no GL, the clipboard is HeadlessImGuiContext's
/// in-memory hook (never the OS clipboard), synthetic tables only. Nothing
/// skips.
/// Scenario: the real copy_as_action.cpp (compiled into this executable,
/// never into libsmbios_gui_model) is driven through the S5 locator: the
/// format combo is opened by activating `##copy_as/##format`, an item is
/// chosen by activating `<i>/##format_item` inside the combo's popup
/// window, and Copy by activating `##copy_as/Copy`. Oracles: S4 captured
/// text (the combo preview is logged as `{ <name> }`, the open popup's
/// items one per line before `[ Copy ]`), S2's in-memory clipboard, and
/// Dear ImGui's open-popup stack.
/// Boundaries: default vs. non-JSON format, the exact registered name list,
/// every registered node of a table, output over kCopyAsMaxBytes (two
/// 600 KiB OEM strings), nodes without fields (unregistered, decode
/// failed).
/// Determinism (threat model R-10): copy_as_action.cpp keeps a
/// function-local `static CopyAsState` (remembered format, last status)
/// that outlives every HeadlessImGuiContext. Invariant kept by this suite:
/// between cases the remembered format is `json`. Every case that may
/// change it holds a JsonFormatRestorer, which re-selects `json` through
/// the combo when the case ends (also on failure) and fails the case if it
/// cannot; cases that click Copy also select `json` first, so they do not
/// depend on the order cases run in. DefaultFormatIsJsonNoUnsafeNote
/// deliberately does NOT reset first: it checks the invariant (which, with
/// no case leaving another format behind, is the initial default). The
/// remembered status is only ever asserted right after this case's own
/// click. Passes with `--random=1`.
/// Assertions never print captured text, clipboard bytes or fixture text:
/// boolean/count forms with fixed messages only (threat model
/// docs/security/2026-11-11-gui-test-tiers.md, R-5).
/// On failure first check: smbios_gui/src/copy_as_action.cpp, then
/// copy_as.cpp (render_entry_as/create_entry_for_node, also covered by
/// tests/unit/gui_copy_as_test.cpp), then tests/gui/support/.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/gui/copy_as.hpp>
#include <smbios/gui/copy_as_action.hpp>
#include <smbios/gui/field_text.hpp>
#include <smbios/gui/view_model.hpp>

#include "support/browser_fixture.hpp"
#include "support/headless_imgui.hpp"
#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

using smbios::SMBios;
using smbios::gui::EntryNode;
using smbios::test::SyntheticTableBuilder;
using smbios::test::gui::BrowserFixture;
using smbios::test::gui::count_occurrences;
using smbios::test::gui::HeadlessImGuiContext;
using smbios::test::gui::render_browser;
using smbios::test::gui::render_browser_capturing;

namespace {

/// Fixed status text after a successful Copy (copy_as_action.cpp).
constexpr const char* kCopiedText = "Copied to clipboard.";

/// Fixed status text prefix after a refused, oversized Copy.
constexpr const char* kTooLargeText = "Entry too large to copy";

/// How the Copy button is logged (spike S4 format).
constexpr const char* kCopyButtonText = "[ Copy ]";

/// @brief How the combo preview of `name` is logged: `{ <name> }`.
std::string preview_text(std::string_view name)
{
    return "{ " + std::string(name) + " }";
}

/// @brief Position of `name` in the registered format names, or -1.
int format_index(std::string_view name)
{
    const std::vector<std::string> names = smbios::registered_output_formatter_names();
    const auto it = std::find(names.begin(), names.end(), name);
    return it == names.end() ? -1 : static_cast<int>(it - names.begin());
}

/// @brief The format combo's popup window if it is open, else nullptr.
/// BeginCombo() derives the popup ID as `"##ComboPopup"` hashed with the
/// combo's own ID.
ImGuiWindow* open_combo_popup(const HeadlessImGuiContext& ctx)
{
    const std::optional<ImGuiID> combo_id = ctx.find_item_id("##entry_fields", {"##copy_as", "##format"});
    if (!combo_id) {
        return nullptr;
    }
    const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, *combo_id);
    for (const ImGuiPopupData& popup : GImGui->OpenPopupStack) {
        if (popup.PopupId == popup_id) {
            return popup.Window;
        }
    }
    return nullptr;
}

/// @brief Opens the format combo of the selected node (S5) and renders two
/// frames so its popup window exists.
/// @return the popup window, or nullptr if it did not open.
ImGuiWindow* open_combo(HeadlessImGuiContext& ctx, BrowserFixture& browser)
{
    if (!ctx.activate_item("##entry_fields", {"##copy_as", "##format"})) {
        return nullptr;
    }
    render_browser(ctx, browser);
    render_browser(ctx, browser);
    return open_combo_popup(ctx);
}

/// @brief Selects format `name` through the combo, exactly as a user would
/// (open, activate `<i>/##format_item` in the popup), on the selected node,
/// which must show the Copy controls.
/// @return true iff the combo preview then reads `name`.
bool select_format(HeadlessImGuiContext& ctx, BrowserFixture& browser, std::string_view name)
{
    const int index = format_index(name);
    if (index < 0) {
        return false;
    }
    render_browser(ctx, browser);
    ImGuiWindow* popup = open_combo(ctx, browser);
    if (popup == nullptr) {
        return false;
    }
    ImGui::ActivateItemByID(ImGui::GetIDWithSeed("##format_item", nullptr, ImGui::GetIDWithSeed(index, popup->ID)));
    render_browser(ctx, browser);
    render_browser(ctx, browser);
    const std::string text = render_browser_capturing(ctx, browser);
    return count_occurrences(text, preview_text(name)) == 1U && open_combo_popup(ctx) == nullptr;
}

/// @brief Restores copy_as_action.cpp's remembered format to `json` when
/// the case ends (also when it fails or throws), keeping this suite's
/// between-cases invariant (R-10). Declare it after the context and the
/// fixture, so it is destroyed before them.
class JsonFormatRestorer {
public:
    /// @param ctx the case's context.
    /// @param browser the case's fixture.
    /// @param copy_node a node of `browser` that shows the Copy controls.
    /// @param reset_now also select `json` immediately (for cases that must
    /// not depend on the format any earlier case left behind).
    JsonFormatRestorer(HeadlessImGuiContext& ctx, BrowserFixture& browser, int copy_node, bool reset_now)
        : ctx_(ctx), browser_(browser), copy_node_(copy_node)
    {
        if (reset_now) {
            browser_.selected = copy_node_;
            BOOST_TEST_REQUIRE(select_format(ctx_, browser_, smbios::gui::kDefaultCopyFormat),
                               "could not select json before the case");
        }
    }

    ~JsonFormatRestorer()
    {
        try {
            browser_.selected = copy_node_;
            const bool restored = select_format(ctx_, browser_, smbios::gui::kDefaultCopyFormat);
            BOOST_TEST(restored, "could not restore the remembered Copy-as format to json (R-10)");
        } catch (...) {
            BOOST_TEST(false, "restoring the Copy-as format threw (R-10)");
        }
    }

    JsonFormatRestorer(const JsonFormatRestorer&) = delete;
    JsonFormatRestorer& operator=(const JsonFormatRestorer&) = delete;
    JsonFormatRestorer(JsonFormatRestorer&&) = delete;
    JsonFormatRestorer& operator=(JsonFormatRestorer&&) = delete;

private:
    /// The case's context.
    HeadlessImGuiContext& ctx_;
    /// The case's fixture.
    BrowserFixture& browser_;
    /// A node showing the Copy controls.
    int copy_node_;
};

/// @brief The lines between the combo preview and the Copy button in a
/// frame captured while the combo is open: the popup's items, one per
/// line, trimmed, empty lines dropped. Empty if the markers are missing.
std::vector<std::string> captured_popup_items(const std::string& text)
{
    std::vector<std::string> items;
    const size_t label = text.find("Copy as {");
    if (label == std::string::npos) {
        return items;
    }
    const size_t preview_end = text.find('}', label);
    const size_t button = text.find(kCopyButtonText, label);
    if (preview_end == std::string::npos || button == std::string::npos || button < preview_end) {
        return items;
    }
    const std::string section = text.substr(preview_end + 1, button - preview_end - 1);
    size_t start = 0;
    while (start <= section.size()) {
        size_t end = section.find('\n', start);
        if (end == std::string::npos) {
            end = section.size();
        }
        std::string line = section.substr(start, end - start);
        const size_t first = line.find_first_not_of(" \r");
        const size_t last = line.find_last_not_of(" \r");
        if (first != std::string::npos) {
            items.push_back(line.substr(first, last - first + 1));
        }
        start = end + 1;
    }
    return items;
}

/// Node of small_table() used as the "has Copy controls" node.
constexpr int kCopyNode = 0;

} // namespace

BOOST_AUTO_TEST_SUITE(GuiCopyAsHeadlessTests)

/// M-08: the combo shows `json` - the default, and this suite's
/// between-cases invariant - and the unsafe-format note is absent.
BOOST_AUTO_TEST_CASE(DefaultFormatIsJsonNoUnsafeNote)
{
    const std::vector<std::string> names = smbios::registered_output_formatter_names();
    BOOST_TEST_REQUIRE(format_index(smbios::gui::kDefaultCopyFormat) >= 0, "json formatter not registered");
    BOOST_TEST((names[smbios::gui::default_copy_format_index(names)] == smbios::gui::kDefaultCopyFormat),
               "default_copy_format_index() does not name json");

    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    const JsonFormatRestorer restorer(ctx, browser, kCopyNode, false);
    browser.selected = kCopyNode;
    render_browser(ctx, browser);
    const std::string text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(text, "Copy as") == 1U, "'Copy as' label missing");
    BOOST_TEST(count_occurrences(text, preview_text(smbios::gui::kDefaultCopyFormat)) == 1U,
               "combo preview is not json");
    BOOST_TEST(count_occurrences(text, smbios::gui::kCopyAsUnsafeFormatNote) == 0U, "unsafe-format note shown for json");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// M-08 (S5): choosing `yaml` in the combo shows the unsafe-format note;
/// choosing `json` again removes it.
BOOST_AUTO_TEST_CASE(NonJsonFormatShowsUnsafeNote)
{
    BOOST_TEST_REQUIRE(format_index("yaml") >= 0, "yaml formatter not registered");
    BOOST_TEST_REQUIRE(!smbios::gui::is_terminal_safe_format("yaml"));

    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    const JsonFormatRestorer restorer(ctx, browser, kCopyNode, true);
    browser.selected = kCopyNode;

    BOOST_TEST_REQUIRE(select_format(ctx, browser, "yaml"), "could not select yaml through the combo");
    const std::string yaml_text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(yaml_text, preview_text("yaml")) == 1U, "combo preview is not yaml");
    BOOST_TEST(count_occurrences(yaml_text, smbios::gui::kCopyAsUnsafeFormatNote) == 1U,
               "unsafe-format note missing for yaml");

    BOOST_TEST_REQUIRE(select_format(ctx, browser, smbios::gui::kDefaultCopyFormat), "could not reselect json");
    const std::string json_text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(json_text, smbios::gui::kCopyAsUnsafeFormatNote) == 0U,
               "unsafe-format note still shown after reselecting json");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// M-08 (S5): the open combo lists exactly
/// registered_output_formatter_names() - sorted, no extras, no omissions.
/// Asserted against the function, not a literal list: this executable links
/// only the built-in formatters (no csv, no unit-suite fakes).
BOOST_AUTO_TEST_CASE(ComboListsExactlyRegisteredNames)
{
    const std::vector<std::string> names = smbios::registered_output_formatter_names();
    BOOST_TEST_REQUIRE(!names.empty());
    BOOST_TEST(std::is_sorted(names.begin(), names.end()), "registered names are not sorted");

    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    browser.selected = kCopyNode;
    render_browser(ctx, browser);
    BOOST_TEST_REQUIRE(open_combo(ctx, browser) != nullptr, "format combo did not open");
    const std::string text = render_browser_capturing(ctx, browser);
    BOOST_TEST_REQUIRE(open_combo_popup(ctx) != nullptr, "format combo closed while capturing");

    const std::vector<std::string> items = captured_popup_items(text);
    BOOST_TEST(items.size() == names.size(), "combo items: expected " << names.size() << " -> " << items.size());
    BOOST_TEST((items == names), "combo items differ from registered_output_formatter_names()");
    BOOST_TEST(std::is_sorted(items.begin(), items.end()), "combo items are not sorted");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// M-09 click path without the OS (S2/S5): Copy on every registered node
/// puts exactly render_entry_as(create_entry_for_node(...), "json") on the
/// in-memory clipboard, one write per click, and reports the fixed
/// `Copied to clipboard.` status.
BOOST_AUTO_TEST_CASE(CopyClickPutsRenderEntryAsBytesOnClipboard)
{
    HeadlessImGuiContext ctx;
    BrowserFixture browser(smbios::test::gui::small_table());
    const JsonFormatRestorer restorer(ctx, browser, kCopyNode, true);

    int copied_nodes = 0;
    for (int node = 0; node < static_cast<int>(browser.nodes.size()); ++node) {
        const EntryNode& entry_node = browser.nodes[static_cast<size_t>(node)];
        if (entry_node.fields.empty() || entry_node.decode_failed) {
            continue;
        }
        std::optional<std::string> expected;
        {
            const std::unique_ptr<smbios::AbstractSMBiosEntry> entry =
                smbios::gui::create_entry_for_node(browser.bios, static_cast<size_t>(node), entry_node);
            BOOST_TEST_REQUIRE(entry != nullptr, "node #" << node << ": no entry");
            expected = smbios::gui::render_entry_as(*entry, smbios::gui::kDefaultCopyFormat);
        }
        BOOST_TEST_REQUIRE(expected.has_value(), "node #" << node << ": json rendering refused");

        browser.selected = node;
        render_browser(ctx, browser);
        const int writes_before = ctx.clipboard_set_count();
        BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"##copy_as", "Copy"}));
        render_browser(ctx, browser);
        BOOST_TEST(ctx.clipboard_set_count() == writes_before + 1,
                   "node #" << node << ": clipboard writes expected " << writes_before + 1 << " -> "
                            << ctx.clipboard_set_count());
        BOOST_TEST((ctx.clipboard() == *expected), "node #" << node << ": clipboard bytes differ from render_entry_as()");
        const std::string text = render_browser_capturing(ctx, browser);
        BOOST_TEST(count_occurrences(text, kCopiedText) == 1U, "node #" << node << ": copied status missing");
        ++copied_nodes;
    }
    BOOST_TEST(copied_nodes == smbios::test::gui::kSmallTableUnknownNode, "registered nodes copied: " << copied_nodes);
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// G-5: an entry whose rendering exceeds kCopyAsMaxBytes (OEM Strings with
/// two 600 KiB strings) is refused - the clipboard is not written - and the
/// fixed `Entry too large to copy` status is shown. The cap is not weakened.
BOOST_AUTO_TEST_CASE(CopyRefusesOversizedOutput)
{
    const std::string chunk(600 * 1024, 'A');
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {2}, {chunk, chunk});

    HeadlessImGuiContext ctx;
    BrowserFixture browser(builder.build());
    BOOST_TEST_REQUIRE(browser.nodes.size() == 1U);
    BOOST_TEST_REQUIRE(!browser.nodes[0].fields.empty());
    {
        const std::unique_ptr<smbios::AbstractSMBiosEntry> entry =
            smbios::gui::create_entry_for_node(browser.bios, 0, browser.nodes[0]);
        BOOST_TEST_REQUIRE(entry != nullptr);
        const smbios::IOutputFormatter* json = smbios::get_output_formatter(std::string(smbios::gui::kDefaultCopyFormat));
        BOOST_TEST_REQUIRE(json != nullptr);
        BOOST_TEST_REQUIRE(json->render(*entry).size() > smbios::gui::kCopyAsMaxBytes,
                           "fixture does not exceed the copy cap");
    }
    const JsonFormatRestorer restorer(ctx, browser, 0, true);
    browser.selected = 0;
    render_browser(ctx, browser);

    const int writes_before = ctx.clipboard_set_count();
    const std::string clipboard_before = ctx.clipboard();
    BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"##copy_as", "Copy"}));
    render_browser(ctx, browser);
    BOOST_TEST(ctx.clipboard_set_count() == writes_before, "oversized Copy wrote the clipboard");
    BOOST_TEST((ctx.clipboard() == clipboard_before), "oversized Copy changed the clipboard bytes");
    const std::string text = render_browser_capturing(ctx, browser);
    BOOST_TEST(count_occurrences(text, kTooLargeText) == 1U, "too-large status missing");
    BOOST_TEST(count_occurrences(text, kCopiedText) == 0U, "copied status shown for a refused Copy");
    BOOST_TEST(browser.unbalanced_calls == 0);
}

/// G-7/G-10: a node without decoded fields - unregistered type or decode
/// failure - shows no Copy controls: nothing Copy-related is captured, the
/// combo cannot be opened and activating the Copy ID writes nothing.
BOOST_AUTO_TEST_CASE(NoCopyControlsForEmptyFieldNode)
{
    EntryNode failed;
    failed.type = SMBios::BIOSInformation;
    failed.handle = 0x0009;
    failed.label = smbios::gui::entry_type_label(failed.type);
    failed.decode_failed = true;

    HeadlessImGuiContext ctx;
    BrowserFixture unknown(smbios::test::gui::small_table());
    unknown.selected = smbios::test::gui::kSmallTableUnknownNode;
    BOOST_TEST_REQUIRE(unknown.nodes.at(static_cast<size_t>(unknown.selected)).fields.empty());
    BrowserFixture decode_failed(SyntheticTableBuilder().build(), {failed});

    int checked = 0;
    for (BrowserFixture* browser : {&unknown, &decode_failed}) {
        render_browser(ctx, *browser);
        const std::string text = render_browser_capturing(ctx, *browser);
        BOOST_TEST(count_occurrences(text, "Copy") == 0U, "fixture #" << checked << ": Copy text captured");
        BOOST_TEST(count_occurrences(text, smbios::gui::kCopyAsUnsafeFormatNote) == 0U,
                   "fixture #" << checked << ": unsafe-format note captured");
        BOOST_TEST(count_occurrences(text, "{ ") == 0U, "fixture #" << checked << ": a combo preview was captured");

        BOOST_TEST((open_combo(ctx, *browser) == nullptr), "fixture #" << checked << ": format combo opened");
        const int writes_before = ctx.clipboard_set_count();
        BOOST_TEST_REQUIRE(ctx.activate_item("##entry_fields", {"##copy_as", "Copy"}));
        render_browser(ctx, *browser);
        render_browser(ctx, *browser);
        BOOST_TEST(ctx.clipboard_set_count() == writes_before, "fixture #" << checked << ": clipboard written");
        BOOST_TEST(browser->unbalanced_calls == 0);
        ++checked;
    }
    BOOST_TEST(checked == 2);
}

BOOST_AUTO_TEST_SUITE_END()
