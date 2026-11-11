#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/out_of_band_remote_access_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.hpp"
#include "field_assertions.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive OutOfBandRemoteAccessEntry
// (smbios/include/smbios/tables/out_of_band_remote_access_entry.hpp),
// cross-checked against DSP0134 3.9.0's own 7.31 (Table 103), whose
// Connections row was redacted in this task's own source excerpt and was
// therefore re-confirmed directly against docs/standard/DSP0134_3.9.0.md
// before writing any decoding logic here:
//   Bit 0 Inbound Connection Enabled, Bit 1 Outbound Connection Enabled,
//   Bits 7:2 Reserved.
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : BYTE    manufacturer_name (STRING index)
//   offset 0x05 : uint8_t connections (bit-field)

BOOST_AUTO_TEST_SUITE(OutOfBandRemoteAccessEntryUnitTests)

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

} // namespace

/// Normal case: full-length buffer, manufacturer string present, both
/// Connections bits set.
BOOST_AUTO_TEST_CASE(ConnectionsBothBitsSetDecodeTestCase)
{
    // Bit 0 (Inbound) = 1, Bit 1 (Outbound) = 1.
    std::vector<uint8_t> formatted_area{
        0x01, // 0x04 Manufacturer Name (STRING index 1)
        0x03, // 0x05 Connections: inbound + outbound enabled
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OutOfBandRemoteAccess, formatted_area, {"Acme Remote Access"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OutOfBandRemoteAccessEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_name_string(), "Acme Remote Access");
    BOOST_CHECK_EQUAL(entry.get_connections(), static_cast<uint8_t>(0x03));
    BOOST_CHECK(entry.is_inbound_connection_enabled());
    BOOST_CHECK(entry.is_outbound_connection_enabled());
}

/// Normal case: only the Outbound bit set, confirming the two bits are
/// decoded independently (not aliased).
BOOST_AUTO_TEST_CASE(ConnectionsOnlyOutboundBitSetTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x01, // 0x04 Manufacturer Name (STRING index 1)
        0x02, // 0x05 Connections: outbound only
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OutOfBandRemoteAccess, formatted_area, {"Acme Remote Access"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OutOfBandRemoteAccessEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_inbound_connection_enabled());
    BOOST_CHECK(entry.is_outbound_connection_enabled());
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults (empty manufacturer string, 0
/// Connections, both bits disabled).
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OutOfBandRemoteAccess, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OutOfBandRemoteAccessEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_name_string(), std::string());
    BOOST_CHECK_EQUAL(entry.get_connections(), static_cast<uint8_t>(0));
    BOOST_CHECK(!entry.is_inbound_connection_enabled());
    BOOST_CHECK(!entry.is_outbound_connection_enabled());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area{0x01, 0x03};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(OutOfBandRemoteAccessEntry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 05.0 Task 01's own success criterion:
/// OutOfBandRemoteAccess resolves through SMBiosEntryFactory::create() to a
/// real OutOfBandRemoteAccessEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsOutOfBandRemoteAccessEntryTestCase)
{
    std::vector<uint8_t> formatted_area{0x01, 0x03};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OutOfBandRemoteAccess, formatted_area, {"Acme Remote Access"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<OutOfBandRemoteAccessEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009 Story 01.0 Task 05)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(); the two
// Yes/No connection flags become bool fields.
// Boundaries: full (0x06) vs header-only (0x04) structure. The connection
// flags are two fixed Yes/No lines, not a bitset_to_properties() list, so
// there is no ExpandsBitmask case.
// On failure first check: OutOfBandRemoteAccessEntry::get_fields() against
// OutOfBandRemoteAccessEntry::render_to_description() in libsmbios/src/out_of_band_remote_access_entry.cpp.

/// Normal case: inbound disabled, outbound enabled - each flag decoded
/// independently into its own bool field.
BOOST_AUTO_TEST_CASE(OutOfBandRemoteAccessEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area{0x01, 0x02};
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OutOfBandRemoteAccess, formatted_area, {"Acme Remote Access"});
    std::vector<uint8_t> buffer = builder.build();
    OutOfBandRemoteAccessEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Manufacturer Name", str("Acme Remote Access")},
        {"Inbound Connection Enabled", false},
        {"Outbound Connection Enabled", true},
    });
}

/// Boundary case: a header-only structure still yields all three fields
/// (the text prints all three lines) with their defaults.
BOOST_AUTO_TEST_CASE(OutOfBandRemoteAccessEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OutOfBandRemoteAccess, std::vector<uint8_t>{});
    std::vector<uint8_t> buffer = builder.build();
    OutOfBandRemoteAccessEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Manufacturer Name", str("")},
        {"Inbound Connection Enabled", false},
        {"Outbound Connection Enabled", false},
    });
}

BOOST_AUTO_TEST_SUITE_END()
