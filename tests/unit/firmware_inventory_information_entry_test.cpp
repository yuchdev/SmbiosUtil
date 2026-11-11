#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/firmware_inventory_information_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive FirmwareInventoryInformationEntry
// (smbios/include/smbios/tables/firmware_inventory_information_entry.hpp),
// cross-checked directly against DSP0134 3.9.0's own 7.46/7.46.1-7.46.4
// (Table 133-137).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  component_name (DMI string index)
//   offset 0x05 : uint8_t  version (DMI string index)
//   offset 0x06 : uint8_t  version_format (Table 134)
//   offset 0x07 : uint8_t  firmware_id (DMI string index)
//   offset 0x08 : uint8_t  firmware_id_format (Table 135)
//   offset 0x09 : uint8_t  release_date (DMI string index)
//   offset 0x0A : uint8_t  manufacturer (DMI string index)
//   offset 0x0B : uint8_t  lowest_supported_version (DMI string index)
//   offset 0x0C : uint64_t image_size (QWORD; 0xFFFFFFFFFFFFFFFF = unknown)
//   offset 0x14 : uint16_t characteristics (Table 136 bit-field)
//   offset 0x16 : uint8_t  state (Table 137)
//   offset 0x17 : uint8_t  associated_component_count (n) - EXPLICIT count
//   offset 0x18 + 2*i : uint16_t associated_component_handle[i]
//
// This offset layout (0x18 immediately after the 0x17 count byte, nothing
// in between) was confirmed directly against the full Table 133 in
// docs/standard/DSP0134_3.9.0.md - see firmware_inventory_information_entry.hpp's
// own leading comment block.

BOOST_AUTO_TEST_SUITE(FirmwareInventoryInformationEntryUnitTests)

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

/// Build a formatted area for `handles.size()` trailing Associated
/// Component Handles, writing the explicit Number of Associated
/// Components field (0x17) as `handles.size()`.
std::vector<uint8_t> make_formatted_area(uint8_t version_format, uint8_t firmware_id_format,
                                          uint64_t image_size, uint16_t characteristics, uint8_t state,
                                          const std::vector<uint16_t>& handles)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(1);                 // 0x04 component_name string index
    formatted_area.push_back(2);                 // 0x05 version string index
    formatted_area.push_back(version_format);     // 0x06
    formatted_area.push_back(3);                 // 0x07 firmware_id string index
    formatted_area.push_back(firmware_id_format); // 0x08
    formatted_area.push_back(4);                 // 0x09 release_date string index
    formatted_area.push_back(5);                 // 0x0A manufacturer string index
    formatted_area.push_back(6);                 // 0x0B lowest_supported_version string index

    for (int i = 0; i < 8; ++i) {                 // 0x0C image_size (QWORD, little-endian)
        formatted_area.push_back(static_cast<uint8_t>((image_size >> (8 * i)) & 0xFF));
    }

    formatted_area.push_back(static_cast<uint8_t>(characteristics & 0xFF));        // 0x14
    formatted_area.push_back(static_cast<uint8_t>((characteristics >> 8) & 0xFF)); // 0x15
    formatted_area.push_back(state);                                              // 0x16
    formatted_area.push_back(static_cast<uint8_t>(handles.size()));               // 0x17 explicit count

    for (uint16_t handle : handles) {
        formatted_area.push_back(static_cast<uint8_t>(handle & 0xFF));
        formatted_area.push_back(static_cast<uint8_t>((handle >> 8) & 0xFF));
    }

    return formatted_area;
}

const std::vector<std::string> kStrings = {
    "BMC Firmware",                               // 1
    "1.45",                                       // 2
    "1624a9df-5e13-47fc-874a-df3aff143089",       // 3
    "2021-05-15T04:14:33+06:00",                  // 4
    "Acme Corp",                                  // 5
    "1.00"                                        // 6
};

} // namespace

/// Edge case: 0 Associated Component Handles.
/// get_associated_component_count() must be 0, and the handle accessor
/// must not read anything meaningful.
BOOST_AUTO_TEST_CASE(ZeroAssociatedComponentsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatMajorMinor,
        FirmwareInventoryInformationEntry::FirmwareIdFormatUefiGuid,
        12345, FirmwareInventoryInformationEntry::Updatable,
        FirmwareInventoryInformationEntry::StateEnabled, {});
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x18));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareInventoryInformation, formatted_area, kStrings);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    FirmwareInventoryInformationEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_component_name_string(), std::string("BMC Firmware"));
    BOOST_CHECK_EQUAL(entry.get_version_string(), std::string("1.45"));
    BOOST_CHECK_EQUAL(entry.get_version_format(), static_cast<uint8_t>(FirmwareInventoryInformationEntry::VersionFormatMajorMinor));
    BOOST_CHECK_EQUAL(entry.get_version_format_string(), std::string("Major.Minor"));
    BOOST_CHECK_EQUAL(entry.get_firmware_id_string(), std::string("1624a9df-5e13-47fc-874a-df3aff143089"));
    BOOST_CHECK_EQUAL(entry.get_firmware_id_format_string(), std::string("UEFI ESRT FwClass GUID / FMP ImageTypeId"));
    BOOST_CHECK_EQUAL(entry.get_release_date_string(), std::string("2021-05-15T04:14:33+06:00"));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_string(), std::string("Acme Corp"));
    BOOST_CHECK_EQUAL(entry.get_lowest_supported_version_string(), std::string("1.00"));
    BOOST_CHECK_EQUAL(entry.get_image_size(), static_cast<uint64_t>(12345));
    BOOST_CHECK(entry.is_image_size_known());
    BOOST_CHECK(entry.is_updatable());
    BOOST_CHECK(!entry.is_write_protected());
    BOOST_CHECK_EQUAL(entry.get_state_string(), std::string("Enabled"));

    BOOST_CHECK_EQUAL(entry.get_associated_component_count(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_associated_component_handle_at(0), static_cast<uint16_t>(0));
}

/// Normal case: 2+ Associated Component Handles.
BOOST_AUTO_TEST_CASE(TwoOrMoreAssociatedComponentsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatHex32,
        FirmwareInventoryInformationEntry::FirmwareIdFormatFreeForm,
        0x200000, FirmwareInventoryInformationEntry::WriteProtect,
        FirmwareInventoryInformationEntry::StateAbsent,
        { 0x0009, 0x0011, 0x002B });
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x18 + 2 * 3));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareInventoryInformation, formatted_area, kStrings);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    FirmwareInventoryInformationEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_version_format_string(), std::string("32-bit hexadecimal"));
    BOOST_CHECK_EQUAL(entry.get_firmware_id_format_string(), std::string("Free-form"));
    BOOST_CHECK(!entry.is_updatable());
    BOOST_CHECK(entry.is_write_protected());
    BOOST_CHECK_EQUAL(entry.get_state_string(), std::string("Absent"));

    BOOST_CHECK_EQUAL(entry.get_associated_component_count(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_associated_component_handle_at(0), static_cast<uint16_t>(0x0009));
    BOOST_CHECK_EQUAL(entry.get_associated_component_handle_at(1), static_cast<uint16_t>(0x0011));
    BOOST_CHECK_EQUAL(entry.get_associated_component_handle_at(2), static_cast<uint16_t>(0x002B));

    // Out-of-range index must not be confused with a real handle.
    BOOST_CHECK_EQUAL(entry.get_associated_component_handle_at(3), static_cast<uint16_t>(0));
}

/// Edge case: Image Size unknown sentinel (0xFFFFFFFFFFFFFFFF).
BOOST_AUTO_TEST_CASE(UnknownImageSizeSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatFreeForm,
        FirmwareInventoryInformationEntry::FirmwareIdFormatFreeForm,
        FirmwareInventoryInformationEntry::kImageSizeUnknown, 0,
        FirmwareInventoryInformationEntry::StateUnknown, {});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareInventoryInformation, formatted_area, kStrings);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    FirmwareInventoryInformationEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_image_size(), FirmwareInventoryInformationEntry::kImageSizeUnknown);
    BOOST_CHECK(!entry.is_image_size_known());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatFreeForm,
        FirmwareInventoryInformationEntry::FirmwareIdFormatFreeForm,
        0, 0, FirmwareInventoryInformationEntry::StateOther, {});

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(FirmwareInventoryInformationEntry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size()),
                       std::runtime_error);
}

/// FirmwareInventoryInformation resolves through SMBiosEntryFactory::create()
/// to a real FirmwareInventoryInformationEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsFirmwareInventoryInformationEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatFreeForm,
        FirmwareInventoryInformationEntry::FirmwareIdFormatFreeForm,
        0, 0, FirmwareInventoryInformationEntry::StateOther, { 0x0005 });

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareInventoryInformation, formatted_area, kStrings);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<FirmwareInventoryInformationEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009, Story 01.0, Task 02)
//
// Type: unit - deterministic, synthetic buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(): same labels and
// order; "Version"/"Firmware ID" carry the text's own "value (format)"
// string; each set Characteristics bit is a {label, true} field; each
// associated handle is "Handle N" (N 1-based, "0x" hex string as printed).
// Boundaries: both characteristic bits set; a structure truncated before
// Characteristics (no bit fields, state "Reserved", count 0).
// On failure first check: FirmwareInventoryInformationEntry::get_fields() in
// libsmbios/src/firmware_inventory_information_entry.cpp.

/// Normal case: two associated handles, one characteristic bit.
BOOST_AUTO_TEST_CASE(FirmwareInventoryInformationEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatMajorMinor,
        FirmwareInventoryInformationEntry::FirmwareIdFormatUefiGuid,
        12345, FirmwareInventoryInformationEntry::Updatable,
        FirmwareInventoryInformationEntry::StateEnabled, {0x0009, 0x002B});
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareInventoryInformation, formatted_area, kStrings);
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    FirmwareInventoryInformationEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Component Name", str("BMC Firmware")},
        {"Version", str("1.45 (Major.Minor)")},
        {"Firmware ID", str("1624a9df-5e13-47fc-874a-df3aff143089 (UEFI ESRT FwClass GUID / FMP ImageTypeId)")},
        {"Release Date", str("2021-05-15T04:14:33+06:00")},
        {"Manufacturer", str("Acme Corp")},
        {"Lowest Supported Version", str("1.00")},
        {"Image Size", str("12345 bytes")},
        {"Updatable", true},
        {"State", str("Enabled")},
        {"Associated Component Count", num(2)},
        {"Handle 1", str("0x9")},
        {"Handle 2", str("0x2b")},
    });
}

/// Edge case: both characteristic bits set -> two true fields, nothing
/// joined; unknown image size keeps the text's "Unknown" string.
BOOST_AUTO_TEST_CASE(FirmwareInventoryInformationEntryGetFieldsExpandsBitmaskTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatFreeForm,
        FirmwareInventoryInformationEntry::FirmwareIdFormatFreeForm,
        FirmwareInventoryInformationEntry::kImageSizeUnknown,
        FirmwareInventoryInformationEntry::Updatable | FirmwareInventoryInformationEntry::WriteProtect | 0x8000,
        FirmwareInventoryInformationEntry::StateDisabled, {});
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareInventoryInformation, formatted_area, kStrings);
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    FirmwareInventoryInformationEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    const std::vector<NamedField> fields = entry.get_fields();
    BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(11));
    check_fields_equal(std::vector<NamedField>(fields.begin() + 6, fields.begin() + 9), {
        {"Image Size", str("Unknown")},
        {"Updatable", true},
        {"Write-Protect", true},
    });
    check_unique_field_names(fields);
    check_no_joined_string_fields(fields);
}

/// Boundary case: structure truncated right after Image Size (length
/// 0x14) - Characteristics/State/count default, so no bit or handle fields.
BOOST_AUTO_TEST_CASE(FirmwareInventoryInformationEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = make_formatted_area(
        FirmwareInventoryInformationEntry::VersionFormatHex32,
        FirmwareInventoryInformationEntry::FirmwareIdFormatFreeForm,
        0x200000, FirmwareInventoryInformationEntry::Updatable,
        FirmwareInventoryInformationEntry::StateEnabled, {0x0009});
    formatted_area.resize(0x14 - 4);
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::FirmwareInventoryInformation, formatted_area, kStrings);
    std::vector<uint8_t> buffer = builder.build();
    DMIHeader header = make_header(buffer);
    FirmwareInventoryInformationEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Component Name", str("BMC Firmware")},
        {"Version", str("1.45 (32-bit hexadecimal)")},
        {"Firmware ID", str("1624a9df-5e13-47fc-874a-df3aff143089 (Free-form)")},
        {"Release Date", str("2021-05-15T04:14:33+06:00")},
        {"Manufacturer", str("Acme Corp")},
        {"Lowest Supported Version", str("1.00")},
        {"Image Size", str("2097152 bytes")},
        {"State", str("Reserved")},
        {"Associated Component Count", num(0)},
    });
}

BOOST_AUTO_TEST_SUITE_END()
