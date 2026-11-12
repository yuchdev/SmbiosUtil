#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <smbios/gui/view_model.hpp>

#include "field_assertions.hpp"
#include "gui_test_support.hpp"
#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;
using smbios::gui::EntryNode;
using smbios::gui::FieldRow;

// Unit tests (milestone 0011, Story 02.0 Task 04)
//
// Type: Unit - deterministic, no hardware/privileges/display needed; no
// Dear ImGui or GLFW is linked (libsmbios_gui_model is ImGui-free).
// Scenario: smbios_gui's view model (contract GC1, view_model.hpp) -
// build_entry_tree() over synthetic tables fed through SMBios's real table
// walk (SmbiosTableWalkTestAccess), and entry_type_label(). Covers the
// task's four named cases plus threat-model requirements 15-17 and 19
// (docs/security/2026-11-11-gui-browsing-ui.md): hostile strings kept raw in
// the view model and made safe by display_text(); malformed tables;
// duplicate handles; the empty table. Also the per-entry exception
// isolation of requirement 1 (G-6), via three throwaway entry classes
// registered under OEM types 0xF3-0xF5.
// Boundaries: every node count is compared with a manual begin()/end() loop
// (std::distance does not compile on SMBios::iterator). The SMBIOS version
// is whatever the live machine reported (or 0.0 when unprivileged), so field
// expectations are always taken from a direct get_fields() call under the
// same version, never hardcoded per type. All fixture strings are synthetic
// (requirement 20).
// On failure first check: smbios_gui/src/view_model.cpp (build_entry_tree,
// entry_type_label, display_text); for "fields empty but type registered",
// that tests/unit/CMakeLists.txt still whole-archive-links libsmbios.

namespace {

/// @brief OEM type whose get_fields() throws std::runtime_error.
constexpr uint8_t kThrowsInGetFieldsType = 0xF3;
/// @brief OEM type whose constructor throws a non-std exception (an int).
constexpr uint8_t kThrowsInConstructorType = 0xF4;
/// @brief OEM type whose get_fields() throws std::bad_alloc.
constexpr uint8_t kThrowsBadAllocType = 0xF5;

/// @brief Throwaway entry whose get_fields() always throws
/// std::runtime_error - stands in for a buggy decoder hit by hostile input.
class ThrowingFieldsTestEntry : public AbstractSMBiosEntry {
public:
    ThrowingFieldsTestEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
        : AbstractSMBiosEntry(header, table_end)
    {
    }
    std::string get_type() const override { return "Throwing Fields Test Entry"; }
    std::string render_to_description() const override { return get_type(); }
    std::vector<NamedField> get_fields() const override
    {
        throw std::runtime_error("synthetic decode failure");
    }
};

/// @brief Throwaway entry whose constructor throws a non-std::exception
/// type, exercising build_entry_tree()'s catch (...).
class ThrowingConstructorTestEntry : public AbstractSMBiosEntry {
public:
    ThrowingConstructorTestEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
        : AbstractSMBiosEntry(header, table_end)
    {
        throw 42;
    }
    std::string get_type() const override { return "Throwing Constructor Test Entry"; }
    std::string render_to_description() const override { return get_type(); }
    std::vector<NamedField> get_fields() const override { return {}; }
};

/// @brief Throwaway entry whose get_fields() throws std::bad_alloc, the one
/// exception build_entry_tree() must propagate.
class BadAllocTestEntry : public AbstractSMBiosEntry {
public:
    BadAllocTestEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
        : AbstractSMBiosEntry(header, table_end)
    {
    }
    std::string get_type() const override { return "Bad Alloc Test Entry"; }
    std::string render_to_description() const override { return get_type(); }
    std::vector<NamedField> get_fields() const override { throw std::bad_alloc(); }
};

/// @brief Formatted-area length used for registered fixtures: total
/// structure length 0x64, long enough for every type's longest layout
/// (same choice as get_fields_coverage_test.cpp).
constexpr size_t kFormattedAreaLength = 0x60;

/// @brief A 0x02-filled formatted area: every string index selects string 2,
/// every enum reads the conventional SMBIOS "Unknown" code.
std::vector<uint8_t> filled_area()
{
    return std::vector<uint8_t>(kFormattedAreaLength, 0x02);
}

/// @brief Converts a node's FieldRows back to NamedFields, for
/// smbios::test::check_fields_equal().
std::vector<NamedField> to_named_fields(const std::vector<FieldRow>& rows)
{
    std::vector<NamedField> fields;
    for (const FieldRow& row : rows) {
        fields.push_back(NamedField{row.name, row.value});
    }
    return fields;
}

/// @brief Asserts `nodes` is exactly one node per iterable structure of
/// `bios`, in iteration order, with matching type and handle.
void check_nodes_match_walk(SMBios& bios, const std::vector<EntryNode>& nodes)
{
    BOOST_REQUIRE_EQUAL(nodes.size(), smbios::test::gui::count_structures(bios));
    size_t index = 0;
    for (const DMIHeader& header : bios) {
        BOOST_CHECK_EQUAL(nodes[index].type, static_cast<size_t>(header.type));
        BOOST_CHECK_EQUAL(nodes[index].handle, header.handle);
        BOOST_CHECK_EQUAL(nodes[index].label, smbios::gui::entry_type_label(header.type));
        ++index;
    }
}

/// @brief Every named SMBios::SMBiosHandler value (smbios.hpp), listed
/// explicitly so the label test also proves this list is complete.
std::vector<size_t> all_named_handler_values()
{
    return {
        SMBios::BIOSInformation, SMBios::SystemInformation, SMBios::BaseboardInformation,
        SMBios::SystemEnclosure, SMBios::ProcessorInformation, SMBios::CacheInformation,
        SMBios::PortConnection, SMBios::SystemSlots, SMBios::OemStrings,
        SMBios::SystemConfigurationOptions, SMBios::FirmwareLanguageInformation,
        SMBios::GroupAssociations, SMBios::SystemEventLog, SMBios::PhysicalMemoryArray,
        SMBios::MemoryDevice, SMBios::MemoryErrorInformation32Bit, SMBios::MemoryArrayMappedAddress,
        SMBios::MemoryDeviceMappedAddress, SMBios::PointingDevice, SMBios::PortableBattery,
        SMBios::SystemReset, SMBios::HardwareSecurity, SMBios::SystemPowerControls,
        SMBios::VoltageProbe, SMBios::CoolingDevice, SMBios::TemperatureProbe,
        SMBios::ElectricalCurrentProbe, SMBios::OutOfBandRemoteAccess, SMBios::BootIntegrityServices,
        SMBios::SystemBootInformation, SMBios::MemoryErrorInformation64Bit, SMBios::ManagementDevice,
        SMBios::ManagementDeviceComponent, SMBios::ManagementDeviceThresholdData, SMBios::MemoryChannel,
        SMBios::IpmiDeviceInformation, SMBios::SystemPowerSupply, SMBios::AdditionalInformation,
        SMBios::OnboardDevicesExtendedInformation, SMBios::ManagementControllerHostInterface,
        SMBios::TpmDevice, SMBios::ProcessorAdditionalInformation, SMBios::FirmwareInventoryInformation,
        SMBios::StringProperty, SMBios::Inactive, SMBios::EndOfTable,
    };
}

/// @brief True iff `label` is entry_type_label()'s fallback form.
bool is_unknown_label(const std::string& label)
{
    return label.rfind("Unknown (0x", 0) == 0;
}

} // namespace

REGISTER_SMBIOS_ENTRY(kThrowsInGetFieldsType, ThrowingFieldsTestEntry)
REGISTER_SMBIOS_ENTRY(kThrowsInConstructorType, ThrowingConstructorTestEntry)
REGISTER_SMBIOS_ENTRY(kThrowsBadAllocType, BadAllocTestEntry)

BOOST_AUTO_TEST_SUITE(GuiViewModelUnitTests)

//////////////////////////////////////////////////////////////////////////
// Task 04's named cases

/// Normal case: several registered structures (BIOS, System Information,
/// two Memory Devices, OEM Strings) plus one unregistered OEM type yield
/// exactly one node each, in table order.
BOOST_AUTO_TEST_CASE(BuildEntryTreeMatchesStructureCountTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BIOSInformation, filled_area(), {"Synthetic Vendor", "SYN-BIOS-0001"})
        .add_structure(SMBios::SystemInformation, filled_area(), {"Synthetic Maker", "SYN-SYS-0001"})
        .add_structure(SMBios::MemoryDevice, filled_area(), {"DIMM-A", "SYN-MEM-0001"})
        .add_structure(SMBios::MemoryDevice, filled_area(), {"DIMM-B", "SYN-MEM-0002"})
        .add_structure(SMBios::OemStrings, {2}, {"synthetic oem one", "synthetic oem two"})
        .add_structure(0xC8, {0xAA, 0xBB});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    BOOST_REQUIRE_EQUAL(smbios::test::gui::count_structures(bios), static_cast<size_t>(6));

    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    check_nodes_match_walk(bios, nodes);
    // add_structure() assigns handles 1..N sequentially.
    for (size_t i = 0; i < nodes.size(); ++i) {
        BOOST_CHECK_EQUAL(nodes[i].handle, static_cast<uint16_t>(i + 1));
    }
}

/// Normal case: each registered node's fields equal - name for name, value
/// for value (same FieldValue alternative), same order - a direct
/// SMBiosEntryFactory::create() + get_fields() on the same structure under
/// the same version.
BOOST_AUTO_TEST_CASE(BuildEntryTreeFieldsMatchGetFieldsTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BIOSInformation, filled_area(), {"Synthetic Vendor", "SYN-BIOS-0001"})
        .add_structure(SMBios::SystemInformation, filled_area(), {"Synthetic Maker", "SYN-SYS-0001"})
        .add_structure(SMBios::MemoryDevice, filled_area(), {"DIMM-A", "SYN-MEM-0001"})
        .add_structure(SMBios::MemoryDevice, filled_area(), {"DIMM-B", "SYN-MEM-0002"})
        .add_structure(SMBios::OemStrings, {2}, {"synthetic oem one", "synthetic oem two"})
        .add_structure(0xC8, {0xAA, 0xBB});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(6));

    size_t index = 0;
    size_t registered = 0;
    for (const DMIHeader& header : bios) {
        BOOST_TEST_CONTEXT("node " << index << " (type " << static_cast<unsigned>(header.type) << ")")
        {
            const std::unique_ptr<AbstractSMBiosEntry> entry = SMBiosEntryFactory{}.create(
                header, bios.get_smbios_version(), bios.get_table_base() + bios.get_table_size());
            BOOST_CHECK(!nodes[index].decode_failed);
            if (entry) {
                ++registered;
                const std::vector<NamedField> expected = entry->get_fields();
                BOOST_CHECK(!expected.empty());
                smbios::test::check_fields_equal(to_named_fields(nodes[index].fields), expected);
            } else {
                BOOST_CHECK(nodes[index].fields.empty());
            }
        }
        ++index;
    }
    // Not vacuous: the five registered fixtures really were decoded.
    BOOST_CHECK_EQUAL(registered, static_cast<size_t>(5));

    // Spot-check the copy is raw: OEM Strings' strings appear verbatim.
    const std::vector<NamedField> oem = to_named_fields(nodes[4].fields);
    BOOST_REQUIRE_EQUAL(oem.size(), static_cast<size_t>(3));
    BOOST_CHECK(oem[0].value == smbios::test::num(2));
    BOOST_CHECK(oem[1].value == smbios::test::str("synthetic oem one"));
    BOOST_CHECK(oem[2].value == smbios::test::str("synthetic oem two"));
}

/// Normal case: an unregistered OEM type (0xC8) still becomes a node - empty
/// fields, "Unknown (0xC8)" label, decode_failed false - and does not stop
/// the structures after it from being decoded.
BOOST_AUTO_TEST_CASE(BuildEntryTreeUnregisteredTypeYieldsEmptyFieldsTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(0xC8, {0xAA, 0xBB}, {"synthetic oem data"})
        .add_structure(SMBios::OemStrings, {1}, {"after the unknown one"});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(2));

    BOOST_CHECK_EQUAL(nodes[0].type, static_cast<size_t>(0xC8));
    BOOST_CHECK_EQUAL(nodes[0].handle, static_cast<uint16_t>(1));
    BOOST_CHECK_EQUAL(nodes[0].label, "Unknown (0xC8)");
    BOOST_CHECK(nodes[0].fields.empty());
    BOOST_CHECK(!nodes[0].decode_failed);

    BOOST_CHECK_EQUAL(nodes[1].label, "OEM Strings");
    BOOST_REQUIRE_EQUAL(nodes[1].fields.size(), static_cast<size_t>(2));
    BOOST_CHECK(nodes[1].fields[1].value == smbios::test::str("after the unknown one"));
}

/// Normal case: every named SMBios::SMBiosHandler value gets a non-empty,
/// non-"Unknown" label, labels are pairwise distinct, and no unnamed value
/// in 0-255 gets a name (so the explicit list above is exactly the enum).
/// For every registered type the label also equals the entry class's own
/// get_type() text, as view_model.hpp documents.
BOOST_AUTO_TEST_CASE(EntryTypeLabelCoversEveryNamedHandlerValueTestCase)
{
    const std::vector<size_t> named = all_named_handler_values();
    BOOST_CHECK_EQUAL(named.size(), static_cast<size_t>(46));

    std::set<size_t> named_set(named.begin(), named.end());
    BOOST_CHECK_EQUAL(named_set.size(), named.size());

    std::set<std::string> labels;
    for (size_t type : named) {
        const std::string label = smbios::gui::entry_type_label(type);
        BOOST_CHECK_MESSAGE(!label.empty() && !is_unknown_label(label),
                            "type " << type << " has no name: \"" << label << '"');
        BOOST_CHECK_MESSAGE(labels.insert(label).second, "duplicate label \"" << label << '"');
    }

    for (size_t type = 0; type <= 0xFF; ++type) {
        const bool has_name = !is_unknown_label(smbios::gui::entry_type_label(type));
        BOOST_CHECK_MESSAGE(has_name == (named_set.count(type) == 1),
                            "type " << type << ": named=" << has_name << " but enum membership="
                                    << named_set.count(type));
    }

    SMBiosEntryFactory factory;
    for (size_t type : named) {
        if (type == SMBios::EndOfTable) {
            continue;  // never registered: the table walk stops at it
        }
        const std::vector<uint8_t> buffer = smbios::test::SyntheticTableBuilder()
            .add_structure(static_cast<uint8_t>(type), filled_area(), {"synthetic one", "synthetic two"})
            .build();
        DMIHeader header{};
        header.type = buffer[0];
        header.length = buffer[1];
        header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
        header.data = buffer.data();
        const std::unique_ptr<AbstractSMBiosEntry> entry =
            factory.create(header, SMBiosVersion{3, 9}, buffer.data() + buffer.size());
        BOOST_REQUIRE_MESSAGE(entry != nullptr, "type " << type << " is not registered");
        BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(type), entry->get_type());
    }
}

/// Boundary case: unnamed values - obsolete spec types (5, 6, 10), the first
/// unassigned spec type (47), the OEM range ends (0x80, 0xFF) and values
/// wider than a byte - use "Unknown (0xNN)", uppercase, at least two digits.
BOOST_AUTO_TEST_CASE(EntryTypeLabelUnknownFormTestCase)
{
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(5), "Unknown (0x05)");
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(6), "Unknown (0x06)");
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(10), "Unknown (0x0A)");
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(47), "Unknown (0x2F)");
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(0x80), "Unknown (0x80)");
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(0xFF), "Unknown (0xFF)");
    // size_t values past 0xFF never alias a byte-sized enum value.
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(0x100), "Unknown (0x100)");
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(0x100 + SMBios::BIOSInformation), "Unknown (0x100)");
    BOOST_CHECK_EQUAL(smbios::gui::entry_type_label(std::numeric_limits<size_t>::max()),
                      "Unknown (0x" + std::string(sizeof(size_t) * 2, 'F') + ")");
}

//////////////////////////////////////////////////////////////////////////
// Threat-model requirement 15: hostile strings

/// Security case (G-1/G-3/G-4): an OEM Strings structure whose DMI strings
/// are format specifiers, ImGui ID syntax, a forged newline row, terminal
/// escapes (ESC, C1 CSI), bidi/line-separator controls, invalid UTF-8 and a
/// 64 KiB string. build_entry_tree() keeps every byte unchanged; each
/// display_text() projection is valid UTF-8, free of C0/DEL/C1/bidi, capped
/// and idempotent; ImGui-syntax strings pass through display_text() as-is
/// (they are harmless as text - the renderer's job is never to use them as
/// a format string or ID).
BOOST_AUTO_TEST_CASE(HostileStringsKeptRawAndDisplaySafeTestCase)
{
    std::string big = "x";  // misaligns the 3-byte code points below by one
    while (big.size() < 64 * 1024) {
        big += "\xE2\x82\xAC";  // U+20AC, 3 bytes: forces a code-point-boundary cut
    }
    BOOST_REQUIRE_EQUAL(big.size(), static_cast<size_t>(64 * 1024));

    const std::vector<std::string> hostile = {
        "%n%s%x",                                // 1: format specifiers
        "##",                                    // 2: ImGui hidden-label syntax
        "###id",                                 // 3: ImGui ID override syntax
        "line1\nSerial Number: forged",          // 4: forged extra row
        "\x1B[2J",                               // 5: ESC CSI clear screen
        "\xC2\x9B" "31m",                        // 6: U+009B (C1 CSI)
        "\xE2\x80\xAE" "evil",                   // 7: U+202E RIGHT-TO-LEFT OVERRIDE
        "\xC0\xAF",                              // 8: overlong '/' (invalid UTF-8)
        "\xED\xA0\x80",                          // 9: lone surrogate (invalid UTF-8)
        "a\tb\rc\x7F" "d",                       // 10: TAB, CR, DEL
        "\xE2\x80\xA8\xE2\x81\xA6" "z",          // 11: U+2028, U+2066
        big,                                     // 12: 64 KiB
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {static_cast<uint8_t>(hostile.size())}, hostile);

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(1));
    BOOST_CHECK_EQUAL(nodes[0].label, "OEM Strings");
    BOOST_CHECK(!nodes[0].decode_failed);
    BOOST_REQUIRE_EQUAL(nodes[0].fields.size(), hostile.size() + 1);
    BOOST_CHECK(nodes[0].fields[0].value == smbios::test::num(hostile.size()));

    for (size_t i = 0; i < hostile.size(); ++i) {
        BOOST_TEST_CONTEXT("hostile string #" << (i + 1))
        {
            const FieldRow& row = nodes[0].fields[i + 1];
            BOOST_CHECK_EQUAL(row.name, "String " + std::to_string(i + 1));
            const auto* raw = std::get_if<std::string>(&row.value);
            BOOST_REQUIRE(raw != nullptr);
            // Raw bytes preserved: compared without printing (may be 64 KiB
            // or contain terminal escapes).
            BOOST_CHECK_MESSAGE(*raw == hostile[i], "view model altered the raw bytes");

            const std::string shown = smbios::gui::display_text(*raw);
            smbios::test::gui::check_display_safe(shown, "display_text(hostile #" + std::to_string(i + 1) + ")");
        }
    }

    const std::string fffd = "\xEF\xBF\xBD";
    BOOST_CHECK_EQUAL(smbios::gui::display_text(hostile[0]), "%n%s%x");
    BOOST_CHECK_EQUAL(smbios::gui::display_text(hostile[1]), "##");
    BOOST_CHECK_EQUAL(smbios::gui::display_text(hostile[2]), "###id");
    BOOST_CHECK(smbios::gui::display_text(hostile[3]) == "line1" + fffd + "Serial Number: forged");
    BOOST_CHECK(smbios::gui::display_text(hostile[4]) == fffd + "[2J");
    BOOST_CHECK(smbios::gui::display_text(hostile[5]) == fffd + "31m");
    BOOST_CHECK(smbios::gui::display_text(hostile[6]) == fffd + "evil");
    BOOST_CHECK(smbios::gui::display_text(hostile[9]) == "a" + fffd + "b" + fffd + "c" + fffd + "d");
    BOOST_CHECK(smbios::gui::display_text(hostile[10]) == fffd + fffd + "z");
    // Invalid UTF-8 becomes only replacement characters (count is
    // sanitize_utf8()'s business, see formatter_escaping.hpp).
    for (size_t i : {size_t{7}, size_t{8}}) {
        const std::string shown = smbios::gui::display_text(hostile[i]);
        BOOST_CHECK(!shown.empty());
        BOOST_CHECK_EQUAL(shown.size() % fffd.size(), static_cast<size_t>(0));
        for (size_t pos = 0; pos + fffd.size() <= shown.size(); pos += fffd.size()) {
            BOOST_CHECK(shown.compare(pos, fffd.size(), fffd) == 0);
        }
    }

    // 64 KiB: a prefix of the raw text, cut on a code-point boundary, plus
    // " ... [N bytes truncated]" where prefix + N == the full size.
    const std::string shown_big = smbios::gui::display_text(big);
    const std::string marker = " ... [";
    const size_t marker_pos = shown_big.rfind(marker);
    BOOST_REQUIRE(marker_pos != std::string::npos);
    const std::string tail = " bytes truncated]";
    BOOST_REQUIRE(shown_big.size() > tail.size());
    BOOST_CHECK(shown_big.compare(shown_big.size() - tail.size(), tail.size(), tail) == 0);
    const std::string count_text = shown_big.substr(marker_pos + marker.size(),
                                                    shown_big.size() - tail.size() - marker_pos - marker.size());
    BOOST_CHECK_EQUAL(marker_pos + std::stoull(count_text), big.size());
    BOOST_CHECK(big.compare(0, marker_pos, shown_big, 0, marker_pos) == 0);
    BOOST_CHECK_EQUAL((marker_pos - 1) % 3, static_cast<size_t>(0));  // boundary after 'x' + k * 3 bytes
}

//////////////////////////////////////////////////////////////////////////
// Threat-model requirement 16: malformed tables

/// Invalid-input case: a structure whose declared length overruns the
/// buffer ends the walk; the tree has exactly the iterable structures.
BOOST_AUTO_TEST_CASE(MalformedOversizedLengthNodeCountMatchesWalkTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemInformation, filled_area(), {"Synthetic Maker"})
        .add_structure(SMBios::OemStrings, {1}, {"synthetic"})
        .add_structure(0xC8, {})
        .add_raw_bytes({SMBios::MemoryDevice, 0xFF, 0x04, 0x00});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    check_nodes_match_walk(bios, nodes);
    BOOST_CHECK_EQUAL(nodes.size(), static_cast<size_t>(3));
}

/// Invalid-input case: a registered structure whose string section has no
/// terminator (the buffer ends mid-string), and whose string count claims
/// more strings than exist. One node; the entry's own bounds-checked string
/// parser stops at the table end, so missing strings read as "Bad index".
BOOST_AUTO_TEST_CASE(MalformedMissingTerminatorNodeCountMatchesWalkTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_raw_bytes({SMBios::OemStrings, 5, 0x01, 0x00, 3});
    builder.add_raw_bytes({'N', 'o', 'T', 'e', 'r', 'm'});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    check_nodes_match_walk(bios, nodes);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(1));
    BOOST_CHECK(!nodes[0].decode_failed);
    BOOST_REQUIRE_EQUAL(nodes[0].fields.size(), static_cast<size_t>(4));
    for (size_t i = 1; i < nodes[0].fields.size(); ++i) {
        BOOST_CHECK(nodes[0].fields[i].value == smbios::test::str("Bad index"));
    }
}

/// Invalid-input case: get_structures_count() != the iterable count
/// (length-valid bytes after EndOfTable inflate the former). The tree
/// follows the iterable walk, never get_structures_count().
BOOST_AUTO_TEST_CASE(MalformedStructureCountMismatchFollowsIterationTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemInformation, {});
    builder.add_raw_bytes(smbios::test::gui::end_of_table_structure(2));
    builder.add_raw_bytes({0x50, 6, 0x03, 0x00, 0xAA, 0xBB, 0x00, 0x00});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    BOOST_REQUIRE_NE(bios.get_structures_count(), smbios::test::gui::count_structures(bios));

    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    check_nodes_match_walk(bios, nodes);
    BOOST_CHECK_EQUAL(nodes.size(), static_cast<size_t>(1));
}

/// Invalid-input case: a header with length < 4 mid-table ends the walk.
BOOST_AUTO_TEST_CASE(MalformedShortLengthNodeCountMatchesWalkTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {1}, {"synthetic"});
    builder.add_raw_bytes({SMBios::MemoryDevice, 2, 0x02, 0x00, 0x00, 0x00});
    builder.add_raw_bytes(smbios::test::gui::end_of_table_structure(3));

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    check_nodes_match_walk(bios, nodes);
    BOOST_CHECK_EQUAL(nodes.size(), static_cast<size_t>(1));
}

/// Invalid-input case (sanitizer target): 500 deterministic pseudo-random
/// buffers, biased towards registered types and plausible lengths. Every
/// tree matches the walk node-for-node and build_entry_tree() never throws;
/// under ASan/UBSan this is also the out-of-bounds-read proof.
BOOST_AUTO_TEST_CASE(MalformedRandomTablesNodeCountMatchesWalkTestCase)
{
    uint32_t state = 0x5EED1234u;
    const auto next = [&state]() {
        state = state * 1664525u + 1013904223u;  // Numerical Recipes LCG
        return static_cast<uint8_t>(state >> 24);
    };

    for (int round = 0; round < 500; ++round) {
        std::vector<uint8_t> buffer;
        const size_t structures = next() % 6;
        for (size_t s = 0; s < structures; ++s) {
            const uint8_t type = static_cast<uint8_t>(next() % 48);
            const uint8_t length = static_cast<uint8_t>(next() % 0x70);
            buffer.push_back(type);
            buffer.push_back(length);
            buffer.push_back(next());
            buffer.push_back(next());
            const size_t body = length > 4 ? length - 4u : 0u;
            for (size_t b = 0; b < body; ++b) {
                buffer.push_back(next() % 4);  // small values: plausible string indices
            }
            const size_t tail = next() % 12;
            for (size_t b = 0; b < tail; ++b) {
                buffer.push_back(next() % 3 == 0 ? 0 : static_cast<uint8_t>('a' + next() % 26));
            }
        }
        BOOST_TEST_CONTEXT("round " << round << ", " << buffer.size() << " bytes")
        {
            SMBios bios;
            smbios::test::gui::load_table(bios, buffer);
            std::vector<EntryNode> nodes;
            BOOST_CHECK_NO_THROW(nodes = smbios::gui::build_entry_tree(bios));
            check_nodes_match_walk(bios, nodes);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// Threat-model requirement 17: duplicate handles

/// Security case (G-2): two OEM Strings structures with the same type and
/// the same handle yield two distinct nodes, each with its own fields - not
/// merged, deduplicated or overwritten. Their list labels are identical,
/// which is exactly why widget IDs must key on the node index.
BOOST_AUTO_TEST_CASE(DuplicateHandlesYieldDistinctNodesTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_raw_bytes(smbios::test::gui::oem_strings_structure(0x0042, {"duplicate first"}));
    builder.add_raw_bytes(smbios::test::gui::oem_strings_structure(0x0042, {"duplicate second"}));
    builder.add_raw_bytes(smbios::test::gui::end_of_table_structure(0x0043));

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    const std::vector<EntryNode> nodes = smbios::gui::build_entry_tree(bios);
    check_nodes_match_walk(bios, nodes);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(2));

    for (const EntryNode& node : nodes) {
        BOOST_CHECK_EQUAL(node.type, static_cast<size_t>(SMBios::OemStrings));
        BOOST_CHECK_EQUAL(node.handle, static_cast<uint16_t>(0x0042));
        BOOST_REQUIRE_EQUAL(node.fields.size(), static_cast<size_t>(2));
    }
    BOOST_CHECK(nodes[0].fields[1].value == smbios::test::str("duplicate first"));
    BOOST_CHECK(nodes[1].fields[1].value == smbios::test::str("duplicate second"));
}

//////////////////////////////////////////////////////////////////////////
// Threat-model requirement 19: the empty table

/// Boundary case: no table at all (empty buffer) - build_entry_tree() does
/// not throw and returns no nodes.
BOOST_AUTO_TEST_CASE(EmptyTableYieldsNoNodesTestCase)
{
    SMBios bios;
    smbios::test::gui::load_table(bios, {});
    BOOST_REQUIRE_EQUAL(bios.get_structures_count(), static_cast<size_t>(0));
    BOOST_REQUIRE(bios.begin() == bios.end());

    std::vector<EntryNode> nodes{EntryNode{}};
    BOOST_CHECK_NO_THROW(nodes = smbios::gui::build_entry_tree(bios));
    BOOST_CHECK(nodes.empty());
}

/// Boundary case: a table holding only the EndOfTable marker - nothing is
/// iterable, so no node (EndOfTable is never a node).
BOOST_AUTO_TEST_CASE(EndOfTableOnlyYieldsNoNodesTestCase)
{
    SMBios bios;
    smbios::test::gui::load_table(bios, smbios::test::SyntheticTableBuilder().build());
    BOOST_REQUIRE(bios.begin() == bios.end());

    std::vector<EntryNode> nodes{EntryNode{}};
    BOOST_CHECK_NO_THROW(nodes = smbios::gui::build_entry_tree(bios));
    BOOST_CHECK(nodes.empty());
}

//////////////////////////////////////////////////////////////////////////
// Requirement 1 (G-6): per-entry exception isolation

/// Error case: an entry whose get_fields() throws std::runtime_error, and
/// one whose constructor throws a non-std exception, each still yield
/// exactly one node with empty fields and decode_failed set; neighbouring
/// structures are decoded normally.
BOOST_AUTO_TEST_CASE(ThrowingEntryYieldsDecodeFailedNodeTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {1}, {"before"})
        .add_structure(kThrowsInGetFieldsType, {0x01})
        .add_structure(kThrowsInConstructorType, {0x01})
        .add_structure(SMBios::OemStrings, {1}, {"after"});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    std::vector<EntryNode> nodes;
    BOOST_REQUIRE_NO_THROW(nodes = smbios::gui::build_entry_tree(bios));
    check_nodes_match_walk(bios, nodes);
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(4));

    BOOST_CHECK(!nodes[0].decode_failed);
    BOOST_CHECK(nodes[1].decode_failed);
    BOOST_CHECK(nodes[1].fields.empty());
    BOOST_CHECK_EQUAL(nodes[1].label, "Unknown (0xF3)");
    BOOST_CHECK(nodes[2].decode_failed);
    BOOST_CHECK(nodes[2].fields.empty());
    BOOST_CHECK(!nodes[3].decode_failed);
    BOOST_REQUIRE_EQUAL(nodes[3].fields.size(), static_cast<size_t>(2));
    BOOST_CHECK(nodes[3].fields[1].value == smbios::test::str("after"));
}

/// Error case: std::bad_alloc is the one exception build_entry_tree()
/// propagates rather than isolating.
BOOST_AUTO_TEST_CASE(BadAllocPropagatesTestCase)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OemStrings, {1}, {"before"})
        .add_structure(kThrowsBadAllocType, {0x01});

    SMBios bios;
    smbios::test::gui::load_table(bios, builder.build());
    BOOST_CHECK_THROW(smbios::gui::build_entry_tree(bios), std::bad_alloc);
}

/// Ownership case (requirement 3, G-7): nodes are copies - they stay intact
/// after the SMBios they were built from is destroyed.
BOOST_AUTO_TEST_CASE(NodesOutliveSmbiosTestCase)
{
    std::vector<EntryNode> nodes;
    {
        SMBios bios;
        smbios::test::gui::load_table(
            bios, smbios::test::SyntheticTableBuilder().add_structure(SMBios::OemStrings, {1}, {"kept copy"}).build());
        nodes = smbios::gui::build_entry_tree(bios);
    }
    BOOST_REQUIRE_EQUAL(nodes.size(), static_cast<size_t>(1));
    BOOST_REQUIRE_EQUAL(nodes[0].fields.size(), static_cast<size_t>(2));
    BOOST_CHECK(nodes[0].fields[1].value == smbios::test::str("kept copy"));
}

BOOST_AUTO_TEST_SUITE_END()
