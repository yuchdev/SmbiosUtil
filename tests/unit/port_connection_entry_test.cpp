#include <cstdint>
#include <vector>

#include <libsmbios/smbios.h>
#include <libsmbios/tables/port_connection_entry.h>

#include "synthetic_table_builder.h"

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

/// @brief Build a full-length (0x09) formatted-area buffer with known byte
/// values at every offset, then truncate to `length` (via
/// SyntheticTableBuilder::add_structure(), which computes header.length
/// automatically from the formatted area's size). `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length)
{
    std::vector<uint8_t> full_formatted_area{
        0x01,                                 // 0x04 internal_designator_index
        PortConnectionEntry::PS2,             // 0x05 internal_connection
        0x02,                                 // 0x06 external_designator_index
        PortConnectionEntry::RJ45,            // 0x07 external_connection
        PortConnectionEntry::Keyboard,        // 0x08 port_type
    };

    BOOST_REQUIRE_EQUAL(full_formatted_area.size(), static_cast<size_t>(0x09 - 4));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), static_cast<size_t>(0x09));
    BOOST_REQUIRE_GE(static_cast<size_t>(length), static_cast<size_t>(4));

    std::vector<uint8_t> formatted_area(full_formatted_area.begin(),
                                         full_formatted_area.begin() + (length - 4));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortConnection, formatted_area);
    return builder.build();
}

} // namespace

/// Normal case: header.length == 0x09 (minimum valid/full formatted length)
/// - all three getters decode their encoded byte correctly.
BOOST_AUTO_TEST_CASE(PortConnectionEntryMinimumValidLengthDecodesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x09);
    DMIHeader header = make_header(buffer);

    PortConnectionEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

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

    PortConnectionEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

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

    PortConnectionEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_internal_connection_type(), static_cast<uint8_t>(PortConnectionEntry::NoneConnector));
    BOOST_CHECK_EQUAL(entry.get_external_connection_type(), static_cast<uint8_t>(PortConnectionEntry::NoneConnector));
    BOOST_CHECK_EQUAL(entry.get_port_type(), static_cast<uint8_t>(PortConnectionEntry::NonePort));
}

BOOST_AUTO_TEST_SUITE_END()
