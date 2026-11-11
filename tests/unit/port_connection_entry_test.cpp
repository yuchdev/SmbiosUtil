#include <cstdint>
#include <string>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/port_connection_entry.hpp>

#include "synthetic_table_builder.hpp"
#include "field_assertions.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive PortConnectionEntry (smbios/include/smbios/
// port_connection_entry.hpp) across its length-guard boundary. Unlike
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
// Wire layout (see smbios/include/smbios/port_connection_entry.hpp and
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

/// Regression case: internal_connection == NoneConnector (0x00) must render
/// as the numeric string "0", not an embedded NUL character - streaming a
/// uint8_t whose value is 0 through operator<<(uint8_t) previously invoked
/// the char overload and spliced a NUL into the middle of the returned
/// std::string.
BOOST_AUTO_TEST_CASE(PortConnectionEntryNoneConnectorRendersNumericNotNulTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x01,                                 // 0x04 internal_designator_index
        PortConnectionEntry::NoneConnector,   // 0x05 internal_connection == 0x00
        0x02,                                 // 0x06 external_designator_index
        PortConnectionEntry::NoneConnector,   // 0x07 external_connection == 0x00
        PortConnectionEntry::Keyboard,        // 0x08 port_type
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortConnection, formatted_area);
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);

    PortConnectionEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    std::string description = entry.render_to_description();

    BOOST_CHECK_EQUAL(description.find('\0'), std::string::npos);
    BOOST_CHECK_NE(description.find("Internal Connector Type: 0\n"), std::string::npos);
    BOOST_CHECK_NE(description.find("External Connector Type: 0\n"), std::string::npos);
}

// get_fields() (milestone 0009, Story 01.0, Task 04)
//
// Type: unit - deterministic, hand-built buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description() field by field;
// the raw connector type codes are uint64_t (printed as plain decimals),
// the connection/port type names stay strings. No bitmask in this type.
// Boundaries: a 0x08 structure (port type defaults), a bare 0x04 header
// (every field defaults, but every text line - so every field - remains).
// On failure first check: PortConnectionEntry::get_fields() in
// libsmbios/src/port_connection_entry.cpp.

/// Normal case: every text label becomes a field, in text order.
BOOST_AUTO_TEST_CASE(PortConnectionEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> buffer = build_buffer(0x09);
    PortConnectionEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Internal Connector Type", num(PortConnectionEntry::PS2)},
        {"Internal Connection Type", str("PS/2")},
        {"External Connector Type", num(PortConnectionEntry::RJ45)},
        {"External Connection Type", str("RJ-45")},
        {"Port Type", str("Keyboard Port")},
    });
}

/// Boundary cases: the text prints every line unconditionally, so a short
/// structure keeps every field, with the getters' defaults for the absent
/// bytes.
BOOST_AUTO_TEST_CASE(PortConnectionEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> buffer = build_buffer(0x08);
        PortConnectionEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Internal Connector Type", num(PortConnectionEntry::PS2)},
            {"Internal Connection Type", str("PS/2")},
            {"External Connector Type", num(PortConnectionEntry::RJ45)},
            {"External Connection Type", str("RJ-45")},
            {"Port Type", str("None")},
        });
    }
    {
        std::vector<uint8_t> buffer = build_buffer(0x04);
        PortConnectionEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Internal Connector Type", num(0)},
            {"Internal Connection Type", str("None")},
            {"External Connector Type", num(0)},
            {"External Connection Type", str("None")},
            {"Port Type", str("None")},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
