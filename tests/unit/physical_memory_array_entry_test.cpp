#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/physical_memory_array_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive PhysicalMemoryArrayEntry (smbios/include/smbios/
// physical_memory_array_entry.h) across the header.length tiers named by
// docs/roadmap/0004-.../02.0-.../03-system-slots-and-memory-array.md,
// cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 16
// ("Physical Memory Array"):
//   tier 1: header.length < 0x0F   -> every length-guarded getter defaults
//   tier 2: header.length == 0x0F  -> location..number_of_memory_devices
//                                      decode; extended capacity (needs
//                                      length >= 0x17) unreachable
//   tier 3: header.length == 0x17  -> extended capacity also reachable
//   plus the dedicated without-sentinel/with-sentinel/length-insufficient
//   triple for the 0x80000000 maximum-capacity sentinel named by this
//   task's own success criterion.
//
// Wire layout (see smbios/include/smbios/physical_memory_array_entry.h's
// "@brief 0xNN offset" getter comments):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  location
//   offset 0x05 : uint8_t  use
//   offset 0x06 : uint8_t  memory_error_correction
//   offset 0x07 : uint32_t maximum_capacity (0x80000000 sentinel -> extended at 0x0F)
//   offset 0x0B : uint16_t memory_error_information_handle
//   offset 0x0D : uint16_t number_of_memory_devices
//   offset 0x0F : uint64_t extended_maximum_capacity

BOOST_AUTO_TEST_SUITE(PhysicalMemoryArrayEntryUnitTests)

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

/// @brief Append a minimal well-formed (empty) DMI string-section
/// terminator (double NUL) so AbstractSMBiosEntry::parse_dmi_strings() has a
/// well-formed section to walk and does not run past the buffer.
void append_string_terminator(std::vector<uint8_t>& buffer)
{
    buffer.push_back(0x00);
    buffer.push_back(0x00);
}

/// @brief Build a full-length (0x17) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` and patch
/// the header's own length byte accordingly. `maximum_capacity` is
/// overridable (little-endian DWORD) so each test case can drive the exact
/// sentinel branch it needs; `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length, uint32_t maximum_capacity = 0x00100000 /* 1 GB in kB */)
{
    std::vector<uint8_t> full;
    full.push_back(SMBios::PhysicalMemoryArray); // 0x00 type = 16
    full.push_back(0x00);                        // 0x01 length (patched below)
    full.push_back(0x01);                        // 0x02 handle low byte
    full.push_back(0x00);                        // 0x03 handle high byte

    full.push_back(PhysicalMemoryArrayEntry::SystemBoardOrMotherboard); // 0x04 location
    full.push_back(PhysicalMemoryArrayEntry::SystemMemory);             // 0x05 use
    full.push_back(PhysicalMemoryArrayEntry::MultiBitECC);              // 0x06 memory_error_correction

    // 0x07 maximum_capacity (little-endian DWORD)
    full.push_back(static_cast<uint8_t>(maximum_capacity & 0xFF));
    full.push_back(static_cast<uint8_t>((maximum_capacity >> 8) & 0xFF));
    full.push_back(static_cast<uint8_t>((maximum_capacity >> 16) & 0xFF));
    full.push_back(static_cast<uint8_t>((maximum_capacity >> 24) & 0xFF));

    full.push_back(0xFF); full.push_back(0xFF); // 0x0B memory_error_information_handle = No Error
    full.push_back(0x04); full.push_back(0x00); // 0x0D number_of_memory_devices = 4

    // 0x0F extended_maximum_capacity (little-endian QWORD) = 0x100000000 bytes (4 GB)
    full.push_back(0x00); full.push_back(0x00); full.push_back(0x00); full.push_back(0x00);
    full.push_back(0x01); full.push_back(0x00); full.push_back(0x00); full.push_back(0x00);

    BOOST_REQUIRE_EQUAL(full.size(), static_cast<size_t>(0x17));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), full.size());

    std::vector<uint8_t> buffer(full.begin(), full.begin() + length);
    buffer[1] = length;

    append_string_terminator(buffer);
    return buffer;
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x0F means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(PhysicalMemoryArrayEntryTruncatedBelow0x0FDefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    PhysicalMemoryArrayEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_location(), static_cast<uint8_t>(PhysicalMemoryArrayEntry::LocationOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_use(), static_cast<uint8_t>(PhysicalMemoryArrayEntry::UseOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_memory_error_correction(), static_cast<uint8_t>(PhysicalMemoryArrayEntry::MemoryErrorCorrectionOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_maximum_capacity(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_memory_error_information_handle(), static_cast<uint16_t>(0xFFFE));
    BOOST_CHECK_EQUAL(entry.get_number_of_memory_devices(), static_cast<uint16_t>(0));
}

/// Normal case: header.length == 0x0F - location..number_of_memory_devices
/// decode; the maximum_capacity byte is NOT the sentinel here, so the plain
/// DWORD value is returned.
BOOST_AUTO_TEST_CASE(PhysicalMemoryArrayEntryTier0x0FDecodesBaseFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F);
    DMIHeader header = make_header(buffer);

    PhysicalMemoryArrayEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_location(), static_cast<uint8_t>(PhysicalMemoryArrayEntry::SystemBoardOrMotherboard));
    BOOST_CHECK_EQUAL(entry.get_location_string(), std::string("System Board Or Motherboard"));
    BOOST_CHECK_EQUAL(entry.get_use(), static_cast<uint8_t>(PhysicalMemoryArrayEntry::SystemMemory));
    BOOST_CHECK_EQUAL(entry.get_memory_error_correction(), static_cast<uint8_t>(PhysicalMemoryArrayEntry::MultiBitECC));
    BOOST_CHECK_EQUAL(entry.get_maximum_capacity(), static_cast<uint64_t>(0x00100000));
    BOOST_CHECK_EQUAL(entry.get_memory_error_information_handle_string(), std::string("No Error"));
    BOOST_CHECK_EQUAL(entry.get_number_of_memory_devices(), static_cast<uint16_t>(4));
}

/// Maximum capacity sentinel, case without: a plain (non-sentinel) DWORD
/// value is used as-is, even when the buffer is long enough for the
/// extended field.
BOOST_AUTO_TEST_CASE(MaximumCapacityUsesPlainDwordWhenNotSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x17, 0x00100000 /* not the sentinel */);
    DMIHeader header = make_header(buffer);

    PhysicalMemoryArrayEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_maximum_capacity(), static_cast<uint64_t>(0x00100000));
    BOOST_CHECK_EQUAL(entry.get_maximum_capacity_string(), std::string("1048576 kB"));
}

/// Maximum capacity sentinel, case with: 0x80000000 at the legacy DWORD
/// plus a long-enough buffer (length >= 0x17) consults the extended QWORD
/// at 0x0F instead.
BOOST_AUTO_TEST_CASE(MaximumCapacitySentinelConsultsExtendedFieldTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x17, 0x80000000);
    DMIHeader header = make_header(buffer);

    PhysicalMemoryArrayEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    // extended field encodes 0x100000000 (4 GB in bytes)
    BOOST_CHECK_EQUAL(entry.get_maximum_capacity(), static_cast<uint64_t>(0x100000000ULL));
    BOOST_CHECK_EQUAL(entry.get_maximum_capacity_string(), std::string("4294967296 bytes"));
}

/// Regression/edge case: the 0x80000000 sentinel is present, but the buffer
/// is one byte too short for the extended field (length == 0x16, needs
/// 0x17) - the capacity is genuinely unknown, not "fall back to the
/// sentinel value itself".
BOOST_AUTO_TEST_CASE(MaximumCapacitySentinelFallsBackToUnknownWhenLengthInsufficientTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x16, 0x80000000);
    DMIHeader header = make_header(buffer);

    PhysicalMemoryArrayEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_maximum_capacity(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_maximum_capacity_string(), std::string("Unknown"));
}

/// Invalid-input case: constructing with a non-PhysicalMemoryArray header
/// type must throw, not silently misparse.
BOOST_AUTO_TEST_CASE(WrongHeaderTypeThrowsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F);
    buffer[0] = SMBios::BIOSInformation; // deliberately wrong type
    DMIHeader header = make_header(buffer);

    BOOST_CHECK_THROW(PhysicalMemoryArrayEntry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0004 Story 02.0's own success criterion: PhysicalMemoryArray
/// resolves through SMBiosEntryFactory::create() to a real
/// PhysicalMemoryArrayEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsPhysicalMemoryArrayEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x17);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<PhysicalMemoryArrayEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
