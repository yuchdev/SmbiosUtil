#include <cstdint>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/port_connection_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive PortConnectionEntry (smbios/include/smbios/
// port_connection_entry.h) across its length-guard boundary. Unlike
// MemoryDeviceEntry/BiosInformationEntry, PortConnectionEntry has no version
// chain (confirmed by its own header comment: "fully compatible with all
// previous versions"), so there is only one boundary to exercise: the
// formatted area's minimum valid length (0x09), covered per-getter by its
// own `get_entry_size() < offset + sizeof(uint8_t)` guard rather than by a
// single constructor-level check.
//
// The PS/2-keyboard and USB-mouse decode scenarios already exist in
// tests/unit/unit_tests.cpp (PortConnectionEntryParsesPS2KeyboardTestCase /
// PortConnectionEntryParsesUSBMouseTestCase) and are intentionally not
// duplicated here.
//
// Wire layout (see smbios/include/smbios/port_connection_entry.h and
// smbios/src/port_connection_entry.cpp's getters):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  internal_designator_index (no public getter)
//   offset 0x05 : uint8_t  internal_connection (ConnectorType)
//   offset 0x06 : uint8_t  external_designator_index (no public getter)
//   offset 0x07 : uint8_t  external_connection (ConnectorType)
//   offset 0x08 : uint8_t  port_type (PortType)
// minimum valid (full) formatted length = 0x09.

BOOST_AUTO_TEST_SUITE(PortConnectionEntryUnitTests)

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

/// @brief Build a full-length (0x09) formatted-area buffer with known byte
/// values at every offset, then truncate to `length` and set the header's
/// own length byte accordingly. `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length)
{
    std::vector<uint8_t> full;
    full.push_back(SMBios::PortConnection); // 0x00 type = 8
    full.push_back(0x00);                   // 0x01 length (patched below)
    full.push_back(0x01);                   // 0x02 handle low byte
    full.push_back(0x00);                   // 0x03 handle high byte

    full.push_back(0x01);                                   // 0x04 internal_designator_index
    full.push_back(PortConnectionEntry::PS2);                // 0x05 internal_connection
    full.push_back(0x02);                                   // 0x06 external_designator_index
    full.push_back(PortConnectionEntry::RJ45);                // 0x07 external_connection
    full.push_back(PortConnectionEntry::Keyboard);            // 0x08 port_type

    BOOST_REQUIRE_EQUAL(full.size(), static_cast<size_t>(0x09));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), full.size());

    std::vector<uint8_t> buffer(full.begin(), full.begin() + length);
    buffer[1] = length;

    append_string_terminator(buffer);
    return buffer;
}

} // namespace

/// Normal case: header.length == 0x09 (minimum valid/full formatted length)
/// - all three getters decode their encoded byte correctly.
BOOST_AUTO_TEST_CASE(PortConnectionEntryMinimumValidLengthDecodesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x09);
    DMIHeader header = make_header(buffer);

    PortConnectionEntry entry(header, SMBiosVersion{2, 0});

    BOOST_CHECK_EQUAL(entry.get_internal_connection_type(), static_cast<uint8_t>(PortConnectionEntry::PS2));
    BOOST_CHECK_EQUAL(entry.get_external_connection_type(), static_cast<uint8_t>(PortConnectionEntry::RJ45));
    BOOST_CHECK_EQUAL(entry.get_port_type(), static_cast<uint8_t>(PortConnectionEntry::Keyboard));
}

/// Edge case: header.length == 0x08 - internal_connection (0x05, needs
/// length >= 0x06) and external_connection (0x07, needs length >= 0x08)
/// still decode, but port_type (0x08, needs length >= 0x09) must default,
/// exercising the exact per-getter boundary rather than a single
/// constructor-level cutoff.
BOOST_AUTO_TEST_CASE(PortConnectionEntryOneByteBelowMinimumDefaultsPortTypeOnlyTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x08);
    DMIHeader header = make_header(buffer);

    PortConnectionEntry entry(header, SMBiosVersion{2, 0});

    BOOST_CHECK_EQUAL(entry.get_internal_connection_type(), static_cast<uint8_t>(PortConnectionEntry::PS2));
    BOOST_CHECK_EQUAL(entry.get_external_connection_type(), static_cast<uint8_t>(PortConnectionEntry::RJ45));
    BOOST_CHECK_EQUAL(entry.get_port_type(), static_cast<uint8_t>(PortConnectionEntry::NonePort));
}

/// Invalid-input case: header.length == 0x04 (bare common header, no
/// formatted area at all) - every length-guarded getter must fall back to
/// its documented default instead of reading past the buffer.
BOOST_AUTO_TEST_CASE(PortConnectionEntryTruncatedBelowMinimumDefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    PortConnectionEntry entry(header, SMBiosVersion{2, 0});

    BOOST_CHECK_EQUAL(entry.get_internal_connection_type(), static_cast<uint8_t>(PortConnectionEntry::NoneConnector));
    BOOST_CHECK_EQUAL(entry.get_external_connection_type(), static_cast<uint8_t>(PortConnectionEntry::NoneConnector));
    BOOST_CHECK_EQUAL(entry.get_port_type(), static_cast<uint8_t>(PortConnectionEntry::NonePort));
}

BOOST_AUTO_TEST_SUITE_END()
