#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/electrical_current_probe_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive ElectricalCurrentProbeEntry
// (smbios/include/smbios/tables/electrical_current_probe_entry.hpp),
// cross-checked against DSP0134 3.9.0's own 7.30 (Table 101/102) directly,
// including confirming its Location (bits 4:0) is byte-identical to
// VoltageProbeEntry's own 11-value Location, NOT TemperatureProbeEntry's
// 15-value superset.
//
// Wire layout identical to VoltageProbeEntry's/TemperatureProbeEntry's,
// offsets relative to structure start: 0x04 description index, 0x05
// location and status, 0x06/0x08/0x0A/0x0C/0x0E WORD fields, 0x10 DWORD
// oem-defined, 0x14 WORD nominal value (present only if length >= 0x16).

BOOST_AUTO_TEST_SUITE(ElectricalCurrentProbeEntryUnitTests)

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
    const uint8_t location_and_status = static_cast<uint8_t>((ElectricalCurrentProbeEntry::StatusOk << 5) | ElectricalCurrentProbeEntry::LocationPowerUnit);
    std::vector<uint8_t> formatted_area = build_base_formatted_area(location_and_status, 2000, 0, 10, 50, 100, 0x1234);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area, {"12V Rail Current"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), "12V Rail Current");
    BOOST_CHECK_EQUAL(entry.get_status(), ElectricalCurrentProbeEntry::StatusOk);
    BOOST_CHECK_EQUAL(entry.get_location(), ElectricalCurrentProbeEntry::LocationPowerUnit);
    BOOST_CHECK(entry.is_maximum_value_known());
    BOOST_CHECK_EQUAL(entry.get_maximum_value(), static_cast<uint16_t>(2000));
    BOOST_CHECK(entry.is_minimum_value_known());
    BOOST_CHECK_EQUAL(entry.get_minimum_value(), static_cast<uint16_t>(0));
    BOOST_CHECK(entry.is_resolution_known());
    BOOST_CHECK_EQUAL(entry.get_resolution(), static_cast<uint16_t>(10));
    BOOST_CHECK(entry.is_tolerance_known());
    BOOST_CHECK_EQUAL(entry.get_tolerance(), static_cast<uint16_t>(50));
    BOOST_CHECK(entry.is_accuracy_known());
    BOOST_CHECK_EQUAL(entry.get_accuracy(), static_cast<uint16_t>(100));
    BOOST_CHECK_EQUAL(entry.get_oem_defined(), static_cast<uint32_t>(0x1234));
    BOOST_CHECK(!entry.has_nominal_value());
}

/// Normal case: structure extended to include Nominal Value (0x16 bytes),
/// known value round-trips - task's own required accessor trio
/// (get_nominal_value()/is_nominal_value_known()/has_nominal_value()).
BOOST_AUTO_TEST_CASE(NominalValuePresentKnownTestCase)
{
    const uint8_t location_and_status = static_cast<uint8_t>((ElectricalCurrentProbeEntry::StatusOk << 5) | ElectricalCurrentProbeEntry::LocationMotherboard);
    std::vector<uint8_t> formatted_area = build_base_formatted_area(location_and_status, 2000, 0, 10, 50, 100, 0);
    formatted_area.push_back(static_cast<uint8_t>(1500 & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((1500 >> 8) & 0xFF));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.has_nominal_value());
    BOOST_CHECK(entry.is_nominal_value_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_value(), static_cast<uint16_t>(1500));
}

/// Edge case: extended structure (0x16 bytes) with Nominal Value reading
/// the 0x8000 unknown sentinel.
BOOST_AUTO_TEST_CASE(NominalValueUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, 4, 5, 0);
    formatted_area.push_back(static_cast<uint8_t>(ElectricalCurrentProbeEntry::ValueUnknown & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((ElectricalCurrentProbeEntry::ValueUnknown >> 8) & 0xFF));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.has_nominal_value());
    BOOST_CHECK(!entry.is_nominal_value_known());
    BOOST_CHECK_EQUAL(entry.get_nominal_value(), ElectricalCurrentProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Maximum Value == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(MaximumValueUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, ElectricalCurrentProbeEntry::ValueUnknown, 2, 3, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_maximum_value_known());
    BOOST_CHECK_EQUAL(entry.get_maximum_value(), ElectricalCurrentProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Minimum Value == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(MinimumValueUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, ElectricalCurrentProbeEntry::ValueUnknown, 3, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_minimum_value_known());
    BOOST_CHECK_EQUAL(entry.get_minimum_value(), ElectricalCurrentProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Resolution == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(ResolutionUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, ElectricalCurrentProbeEntry::ValueUnknown, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_resolution_known());
    BOOST_CHECK_EQUAL(entry.get_resolution(), ElectricalCurrentProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Tolerance == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(ToleranceUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, ElectricalCurrentProbeEntry::ValueUnknown, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_tolerance_known());
    BOOST_CHECK_EQUAL(entry.get_tolerance(), ElectricalCurrentProbeEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Accuracy == 0x8000 -> unknown.
BOOST_AUTO_TEST_CASE(AccuracyUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, 4, ElectricalCurrentProbeEntry::ValueUnknown, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_accuracy_known());
    BOOST_CHECK_EQUAL(entry.get_accuracy(), ElectricalCurrentProbeEntry::ValueUnknown);
}

/// Edge case: minimum-length buffer with zero-length formatted area
/// (header.length == 0x04) -> every getter defaults safely.
BOOST_AUTO_TEST_CASE(EmptyFormattedAreaDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ElectricalCurrentProbeEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), "");
    BOOST_CHECK_EQUAL(entry.get_status(), ElectricalCurrentProbeEntry::StatusOutOfSpec);
    BOOST_CHECK_EQUAL(entry.get_location(), ElectricalCurrentProbeEntry::LocationOutOfSpec);
    BOOST_CHECK(!entry.is_maximum_value_known());
    BOOST_CHECK(!entry.is_minimum_value_known());
    BOOST_CHECK(!entry.is_resolution_known());
    BOOST_CHECK(!entry.is_tolerance_known());
    BOOST_CHECK(!entry.is_accuracy_known());
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
    builder.add_structure(SMBios::VoltageProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(ElectricalCurrentProbeEntry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 04.0 Task 03's own success criterion:
/// ElectricalCurrentProbe resolves through SMBiosEntryFactory::create() to
/// a real ElectricalCurrentProbeEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsElectricalCurrentProbeEntryTestCase)
{
    std::vector<uint8_t> formatted_area = build_base_formatted_area(0x63, 1, 2, 3, 4, 5, 0);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<ElectricalCurrentProbeEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009 Story 01.0 Task 05)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description() field by field:
// decorated "<value><unit>" / "Unknown" strings, Nominal Value only when
// the structure is long enough to carry it.
// Boundaries: 0x8000 sentinel, base (0x14) vs extended (0x16) length, empty
// formatted area. No bitmask in this type, so there is no ExpandsBitmask case.
// On failure first check: ElectricalCurrentProbeEntry::get_fields() against
// ElectricalCurrentProbeEntry::render_to_description() in libsmbios/src/electrical_current_probe_entry.cpp.

/// Normal case: extended (0x16) structure; every printed line becomes one
/// field, in text order, with the same decorated string the text prints.
BOOST_AUTO_TEST_CASE(ElectricalCurrentProbeEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    const uint8_t location_and_status = static_cast<uint8_t>((ElectricalCurrentProbeEntry::StatusOk << 5) | ElectricalCurrentProbeEntry::LocationProcessor);
    std::vector<uint8_t> formatted_area = build_base_formatted_area(location_and_status, 500, ElectricalCurrentProbeEntry::ValueUnknown, 10, 50, 100, 0xDEADBEEF);
    formatted_area.push_back(static_cast<uint8_t>(330 & 0xFF));
    formatted_area.push_back(static_cast<uint8_t>((330 >> 8) & 0xFF));

    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area, {"Main Current Probe"});
    std::vector<uint8_t> buffer = builder.build();
    ElectricalCurrentProbeEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Description", str("Main Current Probe")},
        {"Location", str("Processor")},
        {"Status", str("OK")},
        {"Maximum Value", str("500 mA")},
        {"Minimum Value", str("Unknown")},
        {"Resolution", str("10 (tenths of mA)")},
        {"Tolerance", str("50 mA")},
        {"Accuracy", str("100 (1/100th %)")},
        {"Nominal Value", str("330 mA")},
    });
}

/// Boundary cases: a base-length (0x14) structure has no Nominal Value
/// field (the text prints no line); an extended structure whose Nominal
/// Value is the 0x8000 sentinel keeps the field as "Unknown"; an empty
/// formatted area keeps every always-printed field at its default.
BOOST_AUTO_TEST_CASE(ElectricalCurrentProbeEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    const uint8_t location_and_status = static_cast<uint8_t>((ElectricalCurrentProbeEntry::StatusCritical << 5) | ElectricalCurrentProbeEntry::LocationMotherboard);
    {
        std::vector<uint8_t> formatted_area = build_base_formatted_area(location_and_status, 1, 2, 3, 4, 5, 0);
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area, {"Main Current Probe"});
        std::vector<uint8_t> buffer = builder.build();
        ElectricalCurrentProbeEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Description", str("Main Current Probe")},
            {"Location", str("Motherboard")},
            {"Status", str("Critical")},
            {"Maximum Value", str("1 mA")},
            {"Minimum Value", str("2 mA")},
            {"Resolution", str("3 (tenths of mA)")},
            {"Tolerance", str("4 mA")},
            {"Accuracy", str("5 (1/100th %)")},
        });
    }
    {
        std::vector<uint8_t> formatted_area = build_base_formatted_area(location_and_status, 1, 2, 3, 4, 5, 0);
        formatted_area.push_back(static_cast<uint8_t>(ElectricalCurrentProbeEntry::ValueUnknown & 0xFF));
        formatted_area.push_back(static_cast<uint8_t>((ElectricalCurrentProbeEntry::ValueUnknown >> 8) & 0xFF));
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ElectricalCurrentProbe, formatted_area, {"Main Current Probe"});
        std::vector<uint8_t> buffer = builder.build();
        ElectricalCurrentProbeEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

        const std::vector<NamedField> fields = entry.get_fields();
        BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(9));
        check_fields_equal({fields.back()}, {{"Nominal Value", str("Unknown")}});
        check_unique_field_names(fields);
    }
    {
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ElectricalCurrentProbe, std::vector<uint8_t>{});
        std::vector<uint8_t> buffer = builder.build();
        ElectricalCurrentProbeEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Description", str("")},
            {"Location", str("Reserved")},
            {"Status", str("Reserved")},
            {"Maximum Value", str("Unknown")},
            {"Minimum Value", str("Unknown")},
            {"Resolution", str("Unknown")},
            {"Tolerance", str("Unknown")},
            {"Accuracy", str("Unknown")},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
