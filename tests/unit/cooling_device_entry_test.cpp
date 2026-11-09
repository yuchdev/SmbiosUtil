#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/cooling_device_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive CoolingDeviceEntry
// (smbios/include/smbios/tables/cooling_device_entry.h), cross-checked
// against DSP0134 3.9.0's own 7.28 (Table 97/98) directly. Covers all
// three documented length tiers for the structure's two independent
// trailing-field length gates: 0x0C (neither Nominal Speed nor
// Description), 0x0D/0x0E (Nominal Speed only - see
// CoolingDeviceEntry::has_nominal_speed()'s own doc comment for why 0x0D
// specifically still reports "not yet present", an edge case distinct from
// 0x0E's "fully present"), and >= 0x0F (both present).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint16_t temperature probe handle
//   offset 0x06 : uint8_t  device type and status
//   offset 0x07 : uint8_t  cooling unit group
//   offset 0x08 : uint32_t oem-defined
//   offset 0x0C : uint16_t nominal speed (present only if length >= 0x0E)
//   offset 0x0E : uint8_t  description string index (present only if
//                 length >= 0x0F)

BOOST_AUTO_TEST_SUITE(CoolingDeviceEntryUnitTests)

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

/// Builds a formatted area through OEM-defined (0x0C bytes total, the
/// documented minimum length).
std::vector<uint8_t> build_base_formatted_area(uint16_t temperature_probe_handle,
                                                uint8_t device_type_and_status,
                                                uint8_t cooling_unit_group,
                                                uint32_t oem_defined)
{
    return std::vector<uint8_t>{
        static_cast<uint8_t>(temperature_probe_handle & 0xFF), static_cast<uint8_t>((temperature_probe_handle >> 8) & 0xFF), // 0x04-0x05
        device_type_and_status, // 0x06
        cooling_unit_group, // 0x07
        static_cast<uint8_t>(oem_defined & 0xFF), static_cast<uint8_t>((oem_defined >> 8) & 0xFF),
        static_cast<uint8_t>((oem_defined >> 16) & 0xFF), static_cast<uint8_t>((oem_defined >> 24) & 0xFF), // 0x08-0x0B
    };
}

} // namespace

/// Normal case: minimum-length buffer (0x0C), known values round-trip,
/// Cooling Unit Group == 0 means not redundant.
BOOST_AUTO_TEST_CASE(MinimumLengthBufferKnownValuesTestCase)
{
    const uint8_t device_type_and_status = static_cast<uint8_t>((CoolingDeviceEntry::StatusOk << 5) | CoolingDeviceEntry::DeviceTypeFan);
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, device_type_and_status, 0, 0xABCD1234);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    CoolingDeviceEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_temperature_probe_handle(), static_cast<uint16_t>(0x0010));
    BOOST_CHECK_EQUAL(entry.get_status(), CoolingDeviceEntry::StatusOk);
    BOOST_CHECK_EQUAL(entry.get_device_type(), CoolingDeviceEntry::DeviceTypeFan);
    BOOST_CHECK_EQUAL(entry.get_device_type_string(), "Fan");
    BOOST_CHECK_EQUAL(entry.get_cooling_unit_group(), static_cast<uint8_t>(0));
    BOOST_CHECK(!entry.is_redundant());
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0xABCD1234));
}

/// Normal case: non-zero Cooling Unit Group means redundant.
BOOST_AUTO_TEST_CASE(NonZeroCoolingUnitGroupIsRedundantTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 2, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    CoolingDeviceEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_cooling_unit_group(), static_cast<uint8_t>(2));
    BOOST_CHECK(entry.is_redundant());
}

/// Length tier 1 of 3: Length == 0x0C -> neither Nominal Speed nor
/// Description present.
BOOST_AUTO_TEST_CASE(LengthTierBaseNeitherTrailingFieldPresentTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 0, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    BOOST_REQUIRE_EQUAL(buffer[1], static_cast<uint8_t>(0x0C));

    DMIHeader header = make_header(buffer);
    CoolingDeviceEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.has_nominal_speed());
    BOOST_CHECK(!entry.is_nominal_speed_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_speed(), CoolingDeviceEntry::NominalSpeedUnknown);
    BOOST_CHECK(!entry.has_description());
    BOOST_CHECK_EQUAL(entry.get_description_string(), "");
}

/// Length tier 2 of 3, boundary a: Length == 0x0D -> Nominal Speed's WORD
/// is only half-present (only its low byte at 0x0C falls within the
/// formatted area) - see has_nominal_speed()'s own doc comment for why
/// this class defensively treats this as "not yet present" rather than
/// reading one byte of the adjacent string section as the field's high
/// byte. Description is absent.
BOOST_AUTO_TEST_CASE(LengthTierIncompleteNominalSpeedNotPresentTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 0, 0);
    formatted_area.push_back(0x34); // single extra byte -> length 0x0D

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    BOOST_REQUIRE_EQUAL(buffer[1], static_cast<uint8_t>(0x0D));

    DMIHeader header = make_header(buffer);
    CoolingDeviceEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.has_nominal_speed());
    BOOST_CHECK(!entry.is_nominal_speed_known());
    BOOST_CHECK(!entry.has_description());
}

/// Length tier 2 of 3, boundary b: Length == 0x0E -> Nominal Speed is
/// fully present (both bytes), Description is still absent - the task's
/// own explicitly required "Nominal Speed present but Description absent"
/// intermediate-length case.
BOOST_AUTO_TEST_CASE(LengthTierNominalSpeedPresentDescriptionAbsentTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 0, 0);
    formatted_area.push_back(static_cast<uint8_t>(3500 & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((3500 >> 8) & 0xFF));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    BOOST_REQUIRE_EQUAL(buffer[1], static_cast<uint8_t>(0x0E));

    DMIHeader header = make_header(buffer);
    CoolingDeviceEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.has_nominal_speed());
    BOOST_CHECK(entry.is_nominal_speed_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_speed(), static_cast<uint16_t>(3500));
    BOOST_CHECK(!entry.has_description());
    BOOST_CHECK_EQUAL(entry.get_description_string(), "");
}

/// Edge case: Nominal Speed reads the 0x8000 unknown/non-rotating
/// sentinel, at the fully-present length tier (0x0E).
BOOST_AUTO_TEST_CASE(NominalSpeedUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 0, 0);
    formatted_area.push_back(static_cast<uint8_t>(CoolingDeviceEntry::NominalSpeedUnknown & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((CoolingDeviceEntry::NominalSpeedUnknown >> 8) & 0xFF));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    CoolingDeviceEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.has_nominal_speed());
    BOOST_CHECK(!entry.is_nominal_speed_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_speed(), CoolingDeviceEntry::NominalSpeedUnknown);
}

/// Length tier 3 of 3: Length >= 0x0F -> both Nominal Speed and
/// Description present.
BOOST_AUTO_TEST_CASE(LengthTierBothTrailingFieldsPresentTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 0, 0);
    formatted_area.push_back(static_cast<uint8_t>(3500 & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((3500 >> 8) & 0xFF));
    formatted_area.push_back(1); // 0x0E Description string index

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area, {"Front Chassis Fan"});
    std::vector<uint8_t> buffer = builder.build();

    BOOST_REQUIRE_EQUAL(buffer[1], static_cast<uint8_t>(0x0F));

    DMIHeader header = make_header(buffer);
    CoolingDeviceEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.has_nominal_speed());
    BOOST_CHECK(entry.is_nominal_speed_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_speed(), static_cast<uint16_t>(3500));
    BOOST_CHECK(entry.has_description());
    BOOST_CHECK_EQUAL(entry.get_description_string(), "Front Chassis Fan");
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 0, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(CoolingDeviceEntry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 04.0 Task 03's own success criterion: CoolingDevice
/// resolves through SMBiosEntryFactory::create() to a real
/// CoolingDeviceEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsCoolingDeviceEntryTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x0010, 0x63, 0, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::CoolingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<CoolingDeviceEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
