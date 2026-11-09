#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/portable_battery_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive PortableBatteryEntry
// (smbios/include/smbios/tables/portable_battery_entry.h), cross-checked
// against DSP0134 3.9.0's own 7.23/7.23.1 (Table 90/91):
//
//   - a normal-case full buffer (including the 2.2+ 0x15h Design Capacity
//     Multiplier field) decoding every documented Device Chemistry (Table
//     91) value plus the Design Capacity Multiplier resolution itself
//   - the SBDS-indicator (string index 0) cases for Manufacture Date and
//     Serial Number as DISTINCT test cases from the normal-string case
//   - the unknown sentinels for Design Capacity (0)/Design Voltage
//     (0)/Maximum Error (0xFF), each as a distinct test case
//   - an edge-case minimum-length buffer -> every getter defaults
//   - an invalid-input case: an unrecognized Device Chemistry byte
//
// Wire layout (see the header's "@brief 0xNN offset" getter comments; the
// SBDS Serial Number/Manufacture Date/Device Chemistry fields at 0x10-0x14
// are not decoded by this class, only padded here to reach the 0x15
// multiplier/0x16 OEM-specific tail for full-buffer tests):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  location_index
//   offset 0x05 : uint8_t  manufacturer_index
//   offset 0x06 : uint8_t  manufacture_date_index
//   offset 0x07 : uint8_t  serial_number_index
//   offset 0x08 : uint8_t  device_name_index
//   offset 0x09 : uint8_t  device_chemistry
//   offset 0x0A : uint16_t design_capacity (raw, pre-multiplication)
//   offset 0x0C : uint16_t design_voltage
//   offset 0x0E : uint8_t  sbds_version_index
//   offset 0x0F : uint8_t  maximum_error
//   offset 0x10 : uint16_t sbds_serial_number (not decoded)
//   offset 0x12 : uint16_t sbds_manufacture_date (not decoded)
//   offset 0x14 : uint8_t  sbds_device_chemistry_index (not decoded)
//   offset 0x15 : uint8_t  design_capacity_multiplier
//   offset 0x16 : uint32_t oem_specific (not decoded)

BOOST_AUTO_TEST_SUITE(PortableBatteryEntryUnitTests)

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

/// Builds a full (0x16-byte formatted area, header.length == 0x1A) Portable
/// Battery structure, with the given design capacity multiplier at 0x15 and
/// chemistry at 0x09, manufacture date / serial number indices as given.
/// Strings referenced, in index order: Location(1), Manufacturer(2),
/// ManufactureDate(3), SerialNumber(4), DeviceName(5), SbdsVersion(6).
std::vector<uint8_t> build_full_formatted_area(uint8_t chemistry, uint16_t design_capacity,
                                                uint16_t design_voltage, uint8_t maximum_error,
                                                uint8_t multiplier, uint8_t manufacture_date_index,
                                                uint8_t serial_number_index)
{
    return std::vector<uint8_t>{
        0x01, // 0x04 location_index
        0x02, // 0x05 manufacturer_index
        manufacture_date_index, // 0x06 manufacture_date_index
        serial_number_index, // 0x07 serial_number_index
        0x05, // 0x08 device_name_index
        chemistry, // 0x09 device_chemistry
        static_cast<uint8_t>(design_capacity & 0xFF), static_cast<uint8_t>((design_capacity >> 8) & 0xFF), // 0x0A-0x0B design_capacity
        static_cast<uint8_t>(design_voltage & 0xFF), static_cast<uint8_t>((design_voltage >> 8) & 0xFF), // 0x0C-0x0D design_voltage
        0x06, // 0x0E sbds_version_index
        maximum_error, // 0x0F maximum_error
        0x00, 0x00, // 0x10-0x11 sbds_serial_number (not decoded)
        0x00, 0x00, // 0x12-0x13 sbds_manufacture_date (not decoded)
        0x00, // 0x14 sbds_device_chemistry_index (not decoded)
        multiplier, // 0x15 design_capacity_multiplier
        0x00, 0x00, 0x00, 0x00, // 0x16-0x19 oem_specific (not decoded)
    };
}

} // namespace

/// Normal case: full buffer, every documented Device Chemistry (Table 91)
/// value decodes to its own enum/string.
BOOST_AUTO_TEST_CASE(EveryDocumentedDeviceChemistryValueDecodesTestCase)
{
    const std::vector<std::pair<uint8_t, std::string>> cases{
        {PortableBatteryEntry::Other, "Other"},
        {PortableBatteryEntry::Unknown, "Unknown"},
        {PortableBatteryEntry::LeadAcid, "Lead Acid"},
        {PortableBatteryEntry::NickelCadmium, "Nickel Cadmium"},
        {PortableBatteryEntry::NickelMetalHydride, "Nickel metal hydride"},
        {PortableBatteryEntry::LithiumIon, "Lithium-ion"},
        {PortableBatteryEntry::ZincAir, "Zinc air"},
        {PortableBatteryEntry::LithiumPolymer, "Lithium Polymer"},
    };

    for (const auto& chemistry_case : cases) {
        std::vector<uint8_t> formatted_area =
            build_full_formatted_area(chemistry_case.first, 5000, 11100, 10, 1, 0x03, 0x04);

        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::PortableBattery, formatted_area,
                               std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.get_device_chemistry(), chemistry_case.first);
        BOOST_CHECK_EQUAL(entry.get_device_chemistry_string(), chemistry_case.second);
    }
}

/// Normal case: full buffer with normal (non-zero) Manufacture Date /
/// Serial Number string indices and a design capacity multiplier of 1 -
/// baseline against which the SBDS-indicator cases below are distinct.
BOOST_AUTO_TEST_CASE(NormalStringManufactureDateAndSerialNumberDecodeTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::LithiumIon, 5000, 11100, 10, 1, 0x03, 0x04);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.uses_sbds_manufacture_date());
    BOOST_CHECK_EQUAL(entry.get_manufacture_date_string(), std::string("2024-01-01"));
    BOOST_CHECK(!entry.uses_sbds_serial_number());
    BOOST_CHECK_EQUAL(entry.get_serial_number_string(), std::string("SN123"));

    BOOST_CHECK_EQUAL(entry.get_location_string(), std::string("In the back"));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_string(), std::string("Acme"));
    BOOST_CHECK_EQUAL(entry.get_device_name_string(), std::string("DR-36"));
    BOOST_CHECK_EQUAL(entry.get_sbds_version_string(), std::string("1.1"));
}

/// Distinct SBDS-indicator case: Manufacture Date Index == 0, the
/// documented 2.2+ Smart Battery convention meaning "see SBDS Manufacture
/// Date instead" - distinct from the normal-string case above.
BOOST_AUTO_TEST_CASE(ManufactureDateIndexZeroIndicatesSbdsTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::Unknown, 5000, 11100, 10, 1, 0x00, 0x03);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.uses_sbds_manufacture_date());
    BOOST_CHECK_EQUAL(entry.get_manufacture_date_string(), std::string());
}

/// Distinct SBDS-indicator case: Serial Number Index == 0 - distinct from
/// the normal-string case above.
BOOST_AUTO_TEST_CASE(SerialNumberIndexZeroIndicatesSbdsTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::Unknown, 5000, 11100, 10, 1, 0x03, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.uses_sbds_serial_number());
    BOOST_CHECK_EQUAL(entry.get_serial_number_string(), std::string());
}

/// Distinct unknown-sentinel case: Design Capacity raw field == 0 ->
/// unknown, regardless of multiplier.
BOOST_AUTO_TEST_CASE(DesignCapacityZeroIsUnknownTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::LeadAcid, 0, 11100, 10, 10, 0x03, 0x04);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_design_capacity_known());
    BOOST_CHECK_EQUAL(entry.get_design_capacity(), static_cast<uint32_t>(0));
}

/// Distinct unknown-sentinel case: Design Voltage field == 0 -> unknown.
BOOST_AUTO_TEST_CASE(DesignVoltageZeroIsUnknownTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::LeadAcid, 5000, 0, 10, 1, 0x03, 0x04);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_design_voltage_known());
    BOOST_CHECK_EQUAL(entry.get_design_voltage(), static_cast<uint16_t>(0));
}

/// Distinct unknown-sentinel case: Maximum Error == 0xFF -> unknown.
BOOST_AUTO_TEST_CASE(MaximumErrorFFIsUnknownTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::LeadAcid, 5000, 11100,
                                   PortableBatteryEntry::MaximumErrorUnknown, 1, 0x03, 0x04);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_maximum_error_known());
    BOOST_CHECK_EQUAL(entry.get_maximum_error_percent(), PortableBatteryEntry::MaximumErrorUnknown);
}

/// Normal case: the Design Capacity Multiplier field (0x15h, 2.2+) IS a
/// separate structure field per DSP0134 3.9.0 Table 90 - see the class doc
/// comment's resolution note. A raw capacity of 500 with multiplier 10
/// (the SBDS convention value per spec) must report an effective capacity
/// of 5000 mWh, not the raw 500.
BOOST_AUTO_TEST_CASE(DesignCapacityMultiplierAppliesWhenFieldPresentTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::LithiumIon, 500, 11100, 10, 10, 0x03, 0x04);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.is_design_capacity_known());
    BOOST_CHECK_EQUAL(entry.get_design_capacity(), static_cast<uint32_t>(5000));
}

/// Edge case: the structure is too short to carry the 0x15h multiplier
/// field at all (length < 0x16) - the multiplier must default to 1 (the
/// spec's own documented default), so the raw and effective values match.
BOOST_AUTO_TEST_CASE(DesignCapacityMultiplierDefaultsToOneWhenFieldAbsentTestCase)
{
    // 2.1-shaped structure: only through Maximum Error (0x0F), length 0x10.
    std::vector<uint8_t> formatted_area{
        0x01, // 0x04 location_index
        0x02, // 0x05 manufacturer_index
        0x03, // 0x06 manufacture_date_index
        0x04, // 0x07 serial_number_index
        0x05, // 0x08 device_name_index
        static_cast<uint8_t>(PortableBatteryEntry::LeadAcid), // 0x09 device_chemistry
        0x88, 0x13, // 0x0A-0x0B design_capacity = 5000
        0x5C, 0x2B, // 0x0C-0x0D design_voltage = 11100
        0x06, // 0x0E sbds_version_index
        0x0A, // 0x0F maximum_error = 10
    };
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x10));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.is_design_capacity_known());
    BOOST_CHECK_EQUAL(entry.get_design_capacity(), static_cast<uint32_t>(5000));
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults (empty strings, OutOfSpec
/// chemistry, unknown sentinels, SBDS-indicator predicates true since the
/// index defaults to 0).
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_location_string(), std::string());
    BOOST_CHECK_EQUAL(entry.get_manufacturer_string(), std::string());
    BOOST_CHECK(entry.uses_sbds_manufacture_date());
    BOOST_CHECK_EQUAL(entry.get_manufacture_date_string(), std::string());
    BOOST_CHECK(entry.uses_sbds_serial_number());
    BOOST_CHECK_EQUAL(entry.get_serial_number_string(), std::string());
    BOOST_CHECK_EQUAL(entry.get_device_name_string(), std::string());
    BOOST_CHECK_EQUAL(entry.get_device_chemistry(), static_cast<uint8_t>(PortableBatteryEntry::ChemistryOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_device_chemistry_string(), std::string("OutOfSpec"));
    BOOST_CHECK(!entry.is_design_capacity_known());
    BOOST_CHECK_EQUAL(entry.get_design_capacity(), static_cast<uint32_t>(0));
    BOOST_CHECK(!entry.is_design_voltage_known());
    BOOST_CHECK_EQUAL(entry.get_design_voltage(), static_cast<uint16_t>(0));
    BOOST_CHECK(!entry.is_maximum_error_known());
    BOOST_CHECK_EQUAL(entry.get_maximum_error_percent(), PortableBatteryEntry::MaximumErrorUnknown);
    BOOST_CHECK_EQUAL(entry.get_sbds_version_string(), std::string());
}

/// Invalid-input case: Device Chemistry byte not present in Table 91 ->
/// OutOfSpec, not a crash or a bogus enum value.
BOOST_AUTO_TEST_CASE(UnrecognizedDeviceChemistryValueReportsOutOfSpecTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(0x7F /* not in Table 91 */, 5000, 11100, 10, 1, 0x03, 0x04);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PortableBatteryEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_device_chemistry(), static_cast<uint8_t>(PortableBatteryEntry::ChemistryOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_device_chemistry_string(), std::string("OutOfSpec"));
}

/// Milestone 0010 Story 04.0 Task 01's own success criterion:
/// PortableBattery resolves through SMBiosEntryFactory::create() to a real
/// PortableBatteryEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsPortableBatteryEntryTestCase)
{
    std::vector<uint8_t> formatted_area =
        build_full_formatted_area(PortableBatteryEntry::LithiumIon, 5000, 11100, 10, 1, 0x03, 0x04);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PortableBattery, formatted_area,
                           std::vector<std::string>{"In the back", "Acme", "2024-01-01", "SN123", "DR-36", "1.1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<PortableBatteryEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
