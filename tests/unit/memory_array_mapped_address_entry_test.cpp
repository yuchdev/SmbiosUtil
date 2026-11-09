#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/memory_array_mapped_address_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive MemoryArrayMappedAddressEntry (smbios/include/smbios/
// memory_array_mapped_address_entry.h) across the header.length tiers
// named by docs/roadmap/0004-.../02.0-.../03-system-slots-and-memory-
// array.md, cross-checked against c-versions/dmidecode_linux/dmidecode.c's
// case 19 ("Memory Array Mapped Address"):
//   tier 1: header.length < 0x0F   -> every length-guarded getter defaults
//   tier 2: header.length == 0x0F  -> starting/ending address..partition
//                                      width decode; extended fields (need
//                                      length >= 0x1F) unreachable
//   tier 3: header.length == 0x1F  -> extended fields also reachable
//   plus the dedicated without-sentinel/with-sentinel/length-insufficient
//   triple for the 0xFFFFFFFF starting-address sentinel named by this
//   task's own success criterion (note: NOT 0x80000000 - that is
//   PhysicalMemoryArrayEntry's own, different, maximum-capacity sentinel).
//
// Wire layout (see smbios/include/smbios/memory_array_mapped_address_
// entry.h's "@brief 0xNN offset" getter comments):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint32_t starting_address (0xFFFFFFFF sentinel -> extended at 0x0F)
//   offset 0x08 : uint32_t ending_address
//   offset 0x0C : uint16_t memory_array_handle
//   offset 0x0E : uint8_t  partition_width
//   offset 0x0F : uint64_t extended_starting_address
//   offset 0x17 : uint64_t extended_ending_address

BOOST_AUTO_TEST_SUITE(MemoryArrayMappedAddressEntryUnitTests)

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

/// @brief Build a full-length (0x1F) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` (via
/// SyntheticTableBuilder::add_structure(), which computes header.length
/// automatically from the formatted area's size). `starting_address` is
/// overridable (little-endian DWORD) so each test case can drive the exact
/// sentinel branch it needs; `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length, uint32_t starting_address = 0x00001000)
{
    std::vector<uint8_t> full_formatted_area{
        // 0x04 starting_address (little-endian DWORD)
        static_cast<uint8_t>(starting_address & 0xFF),
        static_cast<uint8_t>((starting_address >> 8) & 0xFF),
        static_cast<uint8_t>((starting_address >> 16) & 0xFF),
        static_cast<uint8_t>((starting_address >> 24) & 0xFF),

        // 0x08 ending_address (little-endian DWORD) = 0x00002000
        0x00, 0x20, 0x00, 0x00,

        0x05, 0x00, // 0x0C memory_array_handle = 0x0005
        0x08,       // 0x0E partition_width = 8

        // 0x0F extended_starting_address (little-endian QWORD) = 0x200000000
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,

        // 0x17 extended_ending_address (little-endian QWORD) = 0x300000000
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    };

    BOOST_REQUIRE_EQUAL(full_formatted_area.size(), static_cast<size_t>(0x1F - 4));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), static_cast<size_t>(0x1F));
    BOOST_REQUIRE_GE(static_cast<size_t>(length), static_cast<size_t>(4));

    std::vector<uint8_t> formatted_area(full_formatted_area.begin(),
                                         full_formatted_area.begin() + (length - 4));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryArrayMappedAddress, formatted_area);
    return builder.build();
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x0F means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(MemoryArrayMappedAddressEntryTruncatedBelow0x0FDefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    MemoryArrayMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_memory_array_handle(), static_cast<uint16_t>(0xFFFF));
    BOOST_CHECK_EQUAL(entry.get_partition_width(), static_cast<uint8_t>(0));
}

/// Normal case: header.length == 0x0F - starting/ending address..partition
/// width decode; starting_address is NOT the sentinel here, so the plain
/// DWORD value is returned.
BOOST_AUTO_TEST_CASE(MemoryArrayMappedAddressEntryTier0x0FDecodesBaseFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F);
    DMIHeader header = make_header(buffer);

    MemoryArrayMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0x00001000));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0x00002000));
    BOOST_CHECK_EQUAL(entry.get_memory_array_handle(), static_cast<uint16_t>(0x0005));
    BOOST_CHECK_EQUAL(entry.get_partition_width(), static_cast<uint8_t>(8));
}

/// Starting-address sentinel, case without: a plain (non-sentinel) DWORD
/// value is used as-is for both starting and ending address, even when the
/// buffer is long enough for the extended fields.
BOOST_AUTO_TEST_CASE(StartingAddressUsesPlainDwordWhenNotSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1F, 0x00001000 /* not the sentinel */);
    DMIHeader header = make_header(buffer);

    MemoryArrayMappedAddressEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0x00001000));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0x00002000));
}

/// Starting-address sentinel, case with: 0xFFFFFFFF at the legacy DWORD
/// plus a long-enough buffer (length >= 0x1F) consults the extended QWORD
/// fields at 0x0F/0x17 instead, for BOTH starting and ending address (the
/// reference checks the starting-address sentinel only).
BOOST_AUTO_TEST_CASE(StartingAddressSentinelConsultsExtendedFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1F, 0xFFFFFFFF);
    DMIHeader header = make_header(buffer);

    MemoryArrayMappedAddressEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0x200000000ULL));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0x300000000ULL));
}

/// Regression/edge case: the 0xFFFFFFFF sentinel is present, but the
/// buffer is one byte too short for the extended fields (length == 0x1E,
/// needs 0x1F) - genuinely unknown, not "fall back to the sentinel value
/// itself" and not read past the formatted area.
BOOST_AUTO_TEST_CASE(StartingAddressSentinelFallsBackToUnknownWhenLengthInsufficientTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1E, 0xFFFFFFFF);
    DMIHeader header = make_header(buffer);

    MemoryArrayMappedAddressEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_starting_address_string(), std::string("Unknown"));
    BOOST_CHECK_EQUAL(entry.get_ending_address_string(), std::string("Unknown"));
}

/// Invalid-input case: constructing with a non-MemoryArrayMappedAddress
/// header type must throw, not silently misparse.
BOOST_AUTO_TEST_CASE(WrongHeaderTypeThrowsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F);
    buffer[0] = SMBios::BIOSInformation; // deliberately wrong type
    DMIHeader header = make_header(buffer);

    BOOST_CHECK_THROW(MemoryArrayMappedAddressEntry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0004 Story 02.0's own success criterion:
/// MemoryArrayMappedAddress resolves through SMBiosEntryFactory::create()
/// to a real MemoryArrayMappedAddressEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsMemoryArrayMappedAddressEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1F);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<MemoryArrayMappedAddressEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
