#include "support/local_only_guard.hpp"

/// @file live_parity_test.cpp
/// @brief Local-only live parity between the GUI model layer and the
/// `smbios` CLI (milestone 0011, Story 04.0 Task 04; manual-testing.md
/// M-02/M-03).
///
/// - Type: integration (local-only tier, labels `gui;gui-local`). Needs the
///   live SMBIOS table and the `smbios` CLI from the same build; no display.
/// - Scenario: this process reads the live table exactly as `smbios_gui`
///   does (SMBios + build_entry_tree()), and `smbios --format json` is run
///   through spawn_and_capture() (no shell). The CLI's JSON array is split
///   in memory. The GUI's *registered* nodes (the ruled parity predicate:
///   SMBiosEntryFactory::create() gives an entry, status.md 2026-11-11
///   "Parity gate ruling") must match the CLI's entries in count, and per
///   position in structure type and in the exact bytes of the entry's JSON
///   object (render_entry_as(..., "json") vs the CLI element, whitespace
///   around each element ignored). The handle order is covered because
///   create_entry_for_node() re-checks each node's type and handle at its
///   structure position, and a node whose entry differs from the CLI's at
///   that position differs in bytes. Unregistered structures are GUI-only
///   `Unknown (0xNN)` nodes by design; their count is printed once as
///   `GUI-only unregistered nodes: <count>`.
/// - Why JSON, not `--format text`'s `Header type:` lines: the text format
///   does not escape firmware strings, so a string could forge such a line
///   (threat model T-9).
/// - Output: nothing else on success. On failure one line per difference,
///   `<what>: expected -> actual`, with the CLI as *expected* and the GUI as
///   *actual*, carrying counts, node indices, type codes and handles only,
///   at most DiscrepancyReport::kMaxLines lines plus one summary line. No entry
///   text, field value or CLI output is ever printed (T-3, R-20).
/// - Skips: as root (R-19), and only when **both** the GUI model and the
///   CLI see 0 structures (`no readable SMBIOS table`; T-9 - any other
///   disagreement fails). The skip decision is made before any assertion,
///   so the `SKIP:` line can never mask a failure.
/// - On failure, first check: that `smbios` was rebuilt with this target (a
///   stale CLI binary), then whether a new structure type was registered in
///   only one of the two binaries' link lines.

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <smbios/gui/copy_as.hpp>

#include "support/live_table_support.hpp"
#include "support/local_skip.hpp"
#include "support/process_spawn.hpp"
#include "support/scoped_temp_directory.hpp"

namespace utf = boost::unit_test;
using smbios::test::gui::DiscrepancyReport;
using smbios::test::gui::hex_handle;
using smbios::test::gui::json_entry_type_name;
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
using smbios::test::gui::trim_ascii_whitespace;
using smbios::test::gui::type_code_for_name;

namespace {

/// Case name, used in the skip line.
constexpr const char* kCaseName = "LiveParityRegisteredNodesMatchCli";

static_assert(kGuiCopyFormats[0].name == "json", "the parity case runs the CLI with kGuiCopyFormats[0]");

/// @return the type code for a CLI element's type name, or `?`.
std::string cli_type_code(const std::optional<std::string>& name) {
    if (!name) {
        return "?";
    }
    const auto code = type_code_for_name(*name);
    return code ? std::to_string(*code) : std::string("?");
}

} // namespace

BOOST_AUTO_TEST_SUITE(SmbiosGuiLocalTests)

/// Normal case on any machine with a readable table: the GUI's registered
/// nodes equal the CLI's JSON entries in count, type order and bytes.
BOOST_AUTO_TEST_CASE(LiveParityRegisteredNodesMatchCli,
    *utf::precondition([](utf::test_unit_id) { return skip_if_root(kCaseName); }))
{
    // Gather both sides first, assert nothing yet: the skip decision below
    // must come before any assertion (a SKIP: line after a failed
    // assertion would turn the failure into a CTest "Skipped").
    LiveGuiModel model = read_live_gui_model();
    const ScopedTempDirectory working_directory;
    const bool have_directory = working_directory.valid();
    const SpawnResult cli = have_directory ? run_cli(kGuiCopyFormats[0], working_directory.path()) : SpawnResult{};
    const bool cli_ran_cleanly = cli.status == SpawnResult::Status::Exited && cli.exit_code == 0;
    const auto cli_elements = cli_ran_cleanly ? split_json_array(cli.stdout_bytes) : std::nullopt;

    if (!model.read_failed && model.nodes.empty() && cli_elements && cli_elements->empty()) {
        static_cast<void>(skip_unless(false, kCaseName, "no readable SMBIOS table"));
        return;
    }

    BOOST_REQUIRE_MESSAGE(have_directory, "temporary working directory: created -> not created");
    BOOST_REQUIRE_MESSAGE(!model.read_failed, "GUI model table read: ok -> threw");
    BOOST_REQUIRE_MESSAGE((cli.status == SpawnResult::Status::Exited),
                          "CLI status: Exited -> " << status_name(cli.status));
    BOOST_REQUIRE_MESSAGE((cli.exit_code == 0), "CLI exit status: 0 -> " << cli.exit_code);
    BOOST_REQUIRE_MESSAGE(cli_elements.has_value(), "CLI json document: one top-level array -> malformed ("
                                                        << cli.stdout_bytes.size() << " bytes)");

    const std::vector<std::size_t> registered = model.registered_indices();
    DiscrepancyReport report;
    if (registered.size() != cli_elements->size()) {
        report.add("registered node count: " + std::to_string(cli_elements->size()) + " -> "
                   + std::to_string(registered.size()));
    }

    const std::size_t common = std::min(registered.size(), cli_elements->size());
    for (std::size_t position = 0; position < common; ++position) {
        const std::size_t node_index = registered[position];
        const smbios::gui::EntryNode& node = model.nodes[node_index];
        const std::string where = "registered node #" + std::to_string(position) + " (node #"
                                + std::to_string(node_index) + ", type " + std::to_string(node.type)
                                + ", handle " + hex_handle(node.handle) + ")";
        const std::string_view cli_element = (*cli_elements)[position];
        const std::optional<std::string> cli_name = json_entry_type_name(cli_element);

        if (model.kinds[node_index] == NodeKind::DecodeFailed) {
            report.add(where + " decode: ok -> failed");
            continue;
        }
        const auto entry = smbios::test::gui::entry_for_node(model, node_index);
        if (!entry) {
            report.add(where + " entry: created -> not created");
            continue;
        }
        if (!cli_name || !same_bytes(*cli_name, entry->get_type())) {
            report.add("registered node #" + std::to_string(position) + " type: " + cli_type_code(cli_name)
                       + " -> " + std::to_string(node.type));
            continue;
        }
        const std::optional<std::string> gui_json = smbios::gui::render_entry_as(*entry, "json");
        if (!gui_json) {
            report.add(where + " json render: rendered -> refused");
            continue;
        }
        const std::string_view gui_element = trim_ascii_whitespace(*gui_json);
        if (!same_bytes(cli_element, gui_element)) {
            report.add(where + " json object bytes: " + std::to_string(cli_element.size()) + " -> "
                       + std::to_string(gui_element.size())
                       + (cli_element.size() == gui_element.size() ? " (content differs)" : ""));
        }
    }
    for (std::size_t position = common; position < cli_elements->size(); ++position) {
        report.add("registered node #" + std::to_string(position)
                   + " type: " + cli_type_code(json_entry_type_name((*cli_elements)[position])) + " -> (missing)");
    }
    for (std::size_t position = common; position < registered.size(); ++position) {
        report.add("registered node #" + std::to_string(position)
                   + " type: (missing) -> " + std::to_string(model.nodes[registered[position]].type));
    }
    report.finish();
    BOOST_TEST((report.count() == 0), "parity discrepancies: 0 -> " << report.count());

    // The one informational line on success or failure (R-20): a count.
    std::cout << "GUI-only unregistered nodes: " << model.unregistered_count() << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
