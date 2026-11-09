#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.h>
#include <libsmbios/tables/memory_device_mapped_address_entry.h>
#include <libsmbios/smbios_entry_factory.h>
#include <libsmbios/abstract_smbios_entry.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive MemoryDeviceMappedAddressEntry (smbios/include/smbios/
// tables/memory_device_mapped_address_entry.h) across the header.length
// tiers named by docs/roadmap/0010-smbios-structure-coverage-expansion/
// 03.0-event-log-and-memory-error-mapping/03-memory-device-mapped-
// address.md, cross-checked against DSP0134 3.9.0 Table 86 and
// c-versions/dmidecode_linux/dmidecode.c's case 20 ("Memory Device Mapped
// Address"):
//   tier 1: header.length < 0x13   -> every length-guarded getter defaults
//   tier 2: header.length == 0x13  -> 2.1-shaped: legacy addresses only,
//                                      normalized from kibibytes to bytes
//   tier 3: header.length == 0x23  -> 2.7+-shaped: extended fields also
//                                      reachable
//   plus the dedicated without-sentinel/with-sentinel pair for the
//   0xFFFFFFFF starting-address sentinel, and dedicated cases for
//   Partition Row Position's 0/0xFF/normal and Interleave Position's
//   0/0xFF/normal values (two DIFFERENT semantics for the same byte 0).
//
// Wire layout (see the header's "@brief 0xNN offset" getter comments):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint32_t starting_address (kibibytes; 0xFFFFFFFF sentinel -> extended at 0x13)
//   offset 0x08 : uint32_t ending_address (kibibytes)
//   offset 0x0C : uint16_t memory_device_handle
//   offset 0x0E : uint16_t memory_array_mapped_address_handle
//   offset 0x10 : uint8_t  partition_row_position
//   offset 0x11 : uint8_t  interleave_position
//   offset 0x12 : uint8_t  interleaved_data_depth (not exposed by this class)
//   offset 0x13 : uint64_t extended_starting_address (bytes)
//   offset 0x1B : uint64_t extended_ending_address (bytes)

BOOST_AUTO_TEST_SUITE(MemoryDeviceMappedAddressEntryUnitTests)

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

/// @brief Build a full-length (0x23) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` (via
/// SyntheticTableBuilder::add_structure(), which computes header.length
/// automatically from the formatted area's size). `starting_address`,
/// `partition_row_position`, and `interleave_position` are overridable so
/// each test case can drive the exact branch it needs; `length` must be
/// >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length,
                                   uint32_t starting_address = 0x00000010,
                                   uint8_t partition_row_position = 0x01,
                                   uint8_t interleave_position = 0x01)
{
    std::vector<uint8_t> full_formatted_area{
        // 0x04 starting_address (little-endian DWORD, kibibytes)
        static_cast<uint8_t>(starting_address & 0xFF),
        static_cast<uint8_t>((starting_address >> 8) & 0xFF),
        static_cast<uint8_t>((starting_address >> 16) & 0xFF),
        static_cast<uint8_t>((starting_address >> 24) & 0xFF),

        // 0x08 ending_address (little-endian DWORD, kibibytes) = 0x00000020
        0x20, 0x00, 0x00, 0x00,

        0x05, 0x00, // 0x0C memory_device_handle = 0x0005
        0x06, 0x00, // 0x0E memory_array_mapped_address_handle = 0x0006

        partition_row_position, // 0x10
        interleave_position,    // 0x11
        0x00,                   // 0x12 interleaved_data_depth (not exposed)

        // 0x13 extended_starting_address (little-endian QWORD, bytes) = 0x200000000
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,

        // 0x1B extended_ending_address (little-endian QWORD, bytes) = 0x300000000
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    };

    BOOST_REQUIRE_EQUAL(full_formatted_area.size(), static_cast<size_t>(0x23 - 4));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), static_cast<size_t>(0x23));
    BOOST_REQUIRE_GE(static_cast<size_t>(length), static_cast<size_t>(4));

    std::vector<uint8_t> formatted_area(full_formatted_area.begin(),
                                         full_formatted_area.begin() + (length - 4));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryDeviceMappedAddress, formatted_area);
    return builder.build();
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x13 means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(MemoryDeviceMappedAddressEntryTruncatedBelow0x13DefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_memory_device_handle(), static_cast<uint16_t>(0xFFFF));
    BOOST_CHECK_EQUAL(entry.get_memory_array_mapped_address_handle(), static_cast<uint16_t>(0xFFFF));
    BOOST_CHECK_EQUAL(entry.get_partition_row_position(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK_EQUAL(entry.get_interleave_position(), static_cast<uint8_t>(0xFF));
}

/// Normal case: a 2.1-shaped buffer (header.length == 0x13, no extended
/// fields). The legacy DWORD values (kibibytes) must be used directly and
/// converted to bytes: starting_address = 0x10 KiB = 0x10 * 1024 bytes =
/// 0x4000 bytes; ending_address = 0x20 KiB = 0x8000 bytes. This is the
/// explicit value-equality assertion for the kibibyte-to-byte conversion
/// arithmetic named by this task's own success criterion.
BOOST_AUTO_TEST_CASE(Shape21LegacyAddressesConvertedToBytesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13, 0x00000010 /* 0x10 KiB */);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0x10) * 1024ULL);
    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0x4000));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0x20) * 1024ULL);
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0x8000));
    BOOST_CHECK_EQUAL(entry.get_memory_device_handle(), static_cast<uint16_t>(0x0005));
    BOOST_CHECK_EQUAL(entry.get_memory_array_mapped_address_handle(), static_cast<uint16_t>(0x0006));
}

/// Starting-address sentinel, case without: a 2.7+-shaped buffer with a
/// REAL (non-sentinel) legacy address present must still use the legacy
/// kibibyte value (converted to bytes), NOT the extended fields, even
/// though the extended fields are present in the buffer - per spec they'd
/// be zero/unused in this case (this synthetic buffer deliberately puts
/// non-zero values there anyway, to prove they are genuinely ignored
/// rather than coincidentally zero).
BOOST_AUTO_TEST_CASE(Shape27PlusUsesLegacyAddressWhenNotSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x23, 0x00000010 /* not the sentinel */);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0x4000));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0x8000));
}

/// Starting-address sentinel, case with: a 2.7+-shaped buffer with the
/// legacy sentinel 0xFFFFFFFF set on both address fields reads through to
/// the extended QWORD fields at 0x13/0x1B instead (already in bytes, no
/// further conversion).
BOOST_AUTO_TEST_CASE(Shape27PlusSentinelConsultsExtendedFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x23, 0xFFFFFFFF);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0x200000000ULL));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0x300000000ULL));
}

/// Regression/edge case: the 0xFFFFFFFF sentinel is present, but the
/// buffer is one byte too short for the extended fields (length == 0x22,
/// needs 0x23) - genuinely unknown, not "fall back to the sentinel value
/// itself" and not read past the formatted area.
BOOST_AUTO_TEST_CASE(StartingAddressSentinelFallsBackToUnknownWhenLengthInsufficientTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x22, 0xFFFFFFFF);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_ending_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_starting_address_string(), std::string("Unknown"));
    BOOST_CHECK_EQUAL(entry.get_ending_address_string(), std::string("Unknown"));
}

/// Partition Row Position: value 0 is reserved per spec - a different
/// semantic than Interleave Position's value 0 ("non-interleaved") despite
/// the same byte value.
BOOST_AUTO_TEST_CASE(PartitionRowPositionZeroIsReservedTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13, 0x00000010, /*partition_row_position*/ 0x00);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_partition_row_position(), static_cast<uint8_t>(0x00));
    BOOST_CHECK_EQUAL(entry.get_partition_row_position_string(), std::string("Reserved"));
}

/// Partition Row Position: 0xFF means unknown.
BOOST_AUTO_TEST_CASE(PartitionRowPositionUnknownTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13, 0x00000010, /*partition_row_position*/ 0xFF);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_partition_row_position(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK_EQUAL(entry.get_partition_row_position_string(), std::string("Unknown"));
}

/// Partition Row Position: an ordinary, valid, non-sentinel value.
BOOST_AUTO_TEST_CASE(PartitionRowPositionNormalValueTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13, 0x00000010, /*partition_row_position*/ 0x02);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_partition_row_position(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_partition_row_position_string(), std::string("2"));
}

/// Interleave Position: value 0 means "non-interleaved" - a valid,
/// meaningful value, NOT the same semantic as Partition Row Position's
/// reserved 0 despite sharing the same byte value.
BOOST_AUTO_TEST_CASE(InterleavePositionZeroIsNonInterleavedTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13, 0x00000010, 0x01, /*interleave_position*/ 0x00);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interleave_position(), static_cast<uint8_t>(0x00));
    BOOST_CHECK_EQUAL(entry.get_interleave_position_string(), std::string("Non-interleaved"));
}

/// Interleave Position: 0xFF means unknown.
BOOST_AUTO_TEST_CASE(InterleavePositionUnknownTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13, 0x00000010, 0x01, /*interleave_position*/ 0xFF);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interleave_position(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK_EQUAL(entry.get_interleave_position_string(), std::string("Unknown"));
}

/// Interleave Position: an ordinary, valid, non-sentinel value.
BOOST_AUTO_TEST_CASE(InterleavePositionNormalValueTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13, 0x00000010, 0x01, /*interleave_position*/ 0x03);
    DMIHeader header = make_header(buffer);

    MemoryDeviceMappedAddressEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interleave_position(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry.get_interleave_position_string(), std::string("3"));
}

/// Invalid-input case: constructing with a non-MemoryDeviceMappedAddress
/// header type must throw, not silently misparse.
BOOST_AUTO_TEST_CASE(WrongHeaderTypeThrowsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13);
    buffer[0] = SMBios::BIOSInformation; // deliberately wrong type
    DMIHeader header = make_header(buffer);

    BOOST_CHECK_THROW(MemoryDeviceMappedAddressEntry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size()), std::runtime_error);
}

/// This story's own success criterion: MemoryDeviceMappedAddress resolves
/// through SMBiosEntryFactory::create() to a real
/// MemoryDeviceMappedAddressEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsMemoryDeviceMappedAddressEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x23);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<MemoryDeviceMappedAddressEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
