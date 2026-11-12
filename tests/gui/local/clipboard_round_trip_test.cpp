#include "support/local_only_guard.hpp"

/// @file clipboard_round_trip_test.cpp
/// @brief Local-only "Copy as..." checks against the live table and the
/// `smbios` CLI (milestone 0011, Story 04.0 Task 04; manual-testing.md
/// M-04, M-09, M-10).
///
/// Split per threat model `docs/security/2026-11-11-gui-test-tiers.md` T-2 /
/// R-18 (which overrides the task spec's "every node through the OS
/// clipboard"):
/// - `CopyAsBytesMatchCliEveryNodeAndFormat` checks every registered node in
///   every GUI format **in memory only** - no clipboard at all.
/// - `ClipboardRoundTripMatchesCli` uses the **real OS clipboard for exactly
///   4 writes** (one per format) of one entry whose fields carry no device
///   identifier (find_identifier_free_node(): type 0, BIOS Information,
///   when present), and restores the user's clipboard text afterwards.
///
/// - Type: integration (local-only tier, labels `gui;gui-local`).
/// - Output: counts, node indices, type codes, handles and literal names
///   only; rendered entries, CLI output and clipboard text stay in memory
///   (T-3, R-20). The OS case prints one notice before its first clipboard
///   write and one after restoring.
/// - Skips: as root (R-19); the OS case also with no display/GL, with no
///   readable table, or with no identifier-free entry (then the OS leg is
///   not run at all; manual-testing.md records the accepted gap). Every
///   skip decision is made before the first assertion.
/// - On failure, first check: that `smbios` was rebuilt with this target;
///   for the OS case, whether a clipboard manager rewrote the clipboard
///   during the run (the guard then leaves the clipboard alone and says so).

#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <GLFW/glfw3.h>

#include <libsmbios/output_formatter.hpp>
#include <smbios/gui/copy_as.hpp>

#include "support/display_probe.hpp"
#include "support/glfw_test_window.hpp"
#include "support/live_table_support.hpp"
#include "support/local_skip.hpp"
#include "support/process_spawn.hpp"
#include "support/scoped_temp_directory.hpp"

namespace utf = boost::unit_test;
using smbios::test::gui::DiscrepancyReport;
using smbios::test::gui::display_and_gl_available;
using smbios::test::gui::entry_for_node;
using smbios::test::gui::find_bytes;
using smbios::test::gui::find_identifier_free_node;
using smbios::test::gui::GlfwTestWindow;
using smbios::test::gui::GuiCopyFormat;
using smbios::test::gui::hex_handle;
using smbios::test::gui::kGuiCopyFormats;
using smbios::test::gui::LiveGuiModel;
using smbios::test::gui::NodeKind;
using smbios::test::gui::read_live_gui_model;
using smbios::test::gui::run_cli;
using smbios::test::gui::same_bytes;
using smbios::test::gui::ScopedTempDirectory;
using smbios::test::gui::skip_if_root;
using smbios::test::gui::skip_unless;
using smbios::test::gui::split_json_array;
using smbios::test::gui::SpawnResult;
using smbios::test::gui::status_name;

namespace {

constexpr const char* kInMemoryCase = "CopyAsBytesMatchCliEveryNodeAndFormat";
constexpr const char* kClipboardCase = "ClipboardRoundTripMatchesCli";

/// @return `<format> registered node #<p> (node #<i>, type <t>, handle 0xHHHH)`.
std::string node_where(std::string_view format, std::size_t position, std::size_t node_index,
                       const smbios::gui::EntryNode& node) {
    return std::string(format) + " registered node #" + std::to_string(position) + " (node #"
         + std::to_string(node_index) + ", type " + std::to_string(node.type) + ", handle "
         + hex_handle(node.handle) + ")";
}

/// @brief True iff the formatter registry of this binary is exactly
/// kGuiCopyFormats (what the GUI's dropdown offers); reports differences.
bool registry_matches_gui_formats(DiscrepancyReport& report) {
    const std::vector<std::string> names = smbios::registered_output_formatter_names();
    if (names.size() != kGuiCopyFormats.size()) {
        report.add("registered formats: " + std::to_string(kGuiCopyFormats.size()) + " -> "
                   + std::to_string(names.size()));
        return false;
    }
    bool matches = true;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!same_bytes(kGuiCopyFormats[i].name, names[i])) {
            report.add("registered format #" + std::to_string(i) + ": " + std::string(kGuiCopyFormats[i].name)
                       + " -> another name");
            matches = false;
        }
    }
    return matches;
}

/// @brief Saves the user's clipboard text, and restores it after the test's
/// writes - but only if the clipboard still holds the test's last write
/// (threat model T-2/R-18). Construct it after the GLFW window exists and
/// destroy it before the window (it lives inside the GLFW lifetime).
///
/// - A NULL saved value (empty or non-text clipboard) restores to an empty
///   string, so live data never stays on the clipboard.
/// - Non-text clipboard content cannot be saved and is lost (ruled and
///   documented in manual-testing.md).
/// - Prints one notice before the first write and one after restore();
///   never prints the saved text or anything written.
/// - The destructor calls restore() if the case did not (failure path).
///   No signal handlers: a crash, kill or Ctrl-C leaves the last write on
///   the clipboard (documented residue).
class ClipboardRestoreGuard {
public:
    /// Result of restore().
    enum class Outcome {
        /// write() was never called; the clipboard was not touched.
        NothingWritten,
        /// The saved text was put back and read back identical.
        Restored,
        /// The clipboard no longer held the test's last write (the user or a
        /// clipboard manager changed it): left alone, not restored.
        ChangedByOthersNotRestored,
        /// The saved text was put back but did not read back identical.
        RestoreNotVerified,
    };

    /// @brief Saves the current clipboard text of `window`'s session.
    ClipboardRestoreGuard(GLFWwindow* window, std::string_view case_name)
        : window_(window), case_name_(case_name) {
        const char* const saved = glfwGetClipboardString(window_);
        saved_ = saved != nullptr ? std::string(saved) : std::string();
    }
    ClipboardRestoreGuard(const ClipboardRestoreGuard&) = delete;
    ClipboardRestoreGuard& operator=(const ClipboardRestoreGuard&) = delete;
    /// @brief Restores if the case did not call restore() itself.
    ~ClipboardRestoreGuard() { static_cast<void>(restore()); }

    /// @brief Puts `bytes` on the clipboard; prints the notice first, once.
    void write(const std::string& bytes) {
        if (!written_) {
            std::cout << "NOTICE: " << case_name_
                      << " overwrites then restores the clipboard text (one write per format)" << std::endl;
            written_ = true;
        }
        last_written_ = bytes;
        glfwSetClipboardString(window_, last_written_.c_str());
    }

    /// @brief Compares, then restores (idempotent). See Outcome.
    Outcome restore() {
        if (done_) {
            return outcome_;
        }
        done_ = true;
        if (!written_) {
            outcome_ = Outcome::NothingWritten;
            return outcome_;
        }
        const char* const current = glfwGetClipboardString(window_);
        if (current == nullptr || !same_bytes(last_written_, current)) {
            std::cout << "NOTICE: " << case_name_
                      << " did not restore the clipboard text: it changed during the run" << std::endl;
            outcome_ = Outcome::ChangedByOthersNotRestored;
            return outcome_;
        }
        glfwSetClipboardString(window_, saved_.c_str());
        const char* const after = glfwGetClipboardString(window_);
        const std::string_view after_view = after != nullptr ? std::string_view(after) : std::string_view();
        if (!same_bytes(saved_, after_view)) {
            std::cout << "NOTICE: " << case_name_
                      << " restored the clipboard text, but it did not read back identical" << std::endl;
            outcome_ = Outcome::RestoreNotVerified;
            return outcome_;
        }
        std::cout << "NOTICE: " << case_name_ << " restored the clipboard text" << std::endl;
        outcome_ = Outcome::Restored;
        return outcome_;
    }

private:
    GLFWwindow* window_;
    std::string_view case_name_;
    std::string saved_;
    std::string last_written_;
    bool written_ = false;
    bool done_ = false;
    Outcome outcome_ = Outcome::NothingWritten;
};

/// @return the literal name of a ClipboardRestoreGuard::Outcome.
std::string_view outcome_name(ClipboardRestoreGuard::Outcome outcome) {
    switch (outcome) {
    case ClipboardRestoreGuard::Outcome::NothingWritten:
        return "NothingWritten";
    case ClipboardRestoreGuard::Outcome::Restored:
        return "Restored";
    case ClipboardRestoreGuard::Outcome::ChangedByOthersNotRestored:
        return "ChangedByOthersNotRestored";
    case ClipboardRestoreGuard::Outcome::RestoreNotVerified:
        return "RestoreNotVerified";
    }
    return "Unknown";
}

} // namespace

BOOST_AUTO_TEST_SUITE(SmbiosGuiLocalTests)

/// Normal case, in memory only (no clipboard): for every registered node and
/// every GUI format, render_entry_as() equals the registered formatter's own
/// render() byte for byte, and those bytes occur verbatim, in node order, in
/// `smbios --format <name>`'s stdout (the CLI adds document framing, so
/// containment, not equality). Also pins the GUI's format list to
/// json/text/xml/yaml.
BOOST_AUTO_TEST_CASE(CopyAsBytesMatchCliEveryNodeAndFormat,
    *utf::precondition([](utf::test_unit_id) { return skip_if_root(kInMemoryCase); }))
{
    LiveGuiModel model = read_live_gui_model();
    const ScopedTempDirectory working_directory;
    std::vector<SpawnResult> cli_runs;
    if (working_directory.valid()) {
        for (const GuiCopyFormat& format : kGuiCopyFormats) {
            cli_runs.push_back(run_cli(format, working_directory.path()));
        }
    }
    // Skip only when both sides see no structures, decided before any
    // assertion (threat model T-9).
    const bool json_ran = !cli_runs.empty() && cli_runs[0].status == SpawnResult::Status::Exited
                       && cli_runs[0].exit_code == 0;
    const auto json_elements = json_ran ? split_json_array(cli_runs[0].stdout_bytes) : std::nullopt;
    if (!model.read_failed && model.nodes.empty() && json_elements && json_elements->empty()) {
        static_cast<void>(skip_unless(false, kInMemoryCase, "no readable SMBIOS table"));
        return;
    }

    BOOST_REQUIRE_MESSAGE(working_directory.valid(), "temporary working directory: created -> not created");
    BOOST_REQUIRE_MESSAGE(!model.read_failed, "GUI model table read: ok -> threw");
    DiscrepancyReport report;
    BOOST_REQUIRE_MESSAGE(registry_matches_gui_formats(report), "GUI format list: json,text,xml,yaml -> differs");

    const std::vector<std::size_t> registered = model.registered_indices();
    std::size_t checked = 0;
    for (std::size_t f = 0; f < kGuiCopyFormats.size(); ++f) {
        const GuiCopyFormat& format = kGuiCopyFormats[f];
        const SpawnResult& cli = cli_runs[f];
        if (cli.status != SpawnResult::Status::Exited || cli.exit_code != 0) {
            report.add(std::string(format.name) + " CLI status: Exited/0 -> " + std::string(status_name(cli.status))
                       + "/" + std::to_string(cli.exit_code));
            continue;
        }
        const smbios::IOutputFormatter* const formatter = smbios::get_output_formatter(std::string(format.name));
        if (formatter == nullptr) {
            report.add(std::string(format.name) + " formatter: registered -> missing");
            continue;
        }
        std::size_t search_from = 0;
        for (std::size_t position = 0; position < registered.size(); ++position) {
            const std::size_t node_index = registered[position];
            const std::string where = node_where(format.name, position, node_index, model.nodes[node_index]);
            if (model.kinds[node_index] == NodeKind::DecodeFailed) {
                report.add(where + " decode: ok -> failed");
                continue;
            }
            const auto entry = entry_for_node(model, node_index);
            if (!entry) {
                report.add(where + " entry: created -> not created");
                continue;
            }
            const std::optional<std::string> copied = smbios::gui::render_entry_as(*entry, format.name);
            if (!copied) {
                report.add(where + " render_entry_as: rendered -> refused");
                continue;
            }
            const std::string reference = formatter->render(*entry);
            if (!same_bytes(reference, *copied)) {
                report.add(where + " bytes vs formatter render(): " + std::to_string(reference.size()) + " -> "
                           + std::to_string(copied->size()));
            }
            const auto found = find_bytes(cli.stdout_bytes, *copied, search_from);
            if (!found) {
                report.add(where + " in CLI output (in order): found -> not found");
                continue;
            }
            search_from = *found + copied->size();
            ++checked;
        }
    }
    report.finish();
    BOOST_TEST((report.count() == 0), "copy-as discrepancies: 0 -> " << report.count());
    BOOST_TEST((checked == registered.size() * kGuiCopyFormats.size()),
               "node x format checks completed: " << registered.size() * kGuiCopyFormats.size() << " -> "
                                                  << checked);
}

/// Normal case through the real OS clipboard: one identifier-free entry,
/// rendered in each of the 4 GUI formats, is written with
/// glfwSetClipboardString() and read back with glfwGetClipboardString();
/// the bytes read back equal render_entry_as() exactly and occur in the
/// CLI's output for that format. The user's clipboard text is restored
/// afterwards (compare-before-restore guard, verified by reading back).
BOOST_AUTO_TEST_CASE(ClipboardRoundTripMatchesCli,
    *utf::precondition([](utf::test_unit_id) {
        auto not_root = skip_if_root(kClipboardCase);
        if (!not_root) {
            return not_root;
        }
        return skip_unless(display_and_gl_available(), kClipboardCase,
                           "no display or OpenGL context (hidden GLFW window creation failed)");
    }))
{
    LiveGuiModel model = read_live_gui_model();
    if (model.read_failed || model.nodes.empty()) {
        static_cast<void>(skip_unless(false, kClipboardCase, "no readable SMBIOS table"));
        return;
    }
    const std::optional<std::size_t> chosen = find_identifier_free_node(model);
    if (!chosen) {
        static_cast<void>(skip_unless(false, kClipboardCase,
                                      "no registered entry without identifier fields; OS clipboard not used"));
        return;
    }

    const smbios::gui::EntryNode& node = model.nodes[*chosen];
    const auto entry = entry_for_node(model, *chosen);
    BOOST_REQUIRE_MESSAGE(static_cast<bool>(entry), "chosen node #" << *chosen << " (type " << node.type
                                                                    << ") entry: created -> not created");

    // Render and check against the CLI in memory first, before any
    // clipboard write.
    const ScopedTempDirectory working_directory;
    BOOST_REQUIRE_MESSAGE(working_directory.valid(), "temporary working directory: created -> not created");
    std::vector<std::string> rendered;
    for (const GuiCopyFormat& format : kGuiCopyFormats) {
        std::optional<std::string> bytes = smbios::gui::render_entry_as(*entry, format.name);
        BOOST_REQUIRE_MESSAGE(bytes.has_value(), format.name << " render_entry_as: rendered -> refused");
        const SpawnResult cli = run_cli(format, working_directory.path());
        BOOST_TEST((cli.status == SpawnResult::Status::Exited && cli.exit_code == 0),
                   format.name << " CLI status: Exited/0 -> " << status_name(cli.status) << "/" << cli.exit_code);
        BOOST_TEST(find_bytes(cli.stdout_bytes, *bytes, 0).has_value(),
                   format.name << " node #" << *chosen << " (type " << node.type
                               << ") in CLI output: found -> not found");
        rendered.push_back(std::move(*bytes));
    }

    const GlfwTestWindow window(GlfwTestWindow::Layers::WindowOnly);
    BOOST_REQUIRE_MESSAGE(window.ready(), "hidden window: Ready -> " << status_name(window.status()));
    {
        ClipboardRestoreGuard guard(window.window(), kClipboardCase);
        for (std::size_t f = 0; f < kGuiCopyFormats.size(); ++f) {
            guard.write(rendered[f]);
            const char* const read_back = glfwGetClipboardString(window.window());
            const std::string_view read_view = read_back != nullptr ? std::string_view(read_back) : std::string_view();
            BOOST_TEST((read_back != nullptr && same_bytes(rendered[f], read_view)),
                       kGuiCopyFormats[f].name << " node #" << *chosen << " (type " << node.type
                                               << ") clipboard round-trip bytes: " << rendered[f].size() << " -> "
                                               << read_view.size()
                                               << (read_back == nullptr ? " (no text)" : ""));
        }
        const ClipboardRestoreGuard::Outcome outcome = guard.restore();
        BOOST_TEST((outcome == ClipboardRestoreGuard::Outcome::Restored),
                   "clipboard restore: Restored -> " << outcome_name(outcome));
    }
}

BOOST_AUTO_TEST_SUITE_END()
