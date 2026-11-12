#pragma once

#include "support/local_only_guard.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/gui/view_model.hpp>

#include <boost/test/unit_test.hpp>

#include "support/process_spawn.hpp"

/// @file live_table_support.hpp
/// @brief Live-table helpers shared by the local-only GUI cases that compare
/// the GUI model layer with the `smbios` CLI (milestone 0011, Story 04.0
/// Task 04; threat model `docs/security/2026-11-11-gui-test-tiers.md`,
/// T-3/T-9, R-18/R-20). Local-only: guarded by local_only_guard.hpp.
///
/// Privacy rule for everything here and for every caller: live table data,
/// rendered entries, CLI stdout and clipboard bytes stay in memory. They are
/// compared only through same_bytes() / find_bytes() (which return a bool or
/// a position and never print their operands) and are never printed,
/// logged or written to a file. Failure messages carry counts, node
/// indices, structure type codes and handles only.

namespace smbios::test::gui {

/// @brief The tier's one byte-comparison helper (threat model T-3/R-20).
/// @param expected the reference bytes.
/// @param actual the bytes under test.
/// @return true iff both have the same size and the same bytes. Never
///         prints either operand.
[[nodiscard]] bool same_bytes(std::string_view expected, std::string_view actual) noexcept;

/// @brief Finds `needle` in `haystack` at or after `from` (containment
/// checks against CLI output). Never prints either operand.
/// @return the position of the first occurrence, or std::nullopt.
[[nodiscard]] std::optional<std::size_t> find_bytes(std::string_view haystack, std::string_view needle,
                                                    std::size_t from) noexcept;

/// @brief How the GUI model layer sees one node, by the ruled parity
/// predicate (status.md 2026-11-11 "Parity gate ruling"): a node is
/// *registered* iff `SMBiosEntryFactory::create()` gives an entry for it -
/// the same predicate the CLI's `render_document()` uses to skip.
enum class NodeKind {
    /// create_entry_for_node() returned an entry.
    Registered,
    /// No registered entry class: a GUI-only `Unknown (0xNN)` node.
    Unregistered,
    /// A registered entry class threw while creating the entry or its
    /// fields (EntryNode::decode_failed, or create_entry_for_node() threw).
    DecodeFailed,
};

/// @brief The live table as the GUI sees it: the SMBios read in this
/// process plus build_entry_tree()'s nodes and each node's NodeKind.
///
/// `bios` is declared first so it outlives every entry created from it.
struct LiveGuiModel {
    /// The table read in this test process (same source chain as the GUI).
    std::unique_ptr<smbios::SMBios> bios;
    /// build_entry_tree(*bios); empty if no table could be read.
    std::vector<smbios::gui::EntryNode> nodes;
    /// kinds[i] classifies nodes[i].
    std::vector<NodeKind> kinds;
    /// True if constructing SMBios or build_entry_tree() threw.
    bool read_failed = false;

    /// @return the indices into `nodes` whose kind is not Unregistered, in
    ///         node order (DecodeFailed nodes count as registered).
    [[nodiscard]] std::vector<std::size_t> registered_indices() const;
    /// @return how many nodes are Unregistered.
    [[nodiscard]] std::size_t unregistered_count() const;
};

/// @brief Reads the live table exactly as `smbios_gui/src/main.cpp` does
/// (SMBios + build_entry_tree()) and classifies every node.
/// @return the model; `read_failed` set (and no nodes) if reading threw.
[[nodiscard]] LiveGuiModel read_live_gui_model();

/// @brief Recreates the entry behind registered node `node_index`.
/// @return the entry, or nullptr if creation returned null or threw.
[[nodiscard]] std::unique_ptr<smbios::AbstractSMBiosEntry> entry_for_node(LiveGuiModel& model,
                                                                          std::size_t node_index);

/// @brief One "Copy as..." format of the GUI and the matching literal CLI
/// argument (spawn_and_capture() accepts compile-time literals only).
struct GuiCopyFormat {
    /// Registry key, e.g. `json`.
    std::string_view name;
    /// The same text as a LiteralArg for `smbios --format <name>`.
    LiteralArg cli_argument;
};

/// The formats the GUI's "Copy as..." dropdown offers (every formatter
/// registered in a binary that links libsmbios whole-archive: `csv` is
/// CLI-only), sorted as registered_output_formatter_names() returns them.
/// A case asserts the registry still equals this list, so a new formatter
/// fails loudly instead of going untested.
inline constexpr std::array<GuiCopyFormat, 4> kGuiCopyFormats{{
    {"json", "json"},
    {"text", "text"},
    {"xml", "xml"},
    {"yaml", "yaml"},
}};

/// @brief Runs `smbios --format <format>` (absolute SMBIOS_CLI_PATH, no
/// shell) in `working_directory` and returns its outcome in memory.
[[nodiscard]] SpawnResult run_cli(const GuiCopyFormat& format, const std::filesystem::path& working_directory);

/// @brief Splits one JSON document whose top level is an array into its
/// elements, without decoding them (string-aware bracket matching).
/// @param document the CLI's `--format json` stdout.
/// @return each element's bytes with surrounding whitespace removed; an
///         empty vector for `[]`; std::nullopt if the document is not one
///         well-formed top-level array (trailing bytes, unbalanced
///         brackets, an empty element, an unterminated string).
[[nodiscard]] std::optional<std::vector<std::string_view>> split_json_array(std::string_view document);

/// @brief The value of the leading `"type": "<name>"` member of one
/// JsonFormatter object (as rendered by JsonFormatter::render()).
/// @return the decoded name, or std::nullopt if the element does not start
///         with that member.
[[nodiscard]] std::optional<std::string> json_entry_type_name(std::string_view element);

/// @brief Maps an entry class's get_type() text back to its SMBIOS
/// structure type code through entry_type_label() (whose named labels
/// match get_type()), so failure messages carry codes, not text.
/// @return the type code, or std::nullopt if no named type has that label.
[[nodiscard]] std::optional<std::size_t> type_code_for_name(std::string_view name);

/// @return `s` without leading and trailing ASCII whitespace.
[[nodiscard]] std::string_view trim_ascii_whitespace(std::string_view s) noexcept;

/// @brief Picks the single entry whose rendering may go through the real
/// OS clipboard (threat model T-2/R-18): the first registered node of type
/// 0 (BIOS Information); failing that, the first registered node whose
/// type is none of 1, 2, 3, 4, 11, 17. In both cases the node must have
/// fields and none of its field *names* may contain `serial`, `uuid` or
/// `asset` (case-insensitive). Only names are inspected; values are never
/// read or printed.
/// @return the node index, or std::nullopt if no node qualifies.
[[nodiscard]] std::optional<std::size_t> find_identifier_free_node(const LiveGuiModel& model);

/// @return `0xHHHH` (uppercase, four digits) for a structure handle.
[[nodiscard]] std::string hex_handle(std::uint16_t handle);

/// @brief Collects one `<what>: expected -> actual` line per difference
/// (report-only-discrepancies format) and reports each as one Boost.Test
/// error, at most kMaxLines of them; finish() adds one summary line for
/// the rest. Callers build every line from counts, node indices, type
/// codes, handles and literal names only - never live or captured text.
class DiscrepancyReport {
public:
    /// Differences printed before the rest are summarised in one line.
    static constexpr std::size_t kMaxLines = 20;

    /// @brief Adds (and, within the cap, reports) one difference.
    void add(const std::string& line) {
        ++count_;
        if (count_ <= kMaxLines) {
            BOOST_ERROR(line);
        }
    }
    /// @brief Reports how many differences were not printed, if any.
    void finish() const {
        if (count_ > kMaxLines) {
            BOOST_ERROR("further discrepancies not shown: 0 -> " << (count_ - kMaxLines));
        }
    }
    /// @return the number of differences added.
    [[nodiscard]] std::size_t count() const noexcept { return count_; }

private:
    std::size_t count_ = 0;
};

} // namespace smbios::test::gui
