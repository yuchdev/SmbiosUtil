#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/memory_channel_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.hpp"
#include "field_assertions.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive MemoryChannelEntry
// (smbios/include/smbios/tables/memory_channel_entry.hpp), cross-checked
// directly against DSP0134 3.9.0's own 7.38/7.38.1 (Table 112/113).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  channel_type (enum, Table 113)
//   offset 0x05 : uint8_t  maximum_channel_load
//   offset 0x06 : uint8_t  memory_device_count (n) - EXPLICIT count field
//   offset 0x07 + 3*i : uint8_t  load[i]
//   offset 0x08 + 3*i : uint16_t handle[i]
//
// Per Table 112's own Length row, Length ("must not be used to determine
// the number of memory devices") is the OPPOSITE derivation direction from
// GroupAssociationsEntry (7.15, Story 02.0) - see memory_channel_entry.hpp's
// own class doc comment. These tests exist specifically to confirm
// get_device_count() reads the explicit 0x06 field, never derives from
// get_entry_size()/Length.

BOOST_AUTO_TEST_SUITE(MemoryChannelEntryUnitTests)

namespace {

DMIHeader make_header(const std::vector<uint8_t>& buffer)
{
    DMIHeader header{};
    header.type = buffer[0];
    header.length = buffer[1];
    header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
    header.data = buffer.data();
    return header;
}

/// Build a formatted area for `device_count` trailing (Load, Handle) pairs,
/// writing the explicit Memory Device Count field (0x06) as `device_count`
/// unless overridden by `explicit_count_override`.
std::vector<uint8_t> make_formatted_area(uint8_t channel_type, uint8_t max_load,
                                          const std::vector<std::pair<uint8_t, uint16_t>>& devices,
                                          int explicit_count_override = -1)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(channel_type); // 0x04
    formatted_area.push_back(max_load);     // 0x05
    formatted_area.push_back(explicit_count_override >= 0
                                  ? static_cast<uint8_t>(explicit_count_override)
                                  : static_cast<uint8_t>(devices.size())); // 0x06
    for (const auto& device : devices) {
        formatted_area.push_back(device.first);                                // Load
        formatted_area.push_back(static_cast<uint8_t>(device.second & 0xFF));         // Handle low
        formatted_area.push_back(static_cast<uint8_t>((device.second >> 8) & 0xFF));  // Handle high
    }
    return formatted_area;
}

} // namespace

/// Edge case: 0 devices. get_device_count() must be 0, no device accessor
/// may read anything meaningful.
BOOST_AUTO_TEST_CASE(ZeroDevicesTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        MemoryChannelEntry::ChannelTypeRambus, 12, {});
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x07));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryChannel, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    MemoryChannelEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_channel_type(), MemoryChannelEntry::ChannelTypeRambus);
    BOOST_CHECK_EQUAL(entry.get_channel_type_string(), std::string("Rambus"));
    BOOST_CHECK_EQUAL(entry.get_maximum_channel_load(), static_cast<uint8_t>(12));
    BOOST_CHECK_EQUAL(entry.get_device_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_load_at(0), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(0), static_cast<uint16_t>(0));
}

/// Normal case: 1 device.
BOOST_AUTO_TEST_CASE(OneDeviceTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        MemoryChannelEntry::ChannelTypeSyncLink, 20, {{8, 0x0011}});
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x0A));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryChannel, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    MemoryChannelEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_channel_type(), MemoryChannelEntry::ChannelTypeSyncLink);
    BOOST_CHECK_EQUAL(entry.get_channel_type_string(), std::string("SyncLink"));
    BOOST_CHECK_EQUAL(entry.get_maximum_channel_load(), static_cast<uint8_t>(20));
    BOOST_CHECK_EQUAL(entry.get_device_count(), static_cast<uint8_t>(1));
    BOOST_CHECK_EQUAL(entry.get_device_load_at(0), static_cast<uint8_t>(8));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(0), static_cast<uint16_t>(0x0011));

    // Out-of-range index must not be confused with device 0.
    BOOST_CHECK_EQUAL(entry.get_device_load_at(1), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(1), static_cast<uint16_t>(0));
}

/// Normal case: 3+ devices.
BOOST_AUTO_TEST_CASE(ThreeOrMoreDevicesTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        MemoryChannelEntry::ChannelTypeOther, 30,
        {
            {8, 0x0011},
            {8, 0x0012},
            {8, 0x0013},
            {4, 0x0014}
        });
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x07 + 4 * 3));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryChannel, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    MemoryChannelEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_device_count(), static_cast<uint8_t>(4));

    BOOST_CHECK_EQUAL(entry.get_device_load_at(0), static_cast<uint8_t>(8));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(0), static_cast<uint16_t>(0x0011));
    BOOST_CHECK_EQUAL(entry.get_device_load_at(1), static_cast<uint8_t>(8));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(1), static_cast<uint16_t>(0x0012));
    BOOST_CHECK_EQUAL(entry.get_device_load_at(2), static_cast<uint8_t>(8));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(2), static_cast<uint16_t>(0x0013));
    BOOST_CHECK_EQUAL(entry.get_device_load_at(3), static_cast<uint8_t>(4));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(3), static_cast<uint16_t>(0x0014));

    BOOST_CHECK_EQUAL(entry.get_device_load_at(4), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(4), static_cast<uint16_t>(0));
}

/// Regression-catching case: this is the test that would FAIL if someone
/// switched get_device_count() to a Length-derived formula (mirroring
/// GroupAssociationsEntry's own `(Length - 5) / 3`) instead of reading the
/// explicit 0x06 field.
///
/// Construct a structure whose own Length implies 1 device
/// ((Length - 7) / 3 = (10 - 7) / 3 = 1) via a Length-derived formula, but
/// whose EXPLICIT Memory Device Count field (0x06) says 0. Length and the
/// explicit field disagree on purpose. If get_device_count() were ever
/// (mis)implemented as Length-derived, this test would observe
/// get_device_count() == 1 and FAIL against the assertion below, which
/// requires the explicit field (0) to win.
///
/// add_raw_bytes() is used because SyntheticTableBuilder::add_structure()
/// always computes a length consistent with the formatted area it is
/// given; this test needs a formatted area sized for 1 trailing
/// (Load, Handle) pair (so Length, if miscomputed from, would imply 1
/// device) while the explicit count field inside that same area says 0.
BOOST_AUTO_TEST_CASE(ExplicitCountFieldWinsOverLengthDerivedCountTestCase)
{
    // Structure layout (raw, header included):
    //   0x00 type   = MemoryChannel (37)
    //   0x01 length = 0x0A (10): 4-byte header + channel_type + max_load +
    //                 explicit_count + one trailing (Load, Handle) triplet
    //                 = 4 + 1 + 1 + 1 + 3 = 10. A Length-derived formula
    //                 mirroring Group Associations' "(Length - base) /
    //                 stride" shape would read this Length as implying 1
    //                 trailing device.
    //   0x02 handle = 0x0001
    //   0x04 channel_type = ChannelTypeOther (1)
    //   0x05 maximum_channel_load = 5
    //   0x06 memory_device_count (EXPLICIT) = 0 <- disagrees with Length
    //   0x07 Load[0]   = 9    (present in the buffer, but must NOT be
    //   0x08-0x09 Handle[0] = 0x00AA   exposed by any accessor, since the
    //                                   explicit count says there are 0
    //                                   devices)
    constexpr uint8_t length = 0x0A; // 10
    std::vector<uint8_t> raw = {
        static_cast<uint8_t>(SMBios::MemoryChannel), length, 0x01, 0x00, // header
        static_cast<uint8_t>(MemoryChannelEntry::ChannelTypeOther),      // 0x04
        0x05,                                                            // 0x05
        0x00,                                                            // 0x06 explicit count = 0
        0x09, 0xAA, 0x00                                                 // 0x07-0x09 stray trailing triplet
    };
    BOOST_REQUIRE_EQUAL(raw.size(), static_cast<size_t>(length));

    // String section: no strings, just the closing double-NUL.
    raw.push_back(0x00);
    raw.push_back(0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_raw_bytes(raw);
    builder.add_raw_bytes(std::vector<uint8_t>{SMBios::EndOfTable, 0x04, 0x00, 0x00});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    MemoryChannelEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE_EQUAL(entry.get_entry_size(), static_cast<size_t>(length));

    // The explicit field (0) must win - this is the core assertion a
    // Length-derived regression would break.
    BOOST_CHECK_EQUAL(entry.get_device_count(), static_cast<uint8_t>(0));

    // Because get_device_count() == 0, every device accessor must report
    // nothing, even though the stray trailing triplet is physically present
    // and in-bounds in the buffer.
    BOOST_CHECK_EQUAL(entry.get_device_load_at(0), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_handle_at(0), static_cast<uint16_t>(0));
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(MemoryChannelEntry::ChannelTypeOther, 1, {});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(MemoryChannelEntry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 05.0 Task 03's own success criterion: MemoryChannel
/// resolves through SMBiosEntryFactory::create() to a real
/// MemoryChannelEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsMemoryChannelEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        MemoryChannelEntry::ChannelTypeOther, 1, {{1, 0x0001}});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryChannel, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<MemoryChannelEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009, Story 01.0, Task 03)
//
// Type: unit - deterministic, hand-built buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description() field by field;
// the text's unnumbered per-device "Load: N, Handle: H" lines become
// 1-based "Load <i>"/"Handle <i>" uint64_t fields (both printed as plain
// decimals). No bitmask in this type.
// Boundaries: an explicit device count of 0 with a stray trailing triplet
// (no device fields), a structure truncated before Maximum Channel Load.
// On failure first check: MemoryChannelEntry::get_fields() in
// libsmbios/src/memory_channel_entry.cpp.

/// Normal case: fixed fields then one indexed Load/Handle pair per device.
BOOST_AUTO_TEST_CASE(MemoryChannelEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = make_formatted_area(
        MemoryChannelEntry::ChannelTypeRambus, 10, {{2, 0x0040}, {3, 0x0041}});
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryChannel, formatted_area);
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    MemoryChannelEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Channel Type", str("Rambus")},
        {"Maximum Channel Load", num(10)},
        {"Memory Device Count", num(2)},
        {"Load 1", num(2)},
        {"Handle 1", num(0x0040)},
        {"Load 2", num(3)},
        {"Handle 2", num(0x0041)},
    });
}

/// Boundary cases: an explicit count of 0 prints no device lines even with a
/// stray triplet in-bounds; a structure holding only Channel Type still
/// prints every fixed line, with defaulted load and count.
BOOST_AUTO_TEST_CASE(MemoryChannelEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> formatted_area = make_formatted_area(
            MemoryChannelEntry::ChannelTypeOther, 5, {{9, 0x00AA}}, /*explicit_count_override*/0);
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::MemoryChannel, formatted_area);
        std::vector<uint8_t> buffer = builder.build();
        MemoryChannelEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Channel Type", str("Other")},
            {"Maximum Channel Load", num(5)},
            {"Memory Device Count", num(0)},
        });
    }
    {
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::MemoryChannel,
                              std::vector<uint8_t>{static_cast<uint8_t>(MemoryChannelEntry::ChannelTypeSyncLink)});
        std::vector<uint8_t> buffer = builder.build();
        MemoryChannelEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Channel Type", str("SyncLink")},
            {"Maximum Channel Load", num(0)},
            {"Memory Device Count", num(0)},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
