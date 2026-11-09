#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/voltage_probe_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive VoltageProbeEntry
// (smbios/include/smbios/tables/voltage_probe_entry.h), cross-checked
// against DSP0134 3.9.0's own 7.27 (Table 95/96) directly - including the
// 0x14 Nominal Value field, which this task's own roadmap document omits
// from Voltage Probe's field list but which Table 95 as it actually reads
// in docs/standard/DSP0134_3.9.0.md shows present (see
// voltage_probe_entry.h's own class doc comment for the full discrepancy
// note).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  description string index
//   offset 0x05 : uint8_t  location and status
//   offset 0x06 : uint16_t maximum value
//   offset 0x08 : uint16_t minimum value
//   offset 0x0A : uint16_t resolution
//   offset 0x0C : uint16_t tolerance
//   offset 0x0E : uint16_t accuracy
//   offset 0x10 : uint32_t oem-defined
//   offset 0x14 : uint16_t nominal value (present only if length >= 0x16)

BOOST_AUTO_TEST_SUITE(VoltageProbeEntryUnitTests)

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

/// Builds a formatted area through OEM-defined (0x14 bytes total, the
/// documented minimum length), with description string index fixed at 1.
std::vector<uint8_t> build_base_formatted_area(uint8_t location_and_status,
                                                uint16_t maximum_value, uint16_t minimum_value,
                                                uint16_t resolution, uint16_t tolerance,
                                                uint16_t accuracy, uint32_t oem_defined)
{
    return std::vector<uint8_t>{
        1, // 0x04 Description string index
        location_and_status, // 0x05
        static_cast<uint8_t>(maximum_value & 0xFF), static_cast<uint8_t>((maximum_value >> 8) & 0xFF), // 0x06-0x07
        static_cast<uint8_t>(minimum_value & 0xFF), static_cast<uint8_t>((minimum_value >> 8) & 0xFF), // 0x08-0x09
        static_cast<uint8_t>(resolution & 0xFF), static_cast<uint8_t>((resolution >> 8) & 0xFF), // 0x0A-0x0B
        static_cast<uint8_t>(tolerance & 0xFF), static_cast<uint8_t>((tolerance >> 8) & 0xFF), // 0x0C-0x0D
        static_cast<uint8_t>(accuracy & 0xFF), static_cast<uint8_t>((accuracy >> 8) & 0xFF), // 0x0E-0x0F
        static_cast<uint8_t>(oem_defined & 0xFF), static_cast<uint8_t>((oem_defined >> 8) & 0xFF),
        static_cast<uint8_t>((oem_defined >> 16) & 0xFF), static_cast<uint8_t>((oem_defined >> 24) & 0xFF), // 0x10-0x13
    };
}

} // namespace

/// Normal case: minimum-length buffer (0x14), known values round-trip.
BOOST_AUTO_TEST_CASE(MinimumLengthBufferKnownValuesTestCase)
{
    // Status = OK (011), Location = Processor (00011) -> 0b01100011 = 0x63.
    const uint8_t location_and_status = static_cast<uint8_t>((VoltageProbeEntry::StatusOk << 5) | VoltageProbeEntry::LocationProcessor);
    std::vector<uint8_t> formatted_area = build_base_formatted_area(location_and_status, 5000, 3000, 10, 50, 100, 0xDEADBEEF);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area, {"Main Voltage Probe"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), "Main Voltage Probe");
    BOOST_CHECK_EQUAL(entry.get_status(), VoltageProbeEntry::StatusOk);
    BOOST_CHECK_EQUAL(entry.get_status_string(), "OK");
    BOOST_CHECK_EQUAL(entry.get_location(), VoltageProbeEntry::LocationProcessor);
    BOOST_CHECK_EQUAL(entry.get_location_string(), "Processor");
    BOOST_CHECK(entry.is_maximum_value_known());
    BOOST_CHECK_EQUAL(entry.get_maximum_value(), static_cast<uint16_t>(5000));
    BOOST_CHECK(entry.is_minimum_value_known());
    BOOST_CHECK_EQUAL(entry.get_minimum_value(), static_cast<uint16_t>(3000));
    BOOST_CHECK(entry.is_resolution_known());
    BOOST_CHECK_EQUAL(entry.get_resolution(), static_cast<uint16_t>(10));
    BOOST_CHECK(entry.is_tolerance_known());
    BOOST_CHECK_EQUAL(entry.get_tolerance(), static_cast<uint16_t>(50));
    BOOST_CHECK(entry.is_accuracy_known());
    BOOST_CHECK_EQUAL(entry.get_accuracy(), static_cast<uint16_t>(100));
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0xDEADBEEF));
    BOOST_CHECK(!entry.has_nominal_value());
    BOOST_CHECK(!entry.is_nominal_value_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_value(), VoltageProbeEntry::ValueUnknown);
}

/// Normal case: structure extended to include Nominal Value (0x16 bytes),
/// known value round-trips.
BOOST_AUTO_TEST_CASE(NominalValuePresentKnownTestCase)
{
    const uint8_t location_and_status = static_cast<uint8_t>((VoltageProbeEntry::StatusOk << 5) | VoltageProbeEntry::LocationMotherboard);
    std::vector<uint8_t> formatted_area = build_base_formatted_area(location_and_status, 5000, 3000, 10, 50, 100, 0);
    formatted_area.push_back(static_cast<uint8_t>(3300 & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((3300 >> 8) & 0xFF));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.has_nominal_value());
    BOOST_CHECK(entry.is_nominal_value_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_value(), static_cast<uint16_t>(3300));
}

/// Edge case: extended structure (0x16 bytes) with Nominal Value reading
/// the 0x8000 unknown sentinel.
BOOST_AUTO_TEST_CASE(NominalValueUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, 4, 5, 0);
    formatted_area.push_back(static_cast<uint8_t>(VoltageProbeEntry::ValueUnknown & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((VoltageProbeEntry::ValueUnknown >> 8) & 0xFF));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.has_nominal_value());
    BOOST_CHECK(!entry.is_nominal_value_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_value(), VoltageProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Maximum Value == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(MaximumValueUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, VoltageProbeEntry::ValueUnknown, 2, 3, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_maximum_value_known());
    BOOST_CHECK_EQUAL(entry.get_maximum_value(), VoltageProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Minimum Value == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(MinimumValueUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, VoltageProbeEntry::ValueUnknown, 3, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_minimum_value_known());
    BOOST_CHECK_EQUAL(entry.get_minimum_value(), VoltageProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Resolution == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(ResolutionUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, VoltageProbeEntry::ValueUnknown, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_resolution_known());
    BOOST_CHECK_EQUAL(entry.get_resolution(), VoltageProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Tolerance == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(ToleranceUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, VoltageProbeEntry::ValueUnknown, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_tolerance_known());
    BOOST_CHECK_EQUAL(entry.get_tolerance(), VoltageProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Accuracy == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(AccuracyUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, 4, VoltageProbeEntry::ValueUnknown, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_accuracy_known());
    BOOST_CHECK_EQUAL(entry.get_accuracy(), VoltageProbeEntry::ValueUnknown);
}

/// Edge case: minimum-length buffer with zero-length formatted area
/// (header.length == 0x04) -> every getter defaults safely.
BOOST_AUTO_TEST_CASE(EmptyFormattedAreaDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    VoltageProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), "");
    BOOST_CHECK_EQUAL(entry.get_location_and_status(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_status(), VoltageProbeEntry::StatusOutOfSpec);
    BOOST_CHECK_EQUAL(entry.get_location(), VoltageProbeEntry::LocationOutOfSpec);
    BOOST_CHECK(!entry.is_maximum_value_known());
    BOOST_CHECK_EQUAL(entry.get_maximum_value(), VoltageProbeEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_minimum_value_known());
    BOOST_CHECK_EQUAL(entry.get_minimum_value(), VoltageProbeEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_resolution_known());
    BOOST_CHECK_EQUAL(entry.get_resolution(), VoltageProbeEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_tolerance_known());
    BOOST_CHECK_EQUAL(entry.get_tolerance(), VoltageProbeEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_accuracy_known());
    BOOST_CHECK_EQUAL(entry.get_accuracy(), VoltageProbeEntry::ValueUnknown);
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0));
    BOOST_CHECK(!entry.has_nominal_value());
    BOOST_CHECK(!entry.is_nominal_value_known());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::TemperatureProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(VoltageProbeEntry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 04.0 Task 03's own success criterion: VoltageProbe
/// resolves through SMBiosEntryFactory::create() to a real
/// VoltageProbeEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsVoltageProbeEntryTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<VoltageProbeEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
