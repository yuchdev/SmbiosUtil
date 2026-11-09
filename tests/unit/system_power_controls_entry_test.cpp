#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/system_power_controls_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive SystemPowerControlsEntry
// (smbios/include/smbios/tables/system_power_controls_entry.h),
// cross-checked against DSP0134 3.9.0's own 7.26/7.26.1 (Table 94), whose
// source excerpt was NOT redacted (every field is a whole BCD byte, no
// sub-byte bit-field confirmation needed).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t month_bcd
//   offset 0x05 : uint8_t day_bcd
//   offset 0x06 : uint8_t hour_bcd
//   offset 0x07 : uint8_t minute_bcd
//   offset 0x08 : uint8_t second_bcd

BOOST_AUTO_TEST_SUITE(SystemPowerControlsEntryUnitTests)

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

/// Normal case: a real BCD date/time decode, with an EXPLICIT
/// value-equality assertion per field (not just "doesn't crash"). Byte
/// 0x25 BCD -> decimal 25, per the task's own worked example.
BOOST_AUTO_TEST_CASE(RealBcdDateTimeDecodesToExpectedDecimalTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x12, // 0x04 month_bcd    -> 12
        0x25, // 0x05 day_bcd      -> 25
        0x09, // 0x06 hour_bcd     -> 9
        0x45, // 0x07 minute_bcd   -> 45
        0x59, // 0x08 second_bcd   -> 59
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerControls, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemPowerControlsEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_month_bcd(), static_cast<uint8_t>(0x12));
    BOOST_CHECK_EQUAL(entry.get_month(), static_cast<uint8_t>(12));

    BOOST_CHECK_EQUAL(entry.get_day_bcd(), static_cast<uint8_t>(0x25));
    BOOST_CHECK_EQUAL(entry.get_day(), static_cast<uint8_t>(25));

    BOOST_CHECK_EQUAL(entry.get_hour_bcd(), static_cast<uint8_t>(0x09));
    BOOST_CHECK_EQUAL(entry.get_hour(), static_cast<uint8_t>(9));

    BOOST_CHECK_EQUAL(entry.get_minute_bcd(), static_cast<uint8_t>(0x45));
    BOOST_CHECK_EQUAL(entry.get_minute(), static_cast<uint8_t>(45));

    BOOST_CHECK_EQUAL(entry.get_second_bcd(), static_cast<uint8_t>(0x59));
    BOOST_CHECK_EQUAL(entry.get_second(), static_cast<uint8_t>(59));
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults to 0.
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerControls, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemPowerControlsEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_month_bcd(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_month(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_day_bcd(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_day(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_hour_bcd(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_hour(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_minute_bcd(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_minute(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_second_bcd(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_second(), static_cast<uint8_t>(0));
}

/// Invalid-input case: a malformed BCD byte (0xFF, not a valid BCD digit
/// pair) decodes via the documented mechanical nibble-pair formula rather
/// than throwing or being silently clamped - (0xF >> 4 is 15) * 10 + 15 =
/// 165 - and is NOT validated against any field's documented range, per
/// bcd_to_decimal()'s own doc comment.
BOOST_AUTO_TEST_CASE(MalformedBcdByteDecodesMechanicallyWithoutRangeValidationTestCase)
{
    std::vector<uint8_t> formatted_area{
        0xFF, // 0x04 month_bcd (malformed)
        0x00, 0x00, 0x00, 0x00,
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerControls, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemPowerControlsEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_month_bcd(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK_EQUAL(entry.get_month(), static_cast<uint8_t>(165));
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area{0x01, 0x01, 0x01, 0x01, 0x01};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(SystemPowerControlsEntry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 04.0 Task 02's own success criterion:
/// SystemPowerControls resolves through SMBiosEntryFactory::create() to a
/// real SystemPowerControlsEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemPowerControlsEntryTestCase)
{
    std::vector<uint8_t> formatted_area{0x01, 0x15, 0x08, 0x30, 0x00};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerControls, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemPowerControlsEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
