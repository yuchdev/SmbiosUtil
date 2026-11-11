#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/output_formatter.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/gui/copy_as.hpp>
#include <smbios/gui/view_model.hpp>

#include "field_assertions.hpp"
#include "gui_test_support.hpp"
#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using smbios::gui::EntryNode;

// Unit tests (milestone 0011, Story 02.0 Task 04)
//
// Type: Unit - deterministic, no hardware/privileges/display/clipboard; no
// Dear ImGui or GLFW linked.
// Scenario: the ImGui-free logic behind "Copy as..." (copy_as.hpp) -
// render_entry_as(), default_copy_format_index(), is_terminal_safe_format()
// and create_entry_for_node() - against synthetic tables. Threat-model
// requirement 18 (docs/security/2026-11-11-gui-browsing-ui.md): every
// registered format renders a synthetic entry to non-empty output that is
// byte-for-byte formatter->render(); an unknown name and an over-1-MiB
// rendering both yield std::nullopt. Plus the exact 1 MiB boundary, and the
// G-7 index/type/handle cross-check behind create_entry_for_node(),
// including duplicate handles (G-2).
// Boundaries: the set of formats is read from
// registered_output_formatter_names() - never hardcoded beyond "contains
// the four built-ins" (smbios_gui registers json/text/xml/yaml only; this
// binary also compiles in the CLI's csv example and other TUs' test-local
// fake formatters, so those are in the list here and covered by the same
// loops). Synthetic strings only (requirement 20).
// On failure first check: smbios_gui/src/copy_as.cpp; for "format not
// registered", that tests/unit/CMakeLists.txt still whole-archive-links
// libsmbios.

namespace {

/// @brief Loads an OEM Strings table with `strings` into `bios` and returns
/// its view-model nodes.
std::vector<EntryNode> load_oem_strings(SMBios& bios, const std::vector<std::string>& strings)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {static_cast<uint8_t>(strings.size())}, strings);
    smbios::test::gui::load_table(bios, builder.build());
    return smbios::gui::build_entry_tree(bios);
}

/// @brief Asserts `names` contains each of the four libsmbios built-ins.
void require_built_in_formats(const std::vector<std::string>& names)
{
    for (const char* built_in : {"json", "text", "xml", "yaml"}) {
        BOOST_REQUIRE_MESSAGE(std::find(names.begin(), names.end(), built_in) != names.end(),
                              "format \"" << built_in << "\" is not registered");
    }
}

/// @brief Asserts render_entry_as(entry, name) equals the formatter's own
/// render(), byte for byte, and is non-empty.
void check_render_matches_formatter(const AbstractSMBiosEntry& entry, const std::string& name)
{
    const IOutputFormatter* formatter = get_output_formatter(name);
    BOOST_REQUIRE(formatter != nullptr);
    const std::string expected = formatter->render(entry);
    const std::optional<std::string> rendered = smbios::gui::render_entry_as(entry, name);
    BOOST_REQUIRE_MESSAGE(rendered.has_value(), "format \"" << name << "\" returned nullopt");
    BOOST_CHECK(!rendered->empty());
    BOOST_CHECK_MESSAGE(*rendered == expected, "format \"" << name << "\" output differs from render()");
}

} // namespace

BOOST_AUTO_TEST_SUITE(GuiCopyAsUnitTests)

//////////////////////////////////////////////////////////////////////////
// render_entry_as()

/// Normal case: every registered format renders a synthetic BIOS
/// Information and an OEM Strings entry to non-empty output equal to
/// formatter->render().
BOOST_AUTO_TEST_CASE(RenderEntryAsEveryRegisteredFormatTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BIOSInformation, std::vector<uint8_t>(0x60, 0x02), {"Synthetic Vendor", "SYN-BIOS-0001"})
        .add_structure(SMBios::OemStrings, {2}, {"synthetic oem one", "synthetic oem two"});
    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(2));

    const std::vector<std::string> names = registered_output_formatter_names();
    require_built_in_formats(names);
    for (size_t index = 0; index < nodes.size(); ++index) {
        const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, index, nodes[index]);
        BOOST_REQUIRE(entry != nullptr);
        for (const std::string& name : names) {
            BOOST_TEST_CONTEXT("node " << index << ", format " << name)
            {
                check_render_matches_formatter(*entry, name);
            }
        }
    }
}

/// Security case (G-5): hostile strings reach every format unmodified by
/// any GUI-side layer - output is still exactly formatter->render().
BOOST_AUTO_TEST_CASE(RenderEntryAsHostileStringsByteExactTestCase)
{
    SMBios bios;
    const std::vector<EntryNode> nodes =
        load_oem_strings(bios, {"\x1B[2J", "line\nbreak", "\xC0\xAF", "\xE2\x80\xAE" "rtl", "%n%s", "###id"});
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(1));
    const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, 0, nodes[0]);
    BOOST_REQUIRE(entry != nullptr);
    for (const std::string& name : registered_output_formatter_names()) {
        BOOST_TEST_CONTEXT("format " << name)
        {
            check_render_matches_formatter(*entry, name);
        }
    }
}

/// Error case: names that are not registered - including the empty name,
/// a case variant and a whitespace variant of "json" - yield nullopt.
BOOST_AUTO_TEST_CASE(RenderEntryAsUnknownNameReturnsNulloptTestCase)
{
    SMBios bios;
    const std::vector<EntryNode> nodes = load_oem_strings(bios, {"synthetic"});
    const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, 0, nodes.at(0));
    BOOST_REQUIRE(entry != nullptr);
    for (const char* unknown : {"no-such-format", "", "JSON", "json ", " json"}) {
        BOOST_REQUIRE(get_output_formatter(unknown) == nullptr);
        BOOST_CHECK_MESSAGE(!smbios::gui::render_entry_as(*entry, unknown).has_value(),
                            "\"" << unknown << "\" did not return nullopt");
    }
}

/// Error case (G-5): an entry whose rendering exceeds 1 MiB in every
/// format (two 600 KiB strings) yields nullopt - refused, never cut.
BOOST_AUTO_TEST_CASE(RenderEntryAsOversizedReturnsNulloptTestCase)
{
    const std::string chunk(600 * 1024, 'A');
    SMBios bios;
    const std::vector<EntryNode> nodes = load_oem_strings(bios, {chunk, chunk});
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(1));
    const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, 0, nodes[0]);
    BOOST_REQUIRE(entry != nullptr);

    const std::vector<std::string> names = registered_output_formatter_names();
    require_built_in_formats(names);
    for (const std::string& name : names) {
        BOOST_TEST_CONTEXT("format " << name)
        {
            const IOutputFormatter* formatter = get_output_formatter(name);
            BOOST_REQUIRE(formatter != nullptr);
            const bool over_cap = formatter->render(*entry).size() > smbios::gui::kCopyAsMaxBytes;
            // Other TUs register fixed-output fake formatters; the built-ins
            // must all really exceed the cap for this case to mean anything.
            const bool built_in = name == "json" || name == "text" || name == "xml" || name == "yaml";
            if (built_in) {
                BOOST_REQUIRE(over_cap);
            }
            if (over_cap) {
                BOOST_CHECK(!smbios::gui::render_entry_as(*entry, name).has_value());
            } else {
                check_render_matches_formatter(*entry, name);
            }
        }
    }
}

/// Boundary case: json output of exactly kCopyAsMaxBytes is returned whole;
/// one byte more is refused. The string length is derived from a probe
/// render ('A' is never escaped, so output grows one byte per character).
BOOST_AUTO_TEST_CASE(RenderEntryAsExactCapBoundaryTestCase)
{
    const IOutputFormatter* json = get_output_formatter("json");
    BOOST_REQUIRE(json != nullptr);

    const auto json_size_for = [json](size_t length) {
        SMBios bios;
        const std::vector<EntryNode> nodes = load_oem_strings(bios, {std::string(length, 'A')});
        const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, 0, nodes.at(0));
        BOOST_REQUIRE(entry != nullptr);
        return json->render(*entry).size();
    };
    const size_t probe_length = 1000;
    const size_t overhead = json_size_for(probe_length) - probe_length;
    const size_t at_cap_length = smbios::gui::kCopyAsMaxBytes - overhead;

    {
        SMBios bios;
        const std::vector<EntryNode> nodes = load_oem_strings(bios, {std::string(at_cap_length, 'A')});
        const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, 0, nodes.at(0));
        BOOST_REQUIRE(entry != nullptr);
        BOOST_REQUIRE_EQUAL(json->render(*entry).size(), smbios::gui::kCopyAsMaxBytes);
        const std::optional<std::string> rendered = smbios::gui::render_entry_as(*entry, "json");
        BOOST_REQUIRE(rendered.has_value());
        BOOST_CHECK_EQUAL(rendered->size(), smbios::gui::kCopyAsMaxBytes);
    }
    {
        SMBios bios;
        const std::vector<EntryNode> nodes = load_oem_strings(bios, {std::string(at_cap_length + 1, 'A')});
        const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, 0, nodes.at(0));
        BOOST_REQUIRE(entry != nullptr);
        BOOST_REQUIRE_EQUAL(json->render(*entry).size(), smbios::gui::kCopyAsMaxBytes + 1);
        BOOST_CHECK(!smbios::gui::render_entry_as(*entry, "json").has_value());
    }
}

//////////////////////////////////////////////////////////////////////////
// default_copy_format_index() / is_terminal_safe_format()

/// Normal + boundary case: the index of "json" wherever it is; 0 when it is
/// absent or the list is empty; against the live registry, it selects json.
BOOST_AUTO_TEST_CASE(DefaultCopyFormatIndexTestCase)
{
    BOOST_CHECK_EQUAL(smbios::gui::default_copy_format_index({"json", "text"}), static_cast<size_t>(0));
    BOOST_CHECK_EQUAL(smbios::gui::default_copy_format_index({"text", "xml", "json", "yaml"}), static_cast<size_t>(2));
    BOOST_CHECK_EQUAL(smbios::gui::default_copy_format_index({"text", "xml"}), static_cast<size_t>(0));
    BOOST_CHECK_EQUAL(smbios::gui::default_copy_format_index({}), static_cast<size_t>(0));
    BOOST_CHECK_EQUAL(smbios::gui::default_copy_format_index({"JSON", "json"}), static_cast<size_t>(1));

    const std::vector<std::string> names = registered_output_formatter_names();
    require_built_in_formats(names);
    const size_t index = smbios::gui::default_copy_format_index(names);
    BOOST_REQUIRE_LT(index, names.size());
    BOOST_CHECK_EQUAL(names[index], "json");
}

/// Security case (G-5, fail-safe): only "json" is terminal-safe; every
/// other registered format, unknown names and near-misses are not.
BOOST_AUTO_TEST_CASE(IsTerminalSafeFormatTestCase)
{
    BOOST_CHECK(smbios::gui::is_terminal_safe_format("json"));
    for (const char* unsafe : {"text", "xml", "yaml", "csv", "", "JSON", "json ", "future-format"}) {
        BOOST_CHECK_MESSAGE(!smbios::gui::is_terminal_safe_format(unsafe), "\"" << unsafe << "\" reported safe");
    }
    for (const std::string& name : registered_output_formatter_names()) {
        BOOST_CHECK_EQUAL(smbios::gui::is_terminal_safe_format(name), name == "json");
    }
}

//////////////////////////////////////////////////////////////////////////
// create_entry_for_node()

/// Normal case: for each node, the re-created entry is the same structure -
/// its get_fields() equals the node's fields.
BOOST_AUTO_TEST_CASE(CreateEntryForNodeMatchesNodeTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {1}, {"first"})
        .add_structure(SMBios::MemoryDevice, std::vector<uint8_t>(0x60, 0x02), {"DIMM-A", "SYN-MEM-0001"})
        .add_structure(SMBios::OemStrings, {1}, {"third"});
    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(3));

    for (size_t index = 0; index < nodes.size(); ++index) {
        BOOST_TEST_CONTEXT("node " << index)
        {
            const std::unique_ptr<AbstractSMBiosEntry> entry = smbios::gui::create_entry_for_node(bios, index, nodes[index]);
            BOOST_REQUIRE(entry != nullptr);
            std::vector<NamedField> node_fields;
            for (const auto& row : nodes[index].fields) {
                node_fields.push_back(NamedField{row.name, row.value});
            }
            smbios::test::check_fields_equal(node_fields, entry->get_fields());
        }
    }
}

/// Error case (G-7 guard): an index past the end, a node whose handle or
/// type does not match the structure at that index, and an unregistered
/// type all yield nullptr.
BOOST_AUTO_TEST_CASE(CreateEntryForNodeRejectsMismatchTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {1}, {"first"}).add_structure(0xC8, {0xAA});
    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(2));

    BOOST_CHECK(smbios::gui::create_entry_for_node(bios, 2, nodes[0]) == nullptr);
    BOOST_CHECK(smbios::gui::create_entry_for_node(bios, static_cast<size_t>(-1), nodes[0]) == nullptr);
    // Node 1 (type 0xC8, handle 2) asked for at index 0 (type 11, handle 1).
    BOOST_CHECK(smbios::gui::create_entry_for_node(bios, 0, nodes[1]) == nullptr);

    EntryNode wrong_handle = nodes[0];
    wrong_handle.handle = static_cast<uint16_t>(nodes[0].handle + 1);
    BOOST_CHECK(smbios::gui::create_entry_for_node(bios, 0, wrong_handle) == nullptr);

    EntryNode wrong_type = nodes[0];
    wrong_type.type = SMBios::MemoryDevice;
    BOOST_CHECK(smbios::gui::create_entry_for_node(bios, 0, wrong_type) == nullptr);

    // Matches, but type 0xC8 has no registered entry class.
    BOOST_CHECK(smbios::gui::create_entry_for_node(bios, 1, nodes[1]) == nullptr);
}

/// Security case (G-2): with two structures sharing type and handle, the
/// index selects which one is re-created - the second node gets the second
/// structure, not the first match on handle.
BOOST_AUTO_TEST_CASE(CreateEntryForNodeDuplicateHandleUsesIndexTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_raw_bytes(smbios::test::gui::oem_strings_structure(0x0042, {"duplicate first"}));
    builder.add_raw_bytes(smbios::test::gui::oem_strings_structure(0x0042, {"duplicate second"}));
    builder.add_raw_bytes(smbios::test::gui::end_of_table_structure(0x0043));
    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(2));

    const std::unique_ptr<AbstractSMBiosEntry> second = smbios::gui::create_entry_for_node(bios, 1, nodes[1]);
    BOOST_REQUIRE(second != nullptr);
    const std::vector<NamedField> fields = second->get_fields();
    BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(2));
    BOOST_CHECK(fields[1].value == smbios::test::str("duplicate second"));
}

BOOST_AUTO_TEST_SUITE_END()
