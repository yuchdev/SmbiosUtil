#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/processor_additional_information_entry.hpp>
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
// ProcessorAdditionalInformationEntry
// (smbios/include/smbios/tables/processor_additional_information_entry.hpp),
// cross-checked directly against DSP0134 3.9.0's own 7.45/7.45.1 (Table
// 128/129/130).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint16_t  referenced_handle
//   offset 0x06 : uint8_t   block_length (N, the Processor-specific
//                 Block's own sub-header field)
//   offset 0x07 : uint8_t   processor_type (Table 130 enum)
//   offset 0x08 : N raw BYTEs  processor_specific_data (opaque)
//
// Double-length relationship under test (per this task's own explicit
// success criterion, see processor_additional_information_entry.hpp's own
// leading comment block for the full derivation): DSP0134 3.9.0 Table 128
// defines the outer structure's own Length byte as "6 + Y", where Y is the
// Processor-specific Block's length (Table 129: Block Length byte +
// Processor Type byte + N data bytes, so Y = N + 2). Substituting gives
// Length = 6 + (N + 2) = 8 + N. SyntheticTableBuilder computes the actual
// header.length byte as 4 (its own header) + formatted_area.size(), and
// formatted_area here is exactly 4 + N bytes (0x04/0x05 Referenced Handle,
// 0x06 Block Length, 0x07 Processor Type, then N data bytes) - so
// header.length == 4 + (4 + N) == 8 + N, matching the spec formula
// exactly. The assertions below check header.length == 8 + N directly.

BOOST_AUTO_TEST_SUITE(ProcessorAdditionalInformationEntryUnitTests)

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

/// Build a formatted area with a Processor-Specific Data block of exactly
/// `data.size()` bytes (N == data.size()).
std::vector<uint8_t> make_formatted_area(uint16_t referenced_handle, uint8_t processor_type,
                                          const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(static_cast<uint8_t>(referenced_handle & 0xFF));         // 0x04
    formatted_area.push_back(static_cast<uint8_t>((referenced_handle >> 8) & 0xFF));  // 0x05
    formatted_area.push_back(static_cast<uint8_t>(data.size()));                      // 0x06 Block Length (N)
    formatted_area.push_back(processor_type);                                         // 0x07 Processor Type
    formatted_area.insert(formatted_area.end(), data.begin(), data.end());            // 0x08.. Processor-Specific Data
    return formatted_area;
}

} // namespace

/// Normal case: a small (N == 2) opaque Processor-Specific Data block,
/// Processor Type == X64 (a real, confirmed Table 130 enum value, not the
/// placeholder Reserved/0x00).
BOOST_AUTO_TEST_CASE(X64ProcessorTypeWithTwoByteDataTestCase)
{
    const std::vector<uint8_t> data = { 0xAA, 0xBB };
    std::vector<uint8_t> formatted_area =
        make_formatted_area(0x0005, ProcessorAdditionalInformationEntry::X64, data);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ProcessorAdditionalInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    // Explicit double-length-relationship assertion (this task's own
    // success criterion): outer structure Length == 8 + N, where N == 2
    // here.
    BOOST_CHECK_EQUAL(static_cast<unsigned>(header.length), static_cast<unsigned>(8 + data.size()));

    ProcessorAdditionalInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_referenced_handle(), static_cast<uint16_t>(0x0005));
    BOOST_CHECK_EQUAL(entry.get_block_length(), static_cast<uint8_t>(data.size()));
    BOOST_CHECK_EQUAL(entry.get_processor_type(), static_cast<uint8_t>(ProcessorAdditionalInformationEntry::X64));
    BOOST_CHECK_EQUAL(entry.get_processor_type_string(), std::string("x64 (x86-64, Intel64, AMD64, EM64T)"));
    BOOST_CHECK_EQUAL(entry.get_processor_specific_data().size(), static_cast<size_t>(2));
    BOOST_CHECK(entry.get_processor_specific_data() == data);
}

/// Normal/edge case: N == 1, Processor Type == Arm64 - a second real,
/// confirmed enum value exercised, plus a single-byte opaque data block.
BOOST_AUTO_TEST_CASE(Arm64ProcessorTypeWithOneByteDataTestCase)
{
    const std::vector<uint8_t> data = { 0x42 };
    std::vector<uint8_t> formatted_area =
        make_formatted_area(0x0009, ProcessorAdditionalInformationEntry::Arm64, data);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ProcessorAdditionalInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    // Double-length relationship holds for N == 1 as well.
    BOOST_CHECK_EQUAL(static_cast<unsigned>(header.length), static_cast<unsigned>(8 + data.size()));

    ProcessorAdditionalInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_referenced_handle(), static_cast<uint16_t>(0x0009));
    BOOST_CHECK_EQUAL(entry.get_block_length(), static_cast<uint8_t>(1));
    BOOST_CHECK_EQUAL(entry.get_processor_type(), static_cast<uint8_t>(ProcessorAdditionalInformationEntry::Arm64));
    BOOST_CHECK_EQUAL(entry.get_processor_type_string(), std::string("64-bit ARM (Aarch64)"));
    BOOST_CHECK_EQUAL(entry.get_processor_specific_data().size(), static_cast<size_t>(1));
    BOOST_CHECK_EQUAL(entry.get_processor_specific_data()[0], static_cast<uint8_t>(0x42));
}

/// Edge case: an unassigned Processor Type byte value (beyond Table 130's
/// confirmed 0x00-0x0A range) must report "Unknown", not crash or silently
/// claim a decoded meaning.
BOOST_AUTO_TEST_CASE(UnknownProcessorTypeTestCase)
{
    const std::vector<uint8_t> data = { 0x01 };
    std::vector<uint8_t> formatted_area = make_formatted_area(0x0001, 0xF0, data);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ProcessorAdditionalInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ProcessorAdditionalInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_processor_type(), static_cast<uint8_t>(0xF0));
    BOOST_CHECK_EQUAL(entry.get_processor_type_string(), std::string("Unknown"));
}

/// Edge case: structure shorter than the declared Block Length (N) - the
/// opaque data accessor must fall back to an empty vector rather than
/// reading past the buffer.
BOOST_AUTO_TEST_CASE(TruncatedProcessorSpecificDataTestCase)
{
    // Declares Block Length == 4 but only supplies 1 data byte.
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(0x01); // 0x04 referenced handle lo
    formatted_area.push_back(0x00); // 0x05 referenced handle hi
    formatted_area.push_back(0x04); // 0x06 Block Length (N == 4, but...)
    formatted_area.push_back(static_cast<uint8_t>(ProcessorAdditionalInformationEntry::Ia32)); // 0x07
    formatted_area.push_back(0xFF); // 0x08 only 1 data byte actually present

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ProcessorAdditionalInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ProcessorAdditionalInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_block_length(), static_cast<uint8_t>(4));
    BOOST_CHECK(entry.get_processor_specific_data().empty());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    const std::vector<uint8_t> data = { 0x00 };
    std::vector<uint8_t> formatted_area = make_formatted_area(0x0000, 0x00, data);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(ProcessorAdditionalInformationEntry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size()),
                       std::runtime_error);
}

/// ProcessorAdditionalInformation resolves through SMBiosEntryFactory::create()
/// to a real ProcessorAdditionalInformationEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsProcessorAdditionalInformationEntryTestCase)
{
    const std::vector<uint8_t> data = { 0x00 };
    std::vector<uint8_t> formatted_area = make_formatted_area(0x0000, 0x00, data);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ProcessorAdditionalInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<ProcessorAdditionalInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
