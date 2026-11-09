#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/management_controller_host_interface_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive
// ManagementControllerHostInterfaceEntry
// (smbios/include/smbios/tables/management_controller_host_interface_entry.hpp),
// cross-checked directly against DSP0134 3.9.0's own 7.43/7.43.1/7.43.2
// (Table 122/123/124), including the confirmed Table 123 Protocol Record
// layout redacted-gap resolution (see the header's own doc comment).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04        : uint8_t  interface_type (enum, Table 124)
//   offset 0x05        : uint8_t  interface_specific_data_length (N)
//   offset 0x06..0x06+N-1 : N raw bytes interface_specific_data
//   offset 0x06+N      : uint8_t  protocol_record_count (X)
//   offset 0x07+N      : Protocol Records, each
//                         {uint8_t protocol_type, uint8_t data_length,
//                          data_length raw bytes}

BOOST_AUTO_TEST_SUITE(ManagementControllerHostInterfaceEntryUnitTests)

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

/// One Protocol Record to be serialized by make_formatted_area().
struct RecordSpec {
    uint8_t protocol_type;
    std::vector<uint8_t> data;
};

std::vector<uint8_t> make_formatted_area(uint8_t interface_type,
                                          const std::vector<uint8_t>& interface_specific_data,
                                          const std::vector<RecordSpec>& records)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(interface_type); // 0x04
    formatted_area.push_back(static_cast<uint8_t>(interface_specific_data.size())); // 0x05 N
    for (uint8_t b : interface_specific_data) formatted_area.push_back(b); // 0x06..0x06+N-1

    formatted_area.push_back(static_cast<uint8_t>(records.size())); // 0x06+N X

    for (const RecordSpec& record : records) {
        formatted_area.push_back(record.protocol_type);
        formatted_area.push_back(static_cast<uint8_t>(record.data.size()));
        for (uint8_t b : record.data) formatted_area.push_back(b);
    }

    return formatted_area;
}

} // namespace

/// Edge case: 0 protocol records.
BOOST_AUTO_TEST_CASE(ZeroProtocolRecordsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02 /* MCTP host interface type */, {0xDE, 0xAD, 0xBE, 0xEF}, {});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementControllerHostInterface, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementControllerHostInterfaceEntry entry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interface_type(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_interface_type_string(), std::string("MCTP Host Interface"));
    BOOST_CHECK_EQUAL(entry.get_interface_specific_data_length(), static_cast<uint8_t>(4));
    BOOST_CHECK(entry.get_interface_specific_data() == (std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}));
    BOOST_CHECK_EQUAL(entry.get_protocol_record_count(), static_cast<uint8_t>(0));

    ManagementControllerHostInterfaceEntry::ProtocolRecord record = entry.get_protocol_record_at(0);
    BOOST_CHECK_EQUAL(record.protocol_type, static_cast<uint8_t>(0));
    BOOST_CHECK(record.data.empty());
}

/// Normal case: 2+ protocol records of differing variable lengths,
/// confirming the record-by-record walk correctly finds each record's
/// boundary from the previous record's own length (no fixed stride).
BOOST_AUTO_TEST_CASE(TwoOrMoreProtocolRecordsWalkCorrectlyTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x40 /* Network Host Interface */, {0x01, 0x02},
        {
            {0x02 /* IPMI */, {0xAA}},
            {0x03 /* MCTP */, {0xBB, 0xCC, 0xDD}},
            {0x04 /* Redfish over IP */, {}}
        });

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementControllerHostInterface, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementControllerHostInterfaceEntry entry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interface_type(), static_cast<uint8_t>(0x40));
    BOOST_CHECK_EQUAL(entry.get_interface_type_string(), std::string("Network Host Interface"));
    BOOST_CHECK_EQUAL(entry.get_interface_specific_data_length(), static_cast<uint8_t>(2));
    BOOST_CHECK(entry.get_interface_specific_data() == (std::vector<uint8_t>{0x01, 0x02}));
    BOOST_CHECK_EQUAL(entry.get_protocol_record_count(), static_cast<uint8_t>(3));

    ManagementControllerHostInterfaceEntry::ProtocolRecord record0 = entry.get_protocol_record_at(0);
    BOOST_CHECK_EQUAL(record0.protocol_type, static_cast<uint8_t>(0x02));
    BOOST_CHECK(record0.data == (std::vector<uint8_t>{0xAA}));

    ManagementControllerHostInterfaceEntry::ProtocolRecord record1 = entry.get_protocol_record_at(1);
    BOOST_CHECK_EQUAL(record1.protocol_type, static_cast<uint8_t>(0x03));
    BOOST_CHECK(record1.data == (std::vector<uint8_t>{0xBB, 0xCC, 0xDD}));

    ManagementControllerHostInterfaceEntry::ProtocolRecord record2 = entry.get_protocol_record_at(2);
    BOOST_CHECK_EQUAL(record2.protocol_type, static_cast<uint8_t>(0x04));
    BOOST_CHECK(record2.data.empty());

    // Out-of-range index must not be confused with a real record.
    ManagementControllerHostInterfaceEntry::ProtocolRecord record3 = entry.get_protocol_record_at(3);
    BOOST_CHECK_EQUAL(record3.protocol_type, static_cast<uint8_t>(0));
    BOOST_CHECK(record3.data.empty());
}

/// Invalid-input/bounds-safety case: Number of Protocol Records claims 2,
/// but the buffer is truncated right after the first record's header
/// (Protocol Type + Length byte) with no data bytes and no second record at
/// all. The walk for index 1 must stop cleanly rather than read past
/// get_entry_size().
BOOST_AUTO_TEST_CASE(TruncatedProtocolRecordListStaysInBoundsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, {}, {{0x02, {0x11, 0x22}}});
    // Declare a second record (bumping X to 2) without actually appending
    // its bytes - simulates a malformed/truncated table. Locate the
    // protocol_record_count byte (0x06+N where N==0, i.e. index 2 within
    // formatted_area since formatted_area[0] is offset 0x04) and bump it.
    const size_t count_index_in_formatted_area = 2;
    BOOST_REQUIRE_EQUAL(formatted_area[count_index_in_formatted_area], static_cast<uint8_t>(1));
    formatted_area[count_index_in_formatted_area] = 2;

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementControllerHostInterface, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementControllerHostInterfaceEntry entry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_protocol_record_count(), static_cast<uint8_t>(2));

    // Record 0 is fully present and decodes normally.
    ManagementControllerHostInterfaceEntry::ProtocolRecord record0 = entry.get_protocol_record_at(0);
    BOOST_CHECK_EQUAL(record0.protocol_type, static_cast<uint8_t>(0x02));
    BOOST_CHECK(record0.data == (std::vector<uint8_t>{0x11, 0x22}));

    // Record 1 does not actually exist in the buffer - the walk must detect
    // running off the end and return a default (empty) record rather than
    // reading out of bounds.
    ManagementControllerHostInterfaceEntry::ProtocolRecord record1 = entry.get_protocol_record_at(1);
    BOOST_CHECK_EQUAL(record1.protocol_type, static_cast<uint8_t>(0));
    BOOST_CHECK(record1.data.empty());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(0x02, {}, {});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(ManagementControllerHostInterfaceEntry(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 05.0 Task 03's own success criterion:
/// ManagementControllerHostInterface resolves through
/// SMBiosEntryFactory::create() to a real
/// ManagementControllerHostInterfaceEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsManagementControllerHostInterfaceEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(0x02, {}, {{0x02, {0xAA}}});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementControllerHostInterface, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<ManagementControllerHostInterfaceEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
