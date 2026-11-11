#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/management_device_component_entry.hpp>
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
// ManagementDeviceComponentEntry
// (smbios/include/smbios/tables/management_device_component_entry.hpp),
// cross-checked against DSP0134 3.9.0's own 7.36 (Table 110) directly,
// including the Threshold Handle "no threshold" sentinel confirmation (see
// the class's own header comment: 0xFFFF, per DSP0134 3.9.0's generic
// Table 3 Handle-field text, since Table 110 itself does not override that
// default).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  description string index
//   offset 0x05 : uint16_t management device handle
//   offset 0x07 : uint16_t component handle
//   offset 0x09 : uint16_t threshold handle (0xFFFF = none)

BOOST_AUTO_TEST_SUITE(ManagementDeviceComponentEntryUnitTests)

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

std::vector<uint8_t> build_formatted_area(uint16_t management_device_handle, uint16_t component_handle, uint16_t threshold_handle)
{
    return std::vector<uint8_t>{
        1, // 0x04 Description string index
        static_cast<uint8_t>(management_device_handle & 0xFF), static_cast<uint8_t>((management_device_handle >> 8) & 0xFF), // 0x05-0x06
        static_cast<uint8_t>(component_handle & 0xFF), static_cast<uint8_t>((component_handle >> 8) & 0xFF), // 0x07-0x08
        static_cast<uint8_t>(threshold_handle & 0xFF), static_cast<uint8_t>((threshold_handle >> 8) & 0xFF), // 0x09-0x0A
    };
}

} // namespace

/// Normal case: real handle values for all three handle fields, including
/// a real (non-sentinel) Threshold Handle.
BOOST_AUTO_TEST_CASE(RealThresholdHandleTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x0010, 0x0020, 0x0030);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area, {"CPU Fan"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceComponentEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), "CPU Fan");
    BOOST_CHECK_EQUAL(entry.get_management_device_handle(), static_cast<uint16_t>(0x0010));
    BOOST_CHECK_EQUAL(entry.get_component_handle(), static_cast<uint16_t>(0x0020));
    BOOST_CHECK_EQUAL(entry.get_threshold_handle(), static_cast<uint16_t>(0x0030));
    BOOST_CHECK(entry.has_threshold());
}

/// Confirmed "no threshold" sentinel case: Threshold Handle == 0xFFFF ->
/// has_threshold() is false.
BOOST_AUTO_TEST_CASE(NoThresholdSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x0010, 0x0020, ManagementDeviceComponentEntry::NoThresholdHandle);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceComponentEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_threshold_handle(), ManagementDeviceComponentEntry::NoThresholdHandle);
    BOOST_CHECK(!entry.has_threshold());
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults (empty description, 0 handles,
/// threshold handle defaults to NoThresholdHandle).
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceComponentEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), std::string());
    BOOST_CHECK_EQUAL(entry.get_management_device_handle(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_component_handle(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_threshold_handle(), ManagementDeviceComponentEntry::NoThresholdHandle);
    BOOST_CHECK(!entry.has_threshold());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x0010, 0x0020, 0x0030);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(ManagementDeviceComponentEntry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 05.0 Task 02's own success criterion:
/// ManagementDeviceComponent resolves through SMBiosEntryFactory::create()
/// to a real ManagementDeviceComponentEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsManagementDeviceComponentEntryTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x0010, 0x0020, 0x0030);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<ManagementDeviceComponentEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009 Story 01.0 Task 05)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(); the two
// handles are uint64_t, Threshold Handle stays the printed string because
// it can print the "None" sentinel.
// Boundaries: real vs 0xFFFF Threshold Handle, header-only structure. No bitmask.
// On failure first check: ManagementDeviceComponentEntry::get_fields() against
// ManagementDeviceComponentEntry::render_to_description() in libsmbios/src/management_device_component_entry.cpp.

/// Normal case: real Threshold Handle prints as a decimal string.
BOOST_AUTO_TEST_CASE(ManagementDeviceComponentEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = build_formatted_area(0x0010, 0x0020, 0x0030);
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area, {"CPU Fan"});
    std::vector<uint8_t> buffer = builder.build();
    ManagementDeviceComponentEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Description", str("CPU Fan")},
        {"Management Device Handle", num(0x0010)},
        {"Component Handle", num(0x0020)},
        {"Threshold Handle", str("48")},
    });
}

/// Boundary cases: the 0xFFFF sentinel prints "None"; a header-only
/// structure defaults the handles to 0 and Threshold Handle to "None".
BOOST_AUTO_TEST_CASE(ManagementDeviceComponentEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> formatted_area = build_formatted_area(0x0010, 0x0020, ManagementDeviceComponentEntry::NoThresholdHandle);
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area, {"CPU Fan"});
        std::vector<uint8_t> buffer = builder.build();
        ManagementDeviceComponentEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Description", str("CPU Fan")},
            {"Management Device Handle", num(0x0010)},
            {"Component Handle", num(0x0020)},
            {"Threshold Handle", str("None")},
        });
    }
    {
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ManagementDeviceComponent, std::vector<uint8_t>{});
        std::vector<uint8_t> buffer = builder.build();
        ManagementDeviceComponentEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Description", str("")},
            {"Management Device Handle", num(0)},
            {"Component Handle", num(0)},
            {"Threshold Handle", str("None")},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
