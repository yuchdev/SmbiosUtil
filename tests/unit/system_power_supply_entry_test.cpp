#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/system_power_supply_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive SystemPowerSupplyEntry
// (smbios/include/smbios/tables/system_power_supply_entry.hpp),
// cross-checked directly against DSP0134 3.9.0's own 7.40/7.40.1 (Table
// 116/117).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  power_unit_group
//   offset 0x05 : uint8_t  location (STRING)
//   offset 0x06 : uint8_t  device_name (STRING)
//   offset 0x07 : uint8_t  manufacturer (STRING)
//   offset 0x08 : uint8_t  serial_number (STRING)
//   offset 0x09 : uint8_t  asset_tag (STRING)
//   offset 0x0A : uint8_t  model_part_number (STRING)
//   offset 0x0B : uint8_t  revision_level (STRING)
//   offset 0x0C : uint16_t max_power_capacity
//   offset 0x0E : uint16_t characteristics (Table 117)
//   offset 0x10 : uint16_t input_voltage_probe_handle
//   offset 0x12 : uint16_t cooling_device_handle
//   offset 0x14 : uint16_t input_current_probe_handle
//
// Handle "absent" sentinel confirmed as 0xFFFF (DSP0134 3.9.0's general
// Handle field description, 7.2/Table 10 - see
// system_power_supply_entry.hpp's own leading comment block).

BOOST_AUTO_TEST_SUITE(SystemPowerSupplyEntryUnitTests)

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

void append_u16(std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

/// Build a minimum-length (0x10) formatted area: Power Unit Group through
/// Power Supply Characteristics, with no trailing handle fields at all.
std::vector<uint8_t> make_minimum_formatted_area(uint8_t power_unit_group, uint16_t max_power_capacity, uint16_t characteristics)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(power_unit_group); // 0x04
    formatted_area.push_back(0x01);             // 0x05 Location
    formatted_area.push_back(0x02);             // 0x06 Device Name
    formatted_area.push_back(0x03);             // 0x07 Manufacturer
    formatted_area.push_back(0x04);             // 0x08 Serial Number
    formatted_area.push_back(0x05);             // 0x09 Asset Tag
    formatted_area.push_back(0x06);             // 0x0A Model Part Number
    formatted_area.push_back(0x07);             // 0x0B Revision Level
    append_u16(formatted_area, max_power_capacity); // 0x0C
    append_u16(formatted_area, characteristics);    // 0x0E
    return formatted_area;
}

/// Build a full-length (0x16) formatted area with all three trailing
/// handle fields present.
std::vector<uint8_t> make_full_formatted_area(uint8_t power_unit_group, uint16_t max_power_capacity, uint16_t characteristics,
                                               uint16_t input_voltage_probe_handle, uint16_t cooling_device_handle,
                                               uint16_t input_current_probe_handle)
{
    std::vector<uint8_t> formatted_area = make_minimum_formatted_area(power_unit_group, max_power_capacity, characteristics);
    append_u16(formatted_area, input_voltage_probe_handle); // 0x10
    append_u16(formatted_area, cooling_device_handle);      // 0x12
    append_u16(formatted_area, input_current_probe_handle); // 0x14
    return formatted_area;
}

} // namespace

/// Normal case / edge case: minimum-length buffer (Length == 0x10, no
/// handle fields present at all). Every handle accessor must fall back to
/// HandleAbsent and every has_*() predicate must be false, purely from the
/// length guard (not from reading a sentinel value that happens not to be
/// there).
BOOST_AUTO_TEST_CASE(MinimumLengthBufferTestCase)
{
    // characteristics = (Switching=0100b << 10) | (OK=011b << 7) |
    // (AutoSwitch=0100b << 3) | present(bit1) | hot-replaceable(bit0)
    //                 = 0x1000 | 0x0180 | 0x0020 | 0x0002 | 0x0001 = 0x11A3
    std::vector<uint8_t> formatted_area = make_minimum_formatted_area(0x00, 450, 0x11A3);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x10));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerSupply, formatted_area,
                           {"Left Supply Bay", "DR-36", "Acme", "SN123", "AT001", "PN456", "2.30"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemPowerSupplyEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_power_unit_group(), static_cast<uint8_t>(0));
    BOOST_CHECK(!entry.is_redundant());
    BOOST_CHECK_EQUAL(entry.get_location_string(), std::string("Left Supply Bay"));
    BOOST_CHECK_EQUAL(entry.get_device_name_string(), std::string("DR-36"));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_string(), std::string("Acme"));
    BOOST_CHECK_EQUAL(entry.get_serial_number_string(), std::string("SN123"));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_string(), std::string("AT001"));
    BOOST_CHECK_EQUAL(entry.get_model_part_number_string(), std::string("PN456"));
    BOOST_CHECK_EQUAL(entry.get_revision_level_string(), std::string("2.30"));
    BOOST_CHECK_EQUAL(entry.get_max_power_capacity(), static_cast<uint16_t>(450));
    BOOST_CHECK(entry.is_max_power_capacity_known());

    BOOST_CHECK_EQUAL(entry.get_power_supply_type(), SystemPowerSupplyEntry::PowerSupplyTypeSwitching);
    BOOST_CHECK_EQUAL(entry.get_power_supply_type_string(), std::string("Switching"));
    BOOST_CHECK_EQUAL(entry.get_power_supply_status(), SystemPowerSupplyEntry::PowerSupplyStatusOk);
    BOOST_CHECK_EQUAL(entry.get_power_supply_status_string(), std::string("OK"));
    BOOST_CHECK_EQUAL(entry.get_voltage_switching(), SystemPowerSupplyEntry::VoltageSwitchingAutoSwitch);
    BOOST_CHECK_EQUAL(entry.get_voltage_switching_string(), std::string("Auto-switch"));
    BOOST_CHECK(!entry.is_unplugged());
    BOOST_CHECK(entry.is_present());
    BOOST_CHECK(entry.is_hot_replaceable());

    // Handle fields are entirely absent from the buffer (Length == 0x10) -
    // every handle accessor must fall back cleanly.
    BOOST_CHECK_EQUAL(entry.get_input_voltage_probe_handle(), SystemPowerSupplyEntry::HandleAbsent);
    BOOST_CHECK(!entry.has_input_voltage_probe());
    BOOST_CHECK_EQUAL(entry.get_cooling_device_handle(), SystemPowerSupplyEntry::HandleAbsent);
    BOOST_CHECK(!entry.has_cooling_device());
    BOOST_CHECK_EQUAL(entry.get_input_current_probe_handle(), SystemPowerSupplyEntry::HandleAbsent);
    BOOST_CHECK(!entry.has_input_current_probe());
}

/// Normal case: all three optional handles present with real (non-sentinel)
/// values.
BOOST_AUTO_TEST_CASE(AllHandlesPresentTestCase)
{
    std::vector<uint8_t> formatted_area = make_full_formatted_area(
        0x05 /* non-zero -> redundant */, 750, 0x0046 /* type=Other(0001), status=Other(001) */,
        0x0021, 0x0022, 0x0023);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x16));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerSupply, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemPowerSupplyEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.is_redundant());

    BOOST_CHECK_EQUAL(entry.get_input_voltage_probe_handle(), static_cast<uint16_t>(0x0021));
    BOOST_CHECK(entry.has_input_voltage_probe());
    BOOST_CHECK_EQUAL(entry.get_cooling_device_handle(), static_cast<uint16_t>(0x0022));
    BOOST_CHECK(entry.has_cooling_device());
    BOOST_CHECK_EQUAL(entry.get_input_current_probe_handle(), static_cast<uint16_t>(0x0023));
    BOOST_CHECK(entry.has_input_current_probe());
}

/// Edge case: all three handles set to the confirmed "absent" sentinel
/// (0xFFFF), rather than merely omitted from the buffer. has_*() must
/// distinguish this from a real handle, even though the field is
/// physically present.
BOOST_AUTO_TEST_CASE(AllHandlesAbsentSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_full_formatted_area(
        0x00, 100, 0x0000,
        SystemPowerSupplyEntry::HandleAbsent, SystemPowerSupplyEntry::HandleAbsent, SystemPowerSupplyEntry::HandleAbsent);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x16));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerSupply, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemPowerSupplyEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_input_voltage_probe_handle(), SystemPowerSupplyEntry::HandleAbsent);
    BOOST_CHECK(!entry.has_input_voltage_probe());
    BOOST_CHECK_EQUAL(entry.get_cooling_device_handle(), SystemPowerSupplyEntry::HandleAbsent);
    BOOST_CHECK(!entry.has_cooling_device());
    BOOST_CHECK_EQUAL(entry.get_input_current_probe_handle(), SystemPowerSupplyEntry::HandleAbsent);
    BOOST_CHECK(!entry.has_input_current_probe());
}

/// Edge case: Max Power Capacity unknown sentinel (0x8000).
BOOST_AUTO_TEST_CASE(MaxPowerCapacityUnknownTestCase)
{
    std::vector<uint8_t> formatted_area = make_minimum_formatted_area(
        0x00, SystemPowerSupplyEntry::MaxPowerCapacityUnknown, 0x0000);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x10));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerSupply, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemPowerSupplyEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_max_power_capacity(), SystemPowerSupplyEntry::MaxPowerCapacityUnknown);
    BOOST_CHECK(!entry.is_max_power_capacity_known());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_minimum_formatted_area(0x00, 0, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(SystemPowerSupplyEntry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size()), std::runtime_error);
}

/// SystemPowerSupply resolves through SMBiosEntryFactory::create() to a
/// real SystemPowerSupplyEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemPowerSupplyEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_minimum_formatted_area(0x00, 0, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerSupply, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemPowerSupplyEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009, Story 01.0, Task 04)
//
// Type: unit - deterministic, hand-built buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description() field by field;
// Unplugged/Present/Hot-replaceable are bool, Power Unit Group and Max
// Power Capacity stay the printed decorated strings, and each optional
// "0x" + hex handle field is present exactly when the text prints it. No
// bitmask list in this type (Characteristics is three enumerations plus
// three individually printed flags).
// Boundaries: one handle at the 0xFFFF absent sentinel, a minimum-length
// (0x10) structure with no handle bytes, the 0x8000 unknown capacity.
// On failure first check: SystemPowerSupplyEntry::get_fields() in
// libsmbios/src/system_power_supply_entry.cpp.

/// Normal case: every text label becomes a field, in text order; the
/// sentinel current-probe handle is omitted exactly as in the text.
BOOST_AUTO_TEST_CASE(SystemPowerSupplyEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    // characteristics = Switching | OK | Auto-switch | unplugged | hot-replaceable
    std::vector<uint8_t> formatted_area = make_full_formatted_area(
        0x05, 750, 0x11A5, 0x0021, 0x00AB, SystemPowerSupplyEntry::HandleAbsent);
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerSupply, formatted_area,
                          {"Left Supply Bay", "DR-36", "Acme", "SN123", "AT001", "PN456", "2.30"});
    std::vector<uint8_t> buffer = builder.build();
    SystemPowerSupplyEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Location", str("Left Supply Bay")},
        {"Device Name", str("DR-36")},
        {"Manufacturer", str("Acme")},
        {"Serial Number", str("SN123")},
        {"Asset Tag", str("AT001")},
        {"Model Part Number", str("PN456")},
        {"Revision Level", str("2.30")},
        {"Power Unit Group", str("5 (redundant)")},
        {"Max Power Capacity", str("750 W")},
        {"Power Supply Type", str("Switching")},
        {"Status", str("OK")},
        {"Input Voltage Range Switching", str("Auto-switch")},
        {"Unplugged", true},
        {"Present", false},
        {"Hot-replaceable", true},
        {"Input Voltage Probe Handle", str("0x21")},
        {"Cooling Device Handle", str("0xab")},
    });
}

/// Boundary case: a minimum-length (0x10) structure carries no handle
/// bytes, so no handle field appears; an unknown capacity prints "Unknown".
BOOST_AUTO_TEST_CASE(SystemPowerSupplyEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = make_minimum_formatted_area(
        0x00, SystemPowerSupplyEntry::MaxPowerCapacityUnknown, 0x11A3);
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemPowerSupply, formatted_area,
                          {"Left Supply Bay", "DR-36", "Acme", "SN123", "AT001", "PN456", "2.30"});
    std::vector<uint8_t> buffer = builder.build();
    SystemPowerSupplyEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Location", str("Left Supply Bay")},
        {"Device Name", str("DR-36")},
        {"Manufacturer", str("Acme")},
        {"Serial Number", str("SN123")},
        {"Asset Tag", str("AT001")},
        {"Model Part Number", str("PN456")},
        {"Revision Level", str("2.30")},
        {"Power Unit Group", str("0 (not redundant)")},
        {"Max Power Capacity", str("Unknown")},
        {"Power Supply Type", str("Switching")},
        {"Status", str("OK")},
        {"Input Voltage Range Switching", str("Auto-switch")},
        {"Unplugged", false},
        {"Present", true},
        {"Hot-replaceable", true},
    });
}

BOOST_AUTO_TEST_SUITE_END()
