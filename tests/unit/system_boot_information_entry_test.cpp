#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/system_boot_information_entry.hpp>
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
// buffers drive SystemBootInformationEntry (smbios/include/smbios/
// system_boot_information_entry.h) across the header.length tiers named by
// docs/roadmap/0004-.../02.0-.../03-system-slots-and-memory-array.md,
// cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 32
// ("System Boot Information"):
//   tier 1: header.length < 0x0B   -> boot_status defaults, payload empty
//   tier 2: header.length == 0x0B  -> boot_status decodes; payload empty
//                                      (0 trailing bytes)
//   tier 3: header.length == 0x0D  -> boot_status decodes; payload has 2
//                                      trailing bytes
//   tier 4: header.length == 0x10  -> boot_status decodes; payload has 5
//                                      trailing bytes (proves the payload
//                                      is not hardcoded to one size)
//
// Wire layout (see smbios/include/smbios/system_boot_information_entry.h's
// doc comment):
//   offset 0x00 : uint8_t  type     (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04-0x09 : reserved, 6 bytes, no getter
//   offset 0x0A : uint8_t  boot_status
//   offset 0x0B.. : variable-length vendor/status-specific payload

BOOST_AUTO_TEST_SUITE(SystemBootInformationEntryUnitTests)

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

/// @brief Build a formatted-area buffer: 6 reserved bytes, a boot_status
/// byte, then `payload_size` trailing payload bytes with a recognizable
/// ascending pattern (0xC0, 0xC1, 0xC2, ...), via
/// SyntheticTableBuilder::add_structure() (which computes header.length
/// automatically from the formatted area's size). `boot_status` is
/// overridable so each test case can drive the exact BootStatus/range
/// branch it needs.
std::vector<uint8_t> build_buffer(uint8_t boot_status, size_t payload_size)
{
    std::vector<uint8_t> formatted_area(6, 0x00); // 0x04-0x09 reserved, 6 bytes
    formatted_area.push_back(boot_status);         // 0x0A boot_status

    for (size_t i = 0; i < payload_size; ++i) {
        formatted_area.push_back(static_cast<uint8_t>(0xC0 + i)); // 0x0B.. payload
    }

    BOOST_REQUIRE_LE(formatted_area.size() + 4, static_cast<size_t>(0xFF));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemBootInformation, formatted_area);
    return builder.build();
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x0B means the boot_status
/// byte is not actually present, so it must default.
BOOST_AUTO_TEST_CASE(SystemBootInformationEntryTruncatedBelow0x0BDefaultsBootStatusTestCase)
{
    // truncate to below 0x0B entirely (header only, no reserved/boot_status
    // bytes at all)
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemBootInformation, std::vector<uint8_t>{});
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);

    SystemBootInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_boot_status(), static_cast<uint8_t>(SystemBootInformationEntry::NoErrorsDetected));
    BOOST_CHECK(entry.get_status_data().empty());
}

/// Normal case: header.length == 0x0B - boot_status decodes; the payload is
/// empty (0 trailing bytes), proving the payload accessor handles the
/// "no payload at all" case cleanly.
BOOST_AUTO_TEST_CASE(SystemBootInformationEntryTier0x0BDecodesBootStatusEmptyPayloadTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(SystemBootInformationEntry::NoBootableMedia, 0);
    DMIHeader header = make_header(buffer);

    SystemBootInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_boot_status(), static_cast<uint8_t>(SystemBootInformationEntry::NoBootableMedia));
    BOOST_CHECK_EQUAL(entry.get_boot_status_string(), std::string("No bootable media"));
    BOOST_CHECK(entry.get_status_data().empty());
    BOOST_CHECK_EQUAL(entry.get_status_data_string(), std::string(""));
}

/// Variable-length payload, case 1 of 2: a 2-byte trailing payload decodes
/// correctly (length 0x0D).
BOOST_AUTO_TEST_CASE(VariableLengthPayloadDecodesTwoBytesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(SystemBootInformationEntry::UserRequestedBoot, 2);
    DMIHeader header = make_header(buffer);

    SystemBootInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    const std::vector<uint8_t> payload = entry.get_status_data();
    BOOST_REQUIRE_EQUAL(payload.size(), static_cast<size_t>(2));
    BOOST_CHECK_EQUAL(payload[0], static_cast<uint8_t>(0xC0));
    BOOST_CHECK_EQUAL(payload[1], static_cast<uint8_t>(0xC1));
    BOOST_CHECK_EQUAL(entry.get_status_data_string(), std::string("c0 c1"));
}

/// Variable-length payload, case 2 of 2: a DIFFERENT, longer (5-byte)
/// trailing payload also decodes correctly (length 0x10) - proves the
/// accessor is not hardcoded to one payload size.
BOOST_AUTO_TEST_CASE(VariableLengthPayloadDecodesFiveBytesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(SystemBootInformationEntry::SystemSecurityViolation, 5);
    DMIHeader header = make_header(buffer);

    SystemBootInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    const std::vector<uint8_t> payload = entry.get_status_data();
    BOOST_REQUIRE_EQUAL(payload.size(), static_cast<size_t>(5));
    for (size_t i = 0; i < payload.size(); ++i) {
        BOOST_CHECK_EQUAL(payload[i], static_cast<uint8_t>(0xC0 + i));
    }
    BOOST_CHECK_EQUAL(entry.get_status_data_string(), std::string("c0 c1 c2 c3 c4"));
}

/// Boot status, OEM-specific numeric range (128-191).
BOOST_AUTO_TEST_CASE(BootStatusOemSpecificRangeTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(150, 0);
    DMIHeader header = make_header(buffer);

    SystemBootInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_boot_status_string(), std::string("OEM-specific"));
}

/// Boot status, Product-specific numeric range (192-255).
BOOST_AUTO_TEST_CASE(BootStatusProductSpecificRangeTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(200, 0);
    DMIHeader header = make_header(buffer);

    SystemBootInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_boot_status_string(), std::string("Product-specific"));
}

/// Boot status, reserved-for-future-assignment range (9-127): not named,
/// not OEM/Product-specific, falls back to "OutOfSpec".
BOOST_AUTO_TEST_CASE(BootStatusReservedRangeRendersOutOfSpecTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(50, 0);
    DMIHeader header = make_header(buffer);

    SystemBootInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_boot_status_string(), std::string("OutOfSpec"));
}

/// Invalid-input case: constructing with a non-SystemBootInformation header
/// type must throw, not silently misparse.
BOOST_AUTO_TEST_CASE(WrongHeaderTypeThrowsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(SystemBootInformationEntry::NoErrorsDetected, 0);
    buffer[0] = SMBios::BIOSInformation; // deliberately wrong type
    DMIHeader header = make_header(buffer);

    BOOST_CHECK_THROW(SystemBootInformationEntry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0004 Story 02.0's own success criterion:
/// SystemBootInformation resolves through SMBiosEntryFactory::create() to
/// a real SystemBootInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemBootInformationEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(SystemBootInformationEntry::NoErrorsDetected, 2);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemBootInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
