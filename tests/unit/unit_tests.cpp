#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <memory>

#include <smbios/smbios.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/port_connection_entry.h>
// `SMBiosImpl`/`unix_bios.h` moved into the Linux-only `smbios_source_linux`
// library (milestone 0005, Story 01.1, ADR 0001) - it is no longer part of
// core `smbios` on macOS/BSD/Solaris, so this guard narrows from "any
// unix-like platform" to Linux specifically, matching where the header (and
// the library `smbios` now transitively links on Linux to provide it) still
// actually exists.
#if defined(__linux__)
#include <smbios/source_linux/unix_bios.h>
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: no real SMBios instance is
// constructed here. These either exercise free functions (SMBiosVersion
// comparison), or build a synthetic DMIHeader/raw buffer by hand to drive
// the entry classes and the factory.

BOOST_AUTO_TEST_SUITE(SmbiosUnitTests)

//////////////////////////////////////////////////////////////////////////
// 1a. SMBiosVersion operator>/operator< - lexicographic comparison

/// Normal case: higher major wins regardless of minor
BOOST_AUTO_TEST_CASE(SMBiosVersionGreaterMajorWinsOverMinorTestCase)
{
    SMBiosVersion v2_9{2, 9};
    SMBiosVersion v3_0{3, 0};

    // This is the exact case that used to be wrong: 2.9 > 3.0 must be false,
    // even though minor_version (9 > 0) would incorrectly say otherwise.
    BOOST_CHECK(!(v2_9 > v3_0));
    BOOST_CHECK(v3_0 > v2_9);
}

/// Normal case: higher major, lower minor still greater
BOOST_AUTO_TEST_CASE(SMBiosVersionGreaterMajorDominatesTestCase)
{
    SMBiosVersion v3_0{3, 0};
    SMBiosVersion v2_8{2, 8};

    BOOST_CHECK(v3_0 > v2_8);
    BOOST_CHECK(!(v2_8 > v3_0));
}

/// Edge case: same major, minor decides
BOOST_AUTO_TEST_CASE(SMBiosVersionGreaterSameMajorMinorDecidesTestCase)
{
    SMBiosVersion v3_1{3, 1};
    SMBiosVersion v3_0{3, 0};

    BOOST_CHECK(v3_1 > v3_0);
    BOOST_CHECK(!(v3_0 > v3_1));
}

/// Mirror image of the above three for operator<
BOOST_AUTO_TEST_CASE(SMBiosVersionLessMajorWinsOverMinorTestCase)
{
    SMBiosVersion v2_9{2, 9};
    SMBiosVersion v3_0{3, 0};

    BOOST_CHECK(v2_9 < v3_0);
    BOOST_CHECK(!(v3_0 < v2_9));
}

BOOST_AUTO_TEST_CASE(SMBiosVersionLessMajorDominatesTestCase)
{
    SMBiosVersion v3_0{3, 0};
    SMBiosVersion v2_8{2, 8};

    BOOST_CHECK(!(v3_0 < v2_8));
    BOOST_CHECK(v2_8 < v3_0);
}

BOOST_AUTO_TEST_CASE(SMBiosVersionLessSameMajorMinorDecidesTestCase)
{
    SMBiosVersion v3_1{3, 1};
    SMBiosVersion v3_0{3, 0};

    BOOST_CHECK(!(v3_1 < v3_0));
    BOOST_CHECK(v3_0 < v3_1);
}

//////////////////////////////////////////////////////////////////////////
// 1b. PortConnectionEntry parsing from a hand-built buffer
//
// Wire layout (see smbios/include/smbios/port_connection_entry.h,
// struct PortConnection, #pragma pack(push, 1)):
//   offset 0 : uint32_t header      -> type(1) + length(1) + handle(2)
//   offset 4 : uint8_t internal_designator  (DMI string index)
//   offset 5 : uint8_t internal_connection  (ConnectorType)
//   offset 6 : uint8_t external_designator  (DMI string index)
//   offset 7 : uint8_t external_connection  (ConnectorType)
//   offset 8 : uint8_t port_type            (PortType)
// total formatted length = 9, matching SMBios::PortConnection (type 8).
//
// Note: PortConnectionEntry's own getters (confirmed by reading
// port_connection_entry.cpp) resolve the *_connection/port_type bytes to
// their descriptive enum-map strings; they do not look up the DMI string
// table via the *_designator indices (no public getter exposes those
// indices at all). The DMI string table below is still appended so that
// AbstractSMBiosEntry::parse_dmi_strings() has a well-formed section to
// walk (double NUL terminator) and does not run past the buffer.

namespace {

/// @brief Build a raw SMBIOS "Port Connection" structure (type 8) buffer
std::vector<uint8_t> build_port_connection_buffer(
    uint8_t internal_designator,
    uint8_t internal_connection,
    uint8_t external_designator,
    uint8_t external_connection,
    uint8_t port_type)
{
    std::vector<uint8_t> buffer;

    // 4-byte header: type, length, handle (little-endian uint16_t)
    buffer.push_back(SMBios::PortConnection); // type = 8
    buffer.push_back(0x09);                   // length = 9 (formatted area)
    buffer.push_back(0x01);                   // handle low byte
    buffer.push_back(0x00);                   // handle high byte

    // formatted area
    buffer.push_back(internal_designator);
    buffer.push_back(internal_connection);
    buffer.push_back(external_designator);
    buffer.push_back(external_connection);
    buffer.push_back(port_type);

    // string section: two DMI strings, terminated with a double NUL
    const char strings[] = "Mouse\0Keyboard\0";
    buffer.insert(buffer.end(), strings, strings + sizeof(strings));
    buffer.push_back(0x00); // final terminating NUL completes the double-NUL

    return buffer;
}

DMIHeader make_header(const std::vector<uint8_t>& buffer)
{
    DMIHeader header{};
    header.type = buffer[0];
    header.length = buffer[1];
    header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
    header.data = buffer.data();
    return header;
}

} // namespace

/// Normal case: PS/2 internal, RJ-45 external, Keyboard port
BOOST_AUTO_TEST_CASE(PortConnectionEntryParsesPS2KeyboardTestCase)
{
    std::vector<uint8_t> buffer = build_port_connection_buffer(
        1, PortConnectionEntry::PS2, 2, PortConnectionEntry::RJ45, PortConnectionEntry::Keyboard);
    DMIHeader header = make_header(buffer);

    PortConnectionEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_internal_connection_type(), static_cast<uint8_t>(PortConnectionEntry::PS2));
    BOOST_CHECK_EQUAL(entry.get_internal_connection_string(), "PS/2");

    BOOST_CHECK_EQUAL(entry.get_external_connection_type(), static_cast<uint8_t>(PortConnectionEntry::RJ45));
    BOOST_CHECK_EQUAL(entry.get_external_connection_string(), "RJ-45");

    BOOST_CHECK_EQUAL(entry.get_port_type(), static_cast<uint8_t>(PortConnectionEntry::Keyboard));
    BOOST_CHECK_EQUAL(entry.get_port_string(), "Keyboard Port");
}

/// Second case with different connector/port values, to confirm the parser
/// reads the actual encoded bytes and isn't coincidentally always reading
/// the same offset/value.
BOOST_AUTO_TEST_CASE(PortConnectionEntryParsesUSBMouseTestCase)
{
    std::vector<uint8_t> buffer = build_port_connection_buffer(
        1, PortConnectionEntry::DB9PinMale, 2, PortConnectionEntry::AccessBusUSB, PortConnectionEntry::Mouse);
    DMIHeader header = make_header(buffer);

    PortConnectionEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_internal_connection_type(), static_cast<uint8_t>(PortConnectionEntry::DB9PinMale));
    BOOST_CHECK_EQUAL(entry.get_internal_connection_string(), "DB-9 Pin Male");

    BOOST_CHECK_EQUAL(entry.get_external_connection_type(), static_cast<uint8_t>(PortConnectionEntry::AccessBusUSB));
    BOOST_CHECK_EQUAL(entry.get_external_connection_string(), "Access Bus (USB)");

    BOOST_CHECK_EQUAL(entry.get_port_type(), static_cast<uint8_t>(PortConnectionEntry::Mouse));
    BOOST_CHECK_EQUAL(entry.get_port_string(), "Mouse Port");
}

//////////////////////////////////////////////////////////////////////////
// 1c. SMBiosEntryFactory registration lookup

/// Unregistered type: factory looks up header.type in its map before ever
/// touching header.data (confirmed by reading smbios_entry_factory.cpp), so
/// a default-constructed header with no backing buffer is sufficient here.
BOOST_AUTO_TEST_CASE(SMBiosEntryFactoryReturnsNullForUnregisteredTypeTestCase)
{
    DMIHeader header{};
    header.type = 99; // not a registered SMBiosHandler value
    header.length = 0;
    header.handle = 0;
    header.data = nullptr;

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, nullptr);

    BOOST_CHECK(entry == nullptr);
}

/// Registered type: factory produces a concrete, correctly-typed entry
BOOST_AUTO_TEST_CASE(SMBiosEntryFactoryCreatesPortConnectionEntryTestCase)
{
    std::vector<uint8_t> buffer = build_port_connection_buffer(
        1, PortConnectionEntry::PS2, 2, PortConnectionEntry::RJ45, PortConnectionEntry::Keyboard);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK_EQUAL(entry->get_type(), "Port Connection");
}

//////////////////////////////////////////////////////////////////////////
// 1d. SMBiosImpl::get_table_size() regression test
//
// Regression for a missing-`return` bug in get_table_size(): the buggy
// version fell through without returning table_buffer_.size(), so this
// test fails against that old code and passes against the fix at
// smbios/src/source_linux/unix_bios.cpp.
//
// read_from_physical_memory() is a real, non-mocked seam: it just
// std::move()s the supplied buffer into table_buffer_. Constructing
// SMBiosImpl performs a real (sysfs/EFI) discovery attempt that fails
// cleanly without root/real hardware - that failure is expected and
// harmless here, since we overwrite table_buffer_ immediately after.
//
// Milestone 0005, Story 01.1 (ADR 0001) narrows this guard to Linux only:
// `SMBiosImpl` (`unix_bios.h`) now lives in the Linux-only
// `smbios_source_linux` library, not core `smbios`, so it is simply not
// available to this test target on macOS/BSD/Solaris any more.

#if defined(__linux__)

/// Normal case: get_table_size() reflects the exact size of the buffer
/// most recently handed to read_from_physical_memory()
BOOST_AUTO_TEST_CASE(SMBiosImplGetTableSizeReflectsPhysicalMemoryDumpTestCase)
{
    SMBiosImpl impl;

    std::vector<uint8_t> synthetic_dump(42, 0xAB); // 42 bytes of arbitrary content

    impl.read_from_physical_memory(synthetic_dump);

    BOOST_CHECK_EQUAL(impl.get_table_size(), static_cast<size_t>(42));
}

#endif //defined(__linux__)

BOOST_AUTO_TEST_SUITE_END()
