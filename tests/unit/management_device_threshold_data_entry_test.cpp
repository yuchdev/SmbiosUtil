#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/management_device_threshold_data_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive
// ManagementDeviceThresholdDataEntry
// (smbios/include/smbios/tables/management_device_threshold_data_entry.hpp),
// cross-checked against DSP0134 3.9.0's own 7.37 (Table 111) directly.
// Per this story's own task document allowance, not all 6 fields'
// unknown-sentinel cases are exercised exhaustively here (the shared
// `is_word_value_unknown` helper's own correctness is already covered by
// Story 04.0's probe tests' precedent for that helper) - at least one
// distinct unknown-sentinel case is included below, plus a normal-value
// case covering a couple of the 6 fields.
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint16_t lower threshold - non-critical
//   offset 0x06 : uint16_t upper threshold - non-critical
//   offset 0x08 : uint16_t lower threshold - critical
//   offset 0x0A : uint16_t upper threshold - critical
//   offset 0x0C : uint16_t lower threshold - non-recoverable
//   offset 0x0E : uint16_t upper threshold - non-recoverable

BOOST_AUTO_TEST_SUITE(ManagementDeviceThresholdDataEntryUnitTests)

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

std::vector<uint8_t> build_formatted_area(uint16_t lower_non_critical, uint16_t upper_non_critical,
                                           uint16_t lower_critical, uint16_t upper_critical,
                                           uint16_t lower_non_recoverable, uint16_t upper_non_recoverable)
{
    auto push_word = [](std::vector<uint8_t>& out, uint16_t value) {
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    };

    std::vector<uint8_t> formatted_area;
    push_word(formatted_area, lower_non_critical);
    push_word(formatted_area, upper_non_critical);
    push_word(formatted_area, lower_critical);
    push_word(formatted_area, upper_critical);
    push_word(formatted_area, lower_non_recoverable);
    push_word(formatted_area, upper_non_recoverable);
    return formatted_area;
}

} // namespace

/// Normal case: known values round-trip for all 6 fields.
BOOST_AUTO_TEST_CASE(NormalValuesDecodeTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(100, 200, 50, 250, 10, 300);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceThresholdData, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceThresholdDataEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.is_lower_threshold_non_critical_known());
    BOOST_CHECK_EQUAL(entry.get_lower_threshold_non_critical(), static_cast<uint16_t>(100));
    BOOST_CHECK(entry.is_upper_threshold_non_critical_known());
    BOOST_CHECK_EQUAL(entry.get_upper_threshold_non_critical(), static_cast<uint16_t>(200));
    BOOST_CHECK(entry.is_lower_threshold_critical_known());
    BOOST_CHECK_EQUAL(entry.get_lower_threshold_critical(), static_cast<uint16_t>(50));
    BOOST_CHECK(entry.is_upper_threshold_critical_known());
    BOOST_CHECK_EQUAL(entry.get_upper_threshold_critical(), static_cast<uint16_t>(250));
    BOOST_CHECK(entry.is_lower_threshold_non_recoverable_known());
    BOOST_CHECK_EQUAL(entry.get_lower_threshold_non_recoverable(), static_cast<uint16_t>(10));
    BOOST_CHECK(entry.is_upper_threshold_non_recoverable_known());
    BOOST_CHECK_EQUAL(entry.get_upper_threshold_non_recoverable(), static_cast<uint16_t>(300));
}

/// Distinct unknown-sentinel case: Lower Threshold - Non-critical == 0x8000
/// -> unknown. (Per task allowance, not all 6 fields' sentinel cases are
/// exhaustively exercised; the shared helper's correctness is already
/// covered by Story 04.0's probe tests.)
BOOST_AUTO_TEST_CASE(LowerThresholdNonCriticalUnknownSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(ManagementDeviceThresholdDataEntry::ValueUnknown, 200, 50, 250, 10, 300);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceThresholdData, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceThresholdDataEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_lower_threshold_non_critical_known());
    BOOST_CHECK_EQUAL(entry.get_lower_threshold_non_critical(), ManagementDeviceThresholdDataEntry::ValueUnknown);
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults to ValueUnknown.
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceThresholdData, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceThresholdDataEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_lower_threshold_non_critical_known());
    BOOST_CHECK_EQUAL(entry.get_lower_threshold_non_critical(), ManagementDeviceThresholdDataEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_upper_threshold_non_critical_known());
    BOOST_CHECK_EQUAL(entry.get_upper_threshold_non_critical(), ManagementDeviceThresholdDataEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_lower_threshold_critical_known());
    BOOST_CHECK_EQUAL(entry.get_lower_threshold_critical(), ManagementDeviceThresholdDataEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_upper_threshold_critical_known());
    BOOST_CHECK_EQUAL(entry.get_upper_threshold_critical(), ManagementDeviceThresholdDataEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_lower_threshold_non_recoverable_known());
    BOOST_CHECK_EQUAL(entry.get_lower_threshold_non_recoverable(), ManagementDeviceThresholdDataEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_upper_threshold_non_recoverable_known());
    BOOST_CHECK_EQUAL(entry.get_upper_threshold_non_recoverable(), ManagementDeviceThresholdDataEntry::ValueUnknown);
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(100, 200, 50, 250, 10, 300);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(ManagementDeviceThresholdDataEntry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 05.0 Task 02's own success criterion:
/// ManagementDeviceThresholdData resolves through
/// SMBiosEntryFactory::create() to a real ManagementDeviceThresholdDataEntry,
/// not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsManagementDeviceThresholdDataEntryTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(100, 200, 50, 250, 10, 300);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceThresholdData, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<ManagementDeviceThresholdDataEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009 Story 01.0 Task 05)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(); each of the six
// thresholds stays the printed decimal or "Unknown" string.
// Boundaries: 0x8000 sentinel, a structure truncated after two thresholds,
// header-only structure. No bitmask.
// On failure first check: ManagementDeviceThresholdDataEntry::get_fields() against
// ManagementDeviceThresholdDataEntry::render_to_description() in libsmbios/src/management_device_threshold_data_entry.cpp.

/// Normal case: five known thresholds and one 0x8000 sentinel.
BOOST_AUTO_TEST_CASE(ManagementDeviceThresholdDataEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = build_formatted_area(100, 200, ManagementDeviceThresholdDataEntry::ValueUnknown, 250, 10, 300);
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceThresholdData, formatted_area);
    std::vector<uint8_t> buffer = builder.build();
    ManagementDeviceThresholdDataEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Lower Threshold - Non-critical", str("100")},
        {"Upper Threshold - Non-critical", str("200")},
        {"Lower Threshold - Critical", str("Unknown")},
        {"Upper Threshold - Critical", str("250")},
        {"Lower Threshold - Non-recoverable", str("10")},
        {"Upper Threshold - Non-recoverable", str("300")},
    });
}

/// Boundary cases: every threshold is always printed, so a structure
/// truncated after the first two WORDs, or with no formatted area at all,
/// still yields all six fields, the missing ones as "Unknown".
BOOST_AUTO_TEST_CASE(ManagementDeviceThresholdDataEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> formatted_area = build_formatted_area(100, 200, 0, 0, 0, 0);
        formatted_area.resize(4); // keep only 0x04-0x07
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ManagementDeviceThresholdData, formatted_area);
        std::vector<uint8_t> buffer = builder.build();
        ManagementDeviceThresholdDataEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Lower Threshold - Non-critical", str("100")},
            {"Upper Threshold - Non-critical", str("200")},
            {"Lower Threshold - Critical", str("Unknown")},
            {"Upper Threshold - Critical", str("Unknown")},
            {"Lower Threshold - Non-recoverable", str("Unknown")},
            {"Upper Threshold - Non-recoverable", str("Unknown")},
        });
    }
    {
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ManagementDeviceThresholdData, std::vector<uint8_t>{});
        std::vector<uint8_t> buffer = builder.build();
        ManagementDeviceThresholdDataEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        const std::vector<NamedField> fields = entry.get_fields();
        BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(6));
        for (const auto& field : fields) {
            BOOST_CHECK(field.value == str("Unknown"));
        }
        check_unique_field_names(fields);
    }
}

BOOST_AUTO_TEST_SUITE_END()
