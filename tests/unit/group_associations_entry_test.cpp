#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/tables/group_associations_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive GroupAssociationsEntry
// (smbios/include/smbios/tables/group_associations_entry.h), cross-checked
// directly against DSP0134 3.9.0's own 7.15 (Table 59) and its own note and
// worked Example 1/Example 2.
//
// Wire layout:
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  group_name (STRING)
//   offset 0x05 + 3*i : uint8_t  item_type[i]
//   offset 0x06 + 3*i : uint16_t item_handle[i]
//
// Per 7.15's own note, there is NO count field anywhere in this structure -
// item count is purely (Length - 5) / 3, derived from the header's own
// length byte. These tests never write a count byte.

BOOST_AUTO_TEST_SUITE(GroupAssociationsEntryUnitTests)

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

/// Build a formatted area for `item_count` trailing (Item Type, Item
/// Handle) pairs, Group Name at string index 1.
std::vector<uint8_t> make_formatted_area(uint8_t group_name_index,
                                          const std::vector<std::pair<uint8_t, uint16_t>>& items)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(group_name_index); // 0x04
    for (const auto& item : items) {
        formatted_area.push_back(item.first);                      // Item Type
        formatted_area.push_back(static_cast<uint8_t>(item.second & 0xFF));        // Item Handle low
        formatted_area.push_back(static_cast<uint8_t>((item.second >> 8) & 0xFF)); // Item Handle high
    }
    return formatted_area;
}

} // namespace

/// Edge case: 0 items. get_item_count() must be 0, no item accessor may
/// read anything meaningful.
BOOST_AUTO_TEST_CASE(ZeroItemsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(/*group_name_index*/1, {});
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x05));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::GroupAssociations, formatted_area, std::vector<std::string>{"Empty Group"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    GroupAssociationsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_group_name_string(), std::string("Empty Group"));
    BOOST_CHECK_EQUAL(entry.get_item_count(), static_cast<size_t>(0));
    BOOST_CHECK_EQUAL(entry.get_item_type_at(0), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(0), static_cast<uint16_t>(0));
}

/// Normal case: 1 item, per 7.15's own shape (just a single CPU reference).
BOOST_AUTO_TEST_CASE(OneItemTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        /*group_name_index*/1, {{SMBios::ProcessorInformation, 0x0008}});
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x08));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::GroupAssociations, formatted_area, std::vector<std::string>{"Single CPU"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    GroupAssociationsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_group_name_string(), std::string("Single CPU"));
    BOOST_CHECK_EQUAL(entry.get_item_count(), static_cast<size_t>(1));
    BOOST_CHECK_EQUAL(entry.get_item_type_at(0), static_cast<uint8_t>(SMBios::ProcessorInformation));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(0), static_cast<uint16_t>(0x0008));
    BOOST_CHECK_EQUAL(entry.get_item_type_string_at(0), std::string("Processor Information"));

    // Out-of-range index must not be confused with item 0.
    BOOST_CHECK_EQUAL(entry.get_item_type_at(1), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(1), static_cast<uint16_t>(0));
}

/// Normal case: 3+ items, reproducing 7.15's own worked Example 1 verbatim
/// (Group "Primary CPU Module": a CPU structure handle 08h, a Cache
/// structure handle 09h), plus a third synthetic item to exceed the
/// "3+" bar explicitly.
BOOST_AUTO_TEST_CASE(ThreeOrMoreItemsFromSpecExampleTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        /*group_name_index*/1,
        {
            {SMBios::ProcessorInformation, 0x0008},
            {SMBios::CacheInformation, 0x0009},
            {SMBios::SystemSlots, 0x000A}
        });
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x0E));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::GroupAssociations, formatted_area, std::vector<std::string>{"Primary CPU Module"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    GroupAssociationsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_group_name_string(), std::string("Primary CPU Module"));
    BOOST_CHECK_EQUAL(entry.get_item_count(), static_cast<size_t>(3));

    BOOST_CHECK_EQUAL(entry.get_item_type_at(0), static_cast<uint8_t>(SMBios::ProcessorInformation));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(0), static_cast<uint16_t>(0x0008));
    BOOST_CHECK_EQUAL(entry.get_item_type_at(1), static_cast<uint8_t>(SMBios::CacheInformation));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(1), static_cast<uint16_t>(0x0009));
    BOOST_CHECK_EQUAL(entry.get_item_type_at(2), static_cast<uint8_t>(SMBios::SystemSlots));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(2), static_cast<uint16_t>(0x000A));

    BOOST_CHECK_EQUAL(entry.get_item_type_string_at(0), std::string("Processor Information"));
    BOOST_CHECK_EQUAL(entry.get_item_type_string_at(1), std::string("Cache Information"));
    BOOST_CHECK_EQUAL(entry.get_item_type_string_at(2), std::string("System Slots"));
}

/// Invalid-input/bounds-safety case: a malformed structure whose Length
/// does not divide evenly by 3 after subtracting 5 - here Length = 5 + 3*2
/// + 1 = 12, one stray trailing byte beyond two full items. Uses
/// add_raw_bytes() directly (bypassing SyntheticTableBuilder's own
/// well-formed-by-default add_structure(), which always computes a
/// consistent length) to construct the structure's raw formatted area with
/// an explicit, deliberately-inconsistent Length byte.
///
/// get_item_count() truncates via integer division to 2 (harmless), but
/// the real property under test is that get_item_type_at()/
/// get_item_handle_at() never read past get_entry_size() even when called
/// with an index a miscounted get_item_count() might technically allow -
/// each is independently guarded against get_entry_size() directly, not
/// just against get_item_count().
BOOST_AUTO_TEST_CASE(MalformedLengthNotDivisibleByThreeStaysInBoundsTestCase)
{
    // Structure layout (raw, header included):
    //   0x00 type   = GroupAssociations (14)
    //   0x01 length = 12 (0x0C): 4-byte header + 1 (group name) + 2*3
    //                 (two full items) + 1 stray trailing byte = 12
    //   0x02 handle = 0x0001
    //   0x04 group_name index = 1
    //   0x05 item[0].type = ProcessorInformation (4)
    //   0x06 item[0].handle = 0x0008 (LE)
    //   0x08 item[1].type = CacheInformation (7)
    //   0x09 item[1].handle = 0x0009 (LE)
    //   0x0B stray trailing byte (not a third item - only 1 byte, not 3)
    //   then the DMI string section: "Group X\0\0"
    constexpr uint8_t length = 0x0C; // 12
    std::vector<uint8_t> raw = {
        static_cast<uint8_t>(SMBios::GroupAssociations), length, 0x01, 0x00, // header
        0x01,                                                                 // group name index
        static_cast<uint8_t>(SMBios::ProcessorInformation), 0x08, 0x00,       // item 0
        static_cast<uint8_t>(SMBios::CacheInformation), 0x09, 0x00,           // item 1
        0xFF                                                                  // stray trailing byte
    };
    BOOST_REQUIRE_EQUAL(raw.size(), static_cast<size_t>(length));

    // String section: one string "Group X" then the closing double-NUL.
    const char* group_name = "Group X";
    for (const char* p = group_name; *p != '\0'; ++p) raw.push_back(static_cast<uint8_t>(*p));
    raw.push_back(0x00); // terminate string 1
    raw.push_back(0x00); // terminate string section

    smbios::test::SyntheticTableBuilder builder;
    builder.add_raw_bytes(raw);
    // add_raw_bytes() suppresses the automatic EndOfTable marker, so add one
    // explicitly to keep the table well-formed for the walk itself.
    builder.add_raw_bytes(std::vector<uint8_t>{SMBios::EndOfTable, 0x04, 0x00, 0x00});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    GroupAssociationsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE_EQUAL(entry.get_entry_size(), static_cast<size_t>(length));
    BOOST_CHECK_EQUAL(entry.get_group_name_string(), std::string("Group X"));

    // (12 - 5) / 3 = 2 (integer division truncation - harmless).
    BOOST_CHECK_EQUAL(entry.get_item_count(), static_cast<size_t>(2));

    // Both in-range items still decode correctly.
    BOOST_CHECK_EQUAL(entry.get_item_type_at(0), static_cast<uint8_t>(SMBios::ProcessorInformation));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(0), static_cast<uint16_t>(0x0008));
    BOOST_CHECK_EQUAL(entry.get_item_type_at(1), static_cast<uint8_t>(SMBios::CacheInformation));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(1), static_cast<uint16_t>(0x0009));

    // Index 2's Item Type byte sits at offset 0x0B - still inside this
    // structure's own 12-byte (0x0C) formatted area, so reading it is not
    // an out-of-bounds access; it legitimately returns the stray trailing
    // byte (0xFF), demonstrating that the malformed-Length case is not
    // caught at the item-type granularity. Index 2's Item Handle, however,
    // needs offset 0x0C-0x0D - 0x0C is already one-past-the-end of this
    // structure's own get_entry_size() (12) - so that read must be refused
    // and must not spill into the Group Name string bytes/next
    // structure's bytes that immediately follow in the buffer. This is the
    // real bounds-safety property this task cares about: the per-field
    // get_entry_size() guard, not get_item_count(), is what prevents the
    // out-of-bounds read.
    BOOST_CHECK_EQUAL(entry.get_item_type_at(2), static_cast<uint8_t>(0xFF));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(2), static_cast<uint16_t>(0));

    // An index far beyond even a sane count must also stay safely in
    // bounds (same guard path).
    BOOST_CHECK_EQUAL(entry.get_item_type_at(1000), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_item_handle_at(1000), static_cast<uint16_t>(0));
}

/// Milestone 0010 Story 02.0 Task 03's own success criterion:
/// GroupAssociations resolves through SMBiosEntryFactory::create() to a
/// real GroupAssociationsEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsGroupAssociationsEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        /*group_name_index*/1, {{SMBios::ProcessorInformation, 0x0008}});
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::GroupAssociations, formatted_area, std::vector<std::string>{"Single CPU"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<GroupAssociationsEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
