#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/tables/system_event_log_entry.h>
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
// smbios::test::SyntheticTableBuilder) drive SystemEventLogEntry
// (smbios/include/smbios/tables/system_event_log_entry.h), cross-checked
// directly against DSP0134 3.9.0's own 7.16 (Table 60).
//
// Wire layout (2.1+ shape):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint16_t log_area_length
//   offset 0x06 : uint16_t log_header_start_offset
//   offset 0x08 : uint16_t log_data_start_offset
//   offset 0x0A : uint8_t  access_method
//   offset 0x0B : uint8_t  log_status
//   offset 0x0C : uint32_t log_change_token
//   offset 0x10 : uint32_t access_method_address
//   offset 0x14 : uint8_t  log_header_format (2.1+)
//   offset 0x15 : uint8_t  supported_descriptor_count (x) (2.1+)
//   offset 0x16 : uint8_t  descriptor_length (y) (2.1+)
//   offset 0x17 : x*y bytes of descriptors (2.1+)

BOOST_AUTO_TEST_SUITE(SystemEventLogEntryUnitTests)

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

/// Build a version-2.0-shaped formatted area (Length == 0x14, through
/// Access Method Address only, no 2.1+ fields at all).
std::vector<uint8_t> make_v20_formatted_area(uint16_t log_area_length, uint16_t log_header_start_offset,
                                              uint16_t log_data_start_offset, uint8_t access_method,
                                              uint8_t log_status, uint32_t log_change_token,
                                              uint32_t access_method_address)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(static_cast<uint8_t>(log_area_length & 0xFF));         // 0x04
    formatted_area.push_back(static_cast<uint8_t>(log_area_length >> 8));
    formatted_area.push_back(static_cast<uint8_t>(log_header_start_offset & 0xFF)); // 0x06
    formatted_area.push_back(static_cast<uint8_t>(log_header_start_offset >> 8));
    formatted_area.push_back(static_cast<uint8_t>(log_data_start_offset & 0xFF));   // 0x08
    formatted_area.push_back(static_cast<uint8_t>(log_data_start_offset >> 8));
    formatted_area.push_back(access_method);                                        // 0x0A
    formatted_area.push_back(log_status);                                           // 0x0B
    for (int i = 0; i < 4; ++i) formatted_area.push_back(static_cast<uint8_t>(log_change_token >> (8 * i))); // 0x0C
    for (int i = 0; i < 4; ++i) formatted_area.push_back(static_cast<uint8_t>(access_method_address >> (8 * i))); // 0x10

    return formatted_area;
}

/// Build a version-2.1+-shaped formatted area, appending the 2.1+ fields
/// plus `count` descriptors of `descriptor_length` bytes each (values taken
/// from `descriptors`, each padded/truncated to descriptor_length bytes).
std::vector<uint8_t> make_v21_formatted_area(uint8_t log_header_format, uint8_t count, uint8_t descriptor_length,
                                              const std::vector<uint32_t>& descriptors)
{
    std::vector<uint8_t> formatted_area = make_v20_formatted_area(
        /*log_area_length*/0x1000, /*log_header_start_offset*/0x0000, /*log_data_start_offset*/0x0010,
        /*access_method*/SystemEventLogEntry::MemoryMappedPhysical32Bit, /*log_status*/0x01,
        /*log_change_token*/0x12345678, /*access_method_address*/0xDEADBEEF);

    formatted_area.push_back(log_header_format); // 0x14
    formatted_area.push_back(count);             // 0x15
    formatted_area.push_back(descriptor_length); // 0x16

    for (uint32_t descriptor : descriptors) {
        for (uint8_t byte_index = 0; byte_index < descriptor_length; ++byte_index) {
            formatted_area.push_back(static_cast<uint8_t>(descriptor >> (8 * byte_index)));
        }
    }

    return formatted_area;
}

} // namespace

/// Normal case: a version-2.0-shaped structure (Length == 0x14h per Table
/// 60's own "The Length is 14h for version 2.0 implementations" note) -
/// confirms every fixed-header field decodes correctly and that the 2.1+
/// accessors correctly report "not available" rather than reading garbage.
BOOST_AUTO_TEST_CASE(Version20StructureHasNoDescriptorFieldsTestCase)
{
    std::vector<uint8_t> formatted_area = make_v20_formatted_area(
        /*log_area_length*/0x0400, /*log_header_start_offset*/0x0000, /*log_data_start_offset*/0x0010,
        /*access_method*/SystemEventLogEntry::MemoryMappedPhysical32Bit, /*log_status*/0x03,
        /*log_change_token*/0xAABBCCDD, /*access_method_address*/0xFEEDFACE);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x14));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemEventLog, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemEventLogEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_log_area_length(), static_cast<uint16_t>(0x0400));
    BOOST_CHECK_EQUAL(entry.get_log_header_start_offset(), static_cast<uint16_t>(0x0000));
    BOOST_CHECK_EQUAL(entry.get_log_data_start_offset(), static_cast<uint16_t>(0x0010));
    BOOST_CHECK_EQUAL(entry.get_access_method(), static_cast<uint8_t>(SystemEventLogEntry::MemoryMappedPhysical32Bit));
    BOOST_CHECK_EQUAL(entry.get_access_method_string(), std::string("Memory-mapped physical 32-bit address"));
    BOOST_CHECK_EQUAL(entry.get_log_status(), static_cast<uint8_t>(0x03));
    BOOST_CHECK(entry.is_log_area_valid());
    BOOST_CHECK(entry.is_log_area_full());
    BOOST_CHECK_EQUAL(entry.get_log_change_token(), static_cast<uint32_t>(0xAABBCCDD));
    BOOST_CHECK_EQUAL(entry.get_access_method_address(), static_cast<uint32_t>(0xFEEDFACE));

    // 2.1+ fields: not present on a genuine 2.0 structure (Length == 0x14).
    BOOST_CHECK_EQUAL(entry.get_log_header_format(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_supported_descriptor_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_descriptor_length(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_log_type_descriptor_at(0), static_cast<uint32_t>(0));
}

/// Edge case: a version-2.1+ structure whose own "Number of Supported Log
/// Type Descriptors" (x) is 0 - per Table 60's own note, "If the value is
/// 0, the list that starts at offset 17h is not present." Confirms this is
/// treated as the normal, common case, not an error.
BOOST_AUTO_TEST_CASE(Version21WithZeroDescriptorsIsNotAnErrorTestCase)
{
    std::vector<uint8_t> formatted_area = make_v21_formatted_area(
        /*log_header_format*/0x01, /*count*/0, /*descriptor_length*/2, {});
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x17));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemEventLog, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemEventLogEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_log_header_format(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_supported_descriptor_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_descriptor_length(), static_cast<uint8_t>(2));

    // Out-of-bounds (there are no descriptors) must report 0, not read past
    // the buffer.
    BOOST_CHECK_EQUAL(entry.get_log_type_descriptor_at(0), static_cast<uint32_t>(0));
}

/// Normal case: a version-2.1+ structure with 2 descriptors, each read via
/// the actual get_descriptor_length() ("y") value rather than a hardcoded
/// 2, confirming the trailing-list offset arithmetic uses the read value.
BOOST_AUTO_TEST_CASE(Version21WithTwoDescriptorsTestCase)
{
    std::vector<uint8_t> formatted_area = make_v21_formatted_area(
        /*log_header_format*/0x01, /*count*/2, /*descriptor_length*/2,
        {/*Log Type=0x01,Variable Data Format=0x00 -> little-endian 0x0001*/0x0001,
         /*Log Type=0x05,Variable Data Format=0x02 -> little-endian 0x0205*/0x0205});
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x17 + 2 * 2));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemEventLog, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemEventLogEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_supported_descriptor_count(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_descriptor_length(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_log_type_descriptor_at(0), static_cast<uint32_t>(0x0001));
    BOOST_CHECK_EQUAL(entry.get_log_type_descriptor_at(1), static_cast<uint32_t>(0x0205));

    // Out of range: beyond the 2 real descriptors, must report 0, never
    // read out of bounds.
    BOOST_CHECK_EQUAL(entry.get_log_type_descriptor_at(2), static_cast<uint32_t>(0));
}

/// Invalid-input/version guard: a 2.1+-shaped buffer (long enough,
/// descriptor fields physically present) but reported under a pre-2.1
/// SMBiosVersion must report the 2.1+ accessors as unavailable (0), proving
/// the guard is driven by the *version*, not merely by buffer length.
BOOST_AUTO_TEST_CASE(PreVersion21GuardsDescriptorFieldsEvenWhenBufferIsLongEnoughTestCase)
{
    std::vector<uint8_t> formatted_area = make_v21_formatted_area(
        /*log_header_format*/0x01, /*count*/1, /*descriptor_length*/2, {0x0001});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemEventLog, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemEventLogEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_log_header_format(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_supported_descriptor_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_descriptor_length(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_log_type_descriptor_at(0), static_cast<uint32_t>(0));

    // Fields below the version gate still decode normally.
    BOOST_CHECK_EQUAL(entry.get_log_change_token(), static_cast<uint32_t>(0x12345678));
}

/// Milestone 0010 Story 03.0 Task 01's own success criterion: SystemEventLog
/// resolves through SMBiosEntryFactory::create() to a real
/// SystemEventLogEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemEventLogEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_v20_formatted_area(
        /*log_area_length*/0x0400, /*log_header_start_offset*/0x0000, /*log_data_start_offset*/0x0010,
        /*access_method*/SystemEventLogEntry::OneIndexOneDataPort8Bit, /*log_status*/0x01,
        /*log_change_token*/0x00000000, /*access_method_address*/0x00000060);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemEventLog, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemEventLogEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
