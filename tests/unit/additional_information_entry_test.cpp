#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/tables/additional_information_entry.h>
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
// smbios::test::SyntheticTableBuilder) drive AdditionalInformationEntry
// (smbios/include/smbios/tables/additional_information_entry.h),
// cross-checked directly against DSP0134 3.9.0's own 7.41/7.41.1 (Table
// 118/119).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04        : uint8_t  entry_count (n)
//   offset 0x05..      : n Additional Information Entries, each
//                         {uint8_t entry_length, uint16_t referenced_handle,
//                          uint8_t referenced_offset, uint8_t string_index,
//                          (entry_length - 5) raw value bytes}

BOOST_AUTO_TEST_SUITE(AdditionalInformationEntryUnitTests)

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

/// One Additional Information Entry to be serialized by
/// make_formatted_area().
struct EntrySpec {
    uint16_t referenced_handle;
    uint8_t referenced_offset;
    uint8_t string_index;
    std::vector<uint8_t> value;
};

std::vector<uint8_t> make_formatted_area(const std::vector<EntrySpec>& entries)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(static_cast<uint8_t>(entries.size())); // 0x04 n

    for (const EntrySpec& entry : entries) {
        const uint8_t entry_length = static_cast<uint8_t>(5 + entry.value.size());
        formatted_area.push_back(entry_length); // +0x00
        formatted_area.push_back(static_cast<uint8_t>(entry.referenced_handle & 0xFF)); // +0x01
        formatted_area.push_back(static_cast<uint8_t>((entry.referenced_handle >> 8) & 0xFF));
        formatted_area.push_back(entry.referenced_offset); // +0x03
        formatted_area.push_back(entry.string_index); // +0x04
        for (uint8_t b : entry.value) formatted_area.push_back(b); // +0x05..
    }

    return formatted_area;
}

} // namespace

/// Edge case: 0 Additional Information entries.
BOOST_AUTO_TEST_CASE(ZeroEntriesTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area({});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::AdditionalInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    AdditionalInformationEntry entry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_entry_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_entry_offsets_count(), static_cast<size_t>(0));
    BOOST_CHECK(entry.get_value_bytes_at(0).empty());
}

/// Normal case: exactly 1 entry.
BOOST_AUTO_TEST_CASE(OneEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area({
        {0x0010, 0x05, 1, {0xAB}}
    });

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::AdditionalInformation, formatted_area, {"Proposed CPU ID"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    AdditionalInformationEntry entry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_entry_count(), static_cast<uint8_t>(1));
    BOOST_CHECK_EQUAL(entry.get_entry_offsets_count(), static_cast<size_t>(1));
    BOOST_CHECK_EQUAL(entry.get_entry_length_at(0), static_cast<uint8_t>(6));
    BOOST_CHECK_EQUAL(entry.get_referenced_handle_at(0), static_cast<uint16_t>(0x0010));
    BOOST_CHECK_EQUAL(entry.get_referenced_offset_at(0), static_cast<uint8_t>(0x05));
    BOOST_CHECK_EQUAL(entry.get_string_index_at(0), static_cast<uint8_t>(1));
    BOOST_CHECK_EQUAL(entry.get_string_at(0), std::string("Proposed CPU ID"));
    BOOST_CHECK(entry.get_value_bytes_at(0) == (std::vector<uint8_t>{0xAB}));
}

/// Normal case: 3+ entries with DIFFERENT Entry Length values (1, 4 and 0
/// value bytes respectively), proving the per-entry-length walk correctly
/// uses each entry's own Entry Length to find the next entry's start,
/// rather than assuming a fixed stride.
BOOST_AUTO_TEST_CASE(MultipleEntriesOfDifferingLengthsWalkCorrectlyTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area({
        {0x0020, 0x06, 1, {0x01}},                   // Entry Length 6
        {0x0021, 0x08, 2, {0xAA, 0xBB, 0xCC, 0xDD}},  // Entry Length 9
        {0x0022, 0x0A, 0, {0xEE}}                     // Entry Length 6
    });

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::AdditionalInformation, formatted_area, {"String one", "String two"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    AdditionalInformationEntry entry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_entry_count(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_entry_offsets_count(), static_cast<size_t>(3));

    // Entry 0: Entry Length 6.
    BOOST_CHECK_EQUAL(entry.get_entry_length_at(0), static_cast<uint8_t>(6));
    BOOST_CHECK_EQUAL(entry.get_referenced_handle_at(0), static_cast<uint16_t>(0x0020));
    BOOST_CHECK_EQUAL(entry.get_referenced_offset_at(0), static_cast<uint8_t>(0x06));
    BOOST_CHECK_EQUAL(entry.get_string_index_at(0), static_cast<uint8_t>(1));
    BOOST_CHECK(entry.get_value_bytes_at(0) == (std::vector<uint8_t>{0x01}));

    // Entry 1: Entry Length 9 (differs from entry 0's 6) - proves the walk
    // advanced by entry 0's own 6 bytes, not a hardcoded fixed stride.
    BOOST_CHECK_EQUAL(entry.get_entry_length_at(1), static_cast<uint8_t>(9));
    BOOST_CHECK_EQUAL(entry.get_referenced_handle_at(1), static_cast<uint16_t>(0x0021));
    BOOST_CHECK_EQUAL(entry.get_referenced_offset_at(1), static_cast<uint8_t>(0x08));
    BOOST_CHECK_EQUAL(entry.get_string_index_at(1), static_cast<uint8_t>(2));
    BOOST_CHECK(entry.get_value_bytes_at(1) == (std::vector<uint8_t>{0xAA, 0xBB, 0xCC, 0xDD}));

    // Entry 2: Entry Length 6 again - its correct location (immediately
    // after entry 1's 9 bytes, not after a fixed 6- or 9-byte stride from
    // entry 1) confirms the walk used entry 1's own length, not entry 0's.
    BOOST_CHECK_EQUAL(entry.get_entry_length_at(2), static_cast<uint8_t>(6));
    BOOST_CHECK_EQUAL(entry.get_referenced_handle_at(2), static_cast<uint16_t>(0x0022));
    BOOST_CHECK_EQUAL(entry.get_referenced_offset_at(2), static_cast<uint8_t>(0x0A));
    BOOST_CHECK_EQUAL(entry.get_string_index_at(2), static_cast<uint8_t>(0));
    BOOST_CHECK(entry.get_value_bytes_at(2) == (std::vector<uint8_t>{0xEE}));

    // Out-of-range index must not be confused with a real entry.
    BOOST_CHECK_EQUAL(entry.get_entry_length_at(3), static_cast<uint8_t>(0));
    BOOST_CHECK(entry.get_value_bytes_at(3).empty());
}

/// Invalid-input/bounds-safety case: the last entry's declared Entry Length
/// claims more bytes than the structure's own buffer (get_entry_size())
/// actually has. The offset-caching walk must stop cleanly before that
/// entry rather than reading past the buffer, and get_entry_offsets_count()
/// must reflect only the entries actually confirmed in-bounds.
BOOST_AUTO_TEST_CASE(MalformedLastEntryLengthOverrunStaysInBoundsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area({
        {0x0030, 0x04, 1, {0x01}}, // Entry 0: well-formed, Entry Length 6.
        {0x0031, 0x05, 2, {0x02}}  // Entry 1: well-formed for now, Entry Length 6.
    });

    // Corrupt entry 1's own Entry Length byte (first byte of the second
    // entry) to claim a length far larger than any bytes actually present
    // in the structure, without changing the buffer's actual size (and
    // therefore without changing header.length / get_entry_size()).
    // Entry 0 occupies formatted_area[1..6] (index 0 is the count byte),
    // so entry 1 starts at formatted_area index 7.
    const size_t entry1_length_index = 7;
    BOOST_REQUIRE_EQUAL(formatted_area[entry1_length_index], static_cast<uint8_t>(6));
    formatted_area[entry1_length_index] = 0xFF; // Claims 255 bytes - wildly overruns the buffer.

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::AdditionalInformation, formatted_area, {"String one", "String two"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    AdditionalInformationEntry entry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    // The structure still declares 2 entries (the count byte was not
    // touched)...
    BOOST_CHECK_EQUAL(entry.get_entry_count(), static_cast<uint8_t>(2));
    // ...but only entry 0 could be walked safely; entry 1's bogus Entry
    // Length stopped the walk before it, so the cache has only 1 entry.
    BOOST_CHECK_EQUAL(entry.get_entry_offsets_count(), static_cast<size_t>(1));

    // Entry 0 is unaffected and decodes normally.
    BOOST_CHECK_EQUAL(entry.get_entry_length_at(0), static_cast<uint8_t>(6));
    BOOST_CHECK_EQUAL(entry.get_referenced_handle_at(0), static_cast<uint16_t>(0x0030));
    BOOST_CHECK(entry.get_value_bytes_at(0) == (std::vector<uint8_t>{0x01}));

    // Entry 1 is unreachable - every accessor returns its documented safe
    // default rather than reading out of bounds.
    BOOST_CHECK_EQUAL(entry.get_entry_length_at(1), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_referenced_handle_at(1), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_referenced_offset_at(1), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_string_index_at(1), static_cast<uint8_t>(0));
    BOOST_CHECK(entry.get_value_bytes_at(1).empty());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area({});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(AdditionalInformationEntry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 06.0 Task 02's own success criterion:
/// AdditionalInformation resolves through SMBiosEntryFactory::create() to a
/// real AdditionalInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsAdditionalInformationEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area({
        {0x0040, 0x04, 0, {0x01}}
    });

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::AdditionalInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<AdditionalInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
