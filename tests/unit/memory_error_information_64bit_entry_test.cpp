#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/memory_error_information_64bit_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive MemoryErrorInformation64BitEntry
// (smbios/include/smbios/tables/memory_error_information_64bit_entry.hpp),
// cross-checked directly against DSP0134 3.9.0's own 7.34 (Table 106) and
// its (shared) 7.19.1/7.19.2/7.19.3 enum tables.
//
// Wire layout:
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  error_type
//   offset 0x05 : uint8_t  error_granularity
//   offset 0x06 : uint8_t  error_operation
//   offset 0x07 : uint32_t vendor_syndrome
//   offset 0x0B : uint64_t memory_array_error_address
//   offset 0x13 : uint64_t device_error_address
//   offset 0x1B : uint32_t error_resolution (DWORD, NOT widened - see
//                 class header's note on Table 106's width asymmetry)

BOOST_AUTO_TEST_SUITE(MemoryErrorInformation64BitEntryUnitTests)

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

std::vector<uint8_t> make_formatted_area(uint8_t error_type, uint8_t error_granularity, uint8_t error_operation,
                                          uint32_t vendor_syndrome, uint64_t memory_array_error_address,
                                          uint64_t device_error_address, uint32_t error_resolution)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(error_type);         // 0x04
    formatted_area.push_back(error_granularity);  // 0x05
    formatted_area.push_back(error_operation);    // 0x06
    for (int i = 0; i < 4; ++i) formatted_area.push_back(static_cast<uint8_t>(vendor_syndrome >> (8 * i)));             // 0x07
    for (int i = 0; i < 8; ++i) formatted_area.push_back(static_cast<uint8_t>(memory_array_error_address >> (8 * i))); // 0x0B
    for (int i = 0; i < 8; ++i) formatted_area.push_back(static_cast<uint8_t>(device_error_address >> (8 * i)));       // 0x13
    for (int i = 0; i < 4; ++i) formatted_area.push_back(static_cast<uint8_t>(error_resolution >> (8 * i)));           // 0x1B

    return formatted_area;
}

std::unique_ptr<MemoryErrorInformation64BitEntry> build_entry(const std::vector<uint8_t>& formatted_area,
                                                                std::vector<uint8_t>& buffer_out)
{
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryErrorInformation64Bit, formatted_area);
    buffer_out = builder.build();

    DMIHeader header = make_header(buffer_out);
    return std::make_unique<MemoryErrorInformation64BitEntry>(header, SMBiosVersion{2, 3}, buffer_out.data() + buffer_out.size());
}

} // namespace

/// Normal case: a fully-populated structure (Table 106 length 0x1F) decodes
/// every fixed field correctly, including both 64-bit address fields.
BOOST_AUTO_TEST_CASE(FullyPopulatedStructureTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        /*error_type*/0x06, /*error_granularity*/0x03, /*error_operation*/0x03,
        /*vendor_syndrome*/0x11223344, /*memory_array_error_address*/0x0000000123456789ULL,
        /*device_error_address*/0x0000000100000000ULL, /*error_resolution*/0x00000010);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x1F));

    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);

    BOOST_CHECK_EQUAL(entry->get_error_type(), static_cast<uint8_t>(0x06));
    BOOST_CHECK_EQUAL(entry->get_error_type_string(), std::string("Single-bit error"));
    BOOST_CHECK_EQUAL(entry->get_error_granularity(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry->get_error_granularity_string(), std::string("Device level"));
    BOOST_CHECK_EQUAL(entry->get_error_operation(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry->get_error_operation_string(), std::string("Read"));
    BOOST_CHECK_EQUAL(entry->get_vendor_syndrome(), static_cast<uint32_t>(0x11223344));
    BOOST_CHECK(entry->is_vendor_syndrome_known());
    BOOST_CHECK_EQUAL(entry->get_memory_array_error_address(), static_cast<uint64_t>(0x0000000123456789ULL));
    BOOST_CHECK(entry->is_memory_array_error_address_known());
    BOOST_CHECK_EQUAL(entry->get_device_error_address(), static_cast<uint64_t>(0x0000000100000000ULL));
    BOOST_CHECK(entry->is_device_error_address_known());
    BOOST_CHECK_EQUAL(entry->get_error_resolution(), static_cast<uint32_t>(0x00000010));
    BOOST_CHECK(entry->is_error_resolution_known());
}

/// Every Error Type enum value (7.19.1, Table 82, shared with the 32-bit
/// structure) resolves to its own name.
BOOST_AUTO_TEST_CASE(ErrorTypeEnumValuesTestCase)
{
    static const std::pair<uint8_t, std::string> kCases[] = {
        {0x01, "Other"}, {0x02, "Unknown"}, {0x03, "OK"}, {0x04, "Bad read"},
        {0x05, "Parity error"}, {0x06, "Single-bit error"}, {0x07, "Double-bit error"},
        {0x08, "Multi-bit error"}, {0x09, "Nibble error"}, {0x0A, "Checksum error"},
        {0x0B, "CRC error"}, {0x0C, "Corrected single-bit error"}, {0x0D, "Corrected error"},
        {0x0E, "Uncorrectable error"}
    };

    for (const auto& test_case : kCases) {
        std::vector<uint8_t> formatted_area = make_formatted_area(
            test_case.first, 0x02, 0x02, 0x00000000, 0x8000000000000000ULL, 0x8000000000000000ULL, 0x80000000);
        std::vector<uint8_t> buffer;
        std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
        BOOST_CHECK_EQUAL(entry->get_error_type(), test_case.first);
        BOOST_CHECK_EQUAL(entry->get_error_type_string(), test_case.second);
    }
}

/// Every Error Granularity enum value (7.19.2, Table 83) resolves to its
/// own name.
BOOST_AUTO_TEST_CASE(ErrorGranularityEnumValuesTestCase)
{
    static const std::pair<uint8_t, std::string> kCases[] = {
        {0x01, "Other"}, {0x02, "Unknown"}, {0x03, "Device level"}, {0x04, "Memory partition level"}
    };

    for (const auto& test_case : kCases) {
        std::vector<uint8_t> formatted_area = make_formatted_area(
            0x02, test_case.first, 0x02, 0x00000000, 0x8000000000000000ULL, 0x8000000000000000ULL, 0x80000000);
        std::vector<uint8_t> buffer;
        std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
        BOOST_CHECK_EQUAL(entry->get_error_granularity(), test_case.first);
        BOOST_CHECK_EQUAL(entry->get_error_granularity_string(), test_case.second);
    }
}

/// Every Error Operation enum value (7.19.3, Table 84) resolves to its own
/// name.
BOOST_AUTO_TEST_CASE(ErrorOperationEnumValuesTestCase)
{
    static const std::pair<uint8_t, std::string> kCases[] = {
        {0x01, "Other"}, {0x02, "Unknown"}, {0x03, "Read"}, {0x04, "Write"}, {0x05, "Partial write"}
    };

    for (const auto& test_case : kCases) {
        std::vector<uint8_t> formatted_area = make_formatted_area(
            0x02, 0x02, test_case.first, 0x00000000, 0x8000000000000000ULL, 0x8000000000000000ULL, 0x80000000);
        std::vector<uint8_t> buffer;
        std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
        BOOST_CHECK_EQUAL(entry->get_error_operation(), test_case.first);
        BOOST_CHECK_EQUAL(entry->get_error_operation_string(), test_case.second);
    }
}

/// Vendor Syndrome's "unknown" sentinel is all-zero - a known, non-zero
/// value must report as known.
BOOST_AUTO_TEST_CASE(VendorSyndromeKnownValueTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, /*vendor_syndrome*/0xDEADBEEF, 0x8000000000000000ULL, 0x8000000000000000ULL, 0x80000000);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_vendor_syndrome(), static_cast<uint32_t>(0xDEADBEEF));
    BOOST_CHECK(entry->is_vendor_syndrome_known());
}

/// Vendor Syndrome's all-zero sentinel must report as unknown.
BOOST_AUTO_TEST_CASE(VendorSyndromeUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, /*vendor_syndrome*/0x00000000, 0x8000000000000000ULL, 0x8000000000000000ULL, 0x80000000);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_vendor_syndrome(), static_cast<uint32_t>(0x00000000));
    BOOST_CHECK(!entry->is_vendor_syndrome_known());
}

/// Memory Array Error Address: a known value above 4GiB (the whole reason
/// this 64-bit structure exists) must report as known.
BOOST_AUTO_TEST_CASE(MemoryArrayErrorAddressKnownValueTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, 0x00000000, /*memory_array_error_address*/0x0000000500000000ULL,
        0x8000000000000000ULL, 0x80000000);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_memory_array_error_address(), static_cast<uint64_t>(0x0000000500000000ULL));
    BOOST_CHECK(entry->is_memory_array_error_address_known());
}

/// Memory Array Error Address: the `8000 0000 0000 0000h` sentinel must
/// report as unknown.
BOOST_AUTO_TEST_CASE(MemoryArrayErrorAddressUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, 0x00000000, /*memory_array_error_address*/0x8000000000000000ULL,
        0x8000000000000000ULL, 0x80000000);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_memory_array_error_address(), static_cast<uint64_t>(0x8000000000000000ULL));
    BOOST_CHECK(!entry->is_memory_array_error_address_known());
}

/// Device Error Address: a known value above 4GiB must report as known.
BOOST_AUTO_TEST_CASE(DeviceErrorAddressKnownValueTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, 0x00000000, 0x8000000000000000ULL,
        /*device_error_address*/0x0000000200000000ULL, 0x80000000);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_device_error_address(), static_cast<uint64_t>(0x0000000200000000ULL));
    BOOST_CHECK(entry->is_device_error_address_known());
}

/// Device Error Address: the `8000 0000 0000 0000h` sentinel must report as
/// unknown.
BOOST_AUTO_TEST_CASE(DeviceErrorAddressUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, 0x00000000, 0x8000000000000000ULL,
        /*device_error_address*/0x8000000000000000ULL, 0x80000000);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_device_error_address(), static_cast<uint64_t>(0x8000000000000000ULL));
    BOOST_CHECK(!entry->is_device_error_address_known());
}

/// Error Resolution: NOT widened to QWORD per Table 106 - a known DWORD
/// value must report as known.
BOOST_AUTO_TEST_CASE(ErrorResolutionKnownValueTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, 0x00000000, 0x8000000000000000ULL, 0x8000000000000000ULL,
        /*error_resolution*/0x00000040);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_error_resolution(), static_cast<uint32_t>(0x00000040));
    BOOST_CHECK(entry->is_error_resolution_known());
}

/// Error Resolution: the DWORD `8000 0000h` sentinel (NOT the QWORD
/// sentinel used by the two address fields) must report as unknown - the
/// key assertion proving the width asymmetry is actually implemented, not
/// just documented.
BOOST_AUTO_TEST_CASE(ErrorResolutionUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x02, 0x02, 0x02, 0x00000000, 0x8000000000000000ULL, 0x8000000000000000ULL,
        /*error_resolution*/0x80000000);
    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);
    BOOST_CHECK_EQUAL(entry->get_error_resolution(), static_cast<uint32_t>(0x80000000));
    BOOST_CHECK(!entry->is_error_resolution_known());
}

/// Invalid-input/edge case: a truncated structure (shorter than the
/// defined 0x1F-byte length) must never read out of bounds, and every
/// accessor past the available bytes must report its documented
/// zero/"unavailable" default.
BOOST_AUTO_TEST_CASE(TruncatedStructureGuardsEveryFieldTestCase)
{
    // Only Error Type/Granularity/Operation present (0x04-0x06); nothing
    // from Vendor Syndrome onward.
    std::vector<uint8_t> formatted_area = {0x06, 0x03, 0x03};
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x07));

    std::vector<uint8_t> buffer;
    std::unique_ptr<MemoryErrorInformation64BitEntry> entry = build_entry(formatted_area, buffer);

    BOOST_CHECK_EQUAL(entry->get_error_type(), static_cast<uint8_t>(0x06));
    BOOST_CHECK_EQUAL(entry->get_error_granularity(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry->get_error_operation(), static_cast<uint8_t>(0x03));

    BOOST_CHECK_EQUAL(entry->get_vendor_syndrome(), static_cast<uint32_t>(0));
    BOOST_CHECK(!entry->is_vendor_syndrome_known());
    BOOST_CHECK_EQUAL(entry->get_memory_array_error_address(), static_cast<uint64_t>(0));
    BOOST_CHECK(!entry->is_memory_array_error_address_known());
    BOOST_CHECK_EQUAL(entry->get_device_error_address(), static_cast<uint64_t>(0));
    BOOST_CHECK(!entry->is_device_error_address_known());
    BOOST_CHECK_EQUAL(entry->get_error_resolution(), static_cast<uint32_t>(0));
    BOOST_CHECK(!entry->is_error_resolution_known());
}

/// Milestone 0010 Story 03.0 Task 02's own success criterion:
/// MemoryErrorInformation64Bit resolves through SMBiosEntryFactory::create()
/// to a real MemoryErrorInformation64BitEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsMemoryErrorInformation64BitEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        0x06, 0x03, 0x03, 0x00000000, 0x8000000000000000ULL, 0x8000000000000000ULL, 0x80000000);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::MemoryErrorInformation64Bit, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<MemoryErrorInformation64BitEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
