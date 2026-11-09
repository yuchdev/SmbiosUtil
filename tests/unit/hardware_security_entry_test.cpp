#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.h>
#include <libsmbios/tables/hardware_security_entry.h>
#include <libsmbios/smbios_entry_factory.h>
#include <libsmbios/abstract_smbios_entry.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive HardwareSecurityEntry
// (smbios/include/smbios/tables/hardware_security_entry.h), cross-checked
// against DSP0134 3.9.0's own 7.25 (Table 93), whose Hardware Security
// Settings row was redacted in this task's own source excerpt and was
// therefore re-confirmed directly against docs/standard/DSP0134_3.9.0.md
// before writing any decoding logic here:
//   Bits 7:6 Power-on Password Status, Bits 5:4 Keyboard Password Status,
//   Bits 3:2 Administrator Password Status, Bits 1:0 Front Panel Reset
//   Status - each: 00b Disabled, 01b Enabled, 10b Not Implemented,
//   11b Unknown.
//
// Wire layout (formatted area, offset relative to structure start):
//   offset 0x04 : uint8_t settings

BOOST_AUTO_TEST_SUITE(HardwareSecurityEntryUnitTests)

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

/// Normal case: every documented SecurityStatus value (Table 93's shared
/// 2-bit space), placed in each of the four sub-fields independently, to
/// confirm bit positions/shifts are correct and not aliased.
BOOST_AUTO_TEST_CASE(EverySecurityStatusValueDecodesInEveryFieldTestCase)
{
    const std::vector<std::pair<HardwareSecurityEntry::SecurityStatus, std::string>> cases{
        {HardwareSecurityEntry::Disabled, "Disabled"},
        {HardwareSecurityEntry::Enabled, "Enabled"},
        {HardwareSecurityEntry::NotImplemented, "Not Implemented"},
        {HardwareSecurityEntry::Unknown, "Unknown"},
    };

    for (const auto& power_on_case : cases) {
        for (const auto& keyboard_case : cases) {
            for (const auto& admin_case : cases) {
                for (const auto& front_panel_case : cases) {
                    const uint8_t settings = static_cast<uint8_t>(
                        (power_on_case.first << 6) |
                        (keyboard_case.first << 4) |
                        (admin_case.first << 2) |
                        (front_panel_case.first << 0));

                    std::vector<uint8_t> formatted_area{settings};

                    smbios::test::SyntheticTableBuilder builder;
                    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
                    std::vector<uint8_t> buffer = builder.build();

                    DMIHeader header = make_header(buffer);
                    HardwareSecurityEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

                    BOOST_CHECK_EQUAL(entry.get_power_on_password_status(), power_on_case.first);
                    BOOST_CHECK_EQUAL(entry.get_power_on_password_status_string(), power_on_case.second);
                    BOOST_CHECK_EQUAL(entry.get_keyboard_password_status(), keyboard_case.first);
                    BOOST_CHECK_EQUAL(entry.get_keyboard_password_status_string(), keyboard_case.second);
                    BOOST_CHECK_EQUAL(entry.get_administrator_password_status(), admin_case.first);
                    BOOST_CHECK_EQUAL(entry.get_administrator_password_status_string(), admin_case.second);
                    BOOST_CHECK_EQUAL(entry.get_front_panel_reset_status(), front_panel_case.first);
                    BOOST_CHECK_EQUAL(entry.get_front_panel_reset_status_string(), front_panel_case.second);
                }
            }
        }
    }
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults to Disabled (0).
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    HardwareSecurityEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_settings(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_power_on_password_status(), HardwareSecurityEntry::Disabled);
    BOOST_CHECK_EQUAL(entry.get_keyboard_password_status(), HardwareSecurityEntry::Disabled);
    BOOST_CHECK_EQUAL(entry.get_administrator_password_status(), HardwareSecurityEntry::Disabled);
    BOOST_CHECK_EQUAL(entry.get_front_panel_reset_status(), HardwareSecurityEntry::Disabled);
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area{0xFF};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(HardwareSecurityEntry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 04.0 Task 02's own success criterion:
/// HardwareSecurity resolves through SMBiosEntryFactory::create() to a real
/// HardwareSecurityEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsHardwareSecurityEntryTestCase)
{
    std::vector<uint8_t> formatted_area{0x55};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<HardwareSecurityEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
