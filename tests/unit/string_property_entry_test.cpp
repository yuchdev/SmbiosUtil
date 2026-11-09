#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/string_property_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive StringPropertyEntry
// (smbios/include/smbios/tables/string_property_entry.h), cross-checked
// directly against DSP0134 3.9.0's own 7.47/7.47.1 (Table 138/139).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint16_t  property_id (Table 139)
//   offset 0x06 : uint8_t   value (DMI string index)
//   offset 0x07 : uint16_t  parent_handle
//
// get_property_id_string()'s three reserved *ranges* (2-32767 DMTF,
// 32768-49151 Firmware Vendor, 49152-65535 OEM) are each exercised by a
// distinct test case below, per this task's own explicit requirement, in
// addition to the one currently-assigned value (1, UEFI device path).

BOOST_AUTO_TEST_SUITE(StringPropertyEntryUnitTests)

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

std::vector<uint8_t> make_formatted_area(uint16_t property_id, uint8_t value_string_index, uint16_t parent_handle)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(static_cast<uint8_t>(property_id & 0xFF));        // 0x04
    formatted_area.push_back(static_cast<uint8_t>((property_id >> 8) & 0xFF)); // 0x05
    formatted_area.push_back(value_string_index);                              // 0x06
    formatted_area.push_back(static_cast<uint8_t>(parent_handle & 0xFF));        // 0x07
    formatted_area.push_back(static_cast<uint8_t>((parent_handle >> 8) & 0xFF)); // 0x08
    return formatted_area;
}

} // namespace

/// Normal case: the one currently-assigned String Property ID (1, UEFI
/// device path).
BOOST_AUTO_TEST_CASE(UefiDevicePathIdTestCase)
{
    std::vector<uint8_t> formatted_area =
        make_formatted_area(StringPropertyEntry::UefiDevicePath, 1, 0x0010);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::StringProperty, formatted_area, { "PciRoot(0x0)/Pci(0x1,0x0)" });
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    StringPropertyEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_property_id(), static_cast<uint16_t>(StringPropertyEntry::UefiDevicePath));
    BOOST_CHECK_EQUAL(entry.get_property_id_string(), std::string("UEFI device path"));
    BOOST_CHECK_EQUAL(entry.get_value_string(), std::string("PciRoot(0x0)/Pci(0x1,0x0)"));
    BOOST_CHECK_EQUAL(entry.get_parent_handle(), static_cast<uint16_t>(0x0010));
}

/// Edge case: Reserved (0) - "do not use", but must still render
/// distinctly as "Reserved" rather than crashing or claiming a decoded
/// meaning.
BOOST_AUTO_TEST_CASE(ReservedZeroIdTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(0, 0, 0x0001);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::StringProperty, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    StringPropertyEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_property_id(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_property_id_string(), std::string("Reserved"));
}

/// Edge case: a value from the DMTF-reserved range (2-32767) must render
/// as "Reserved (DMTF)", not a flat "Unknown".
BOOST_AUTO_TEST_CASE(DmtfReservedRangeIdTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(1000, 0, 0x0002);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::StringProperty, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    StringPropertyEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_property_id(), static_cast<uint16_t>(1000));
    BOOST_CHECK_EQUAL(entry.get_property_id_string(), std::string("Reserved (DMTF)"));
}

/// Edge case: a value from the firmware-vendor-reserved range
/// (32768-49151) must render as "Reserved (Firmware Vendor)".
BOOST_AUTO_TEST_CASE(FirmwareVendorReservedRangeIdTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(40000, 0, 0x0003);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::StringProperty, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    StringPropertyEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_property_id(), static_cast<uint16_t>(40000));
    BOOST_CHECK_EQUAL(entry.get_property_id_string(), std::string("Reserved (Firmware Vendor)"));
}

/// Edge case: a value from the OEM-reserved range (49152-65535) must
/// render as "Reserved (OEM)".
BOOST_AUTO_TEST_CASE(OemReservedRangeIdTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(60000, 0, 0x0004);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::StringProperty, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    StringPropertyEntry entry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_property_id(), static_cast<uint16_t>(60000));
    BOOST_CHECK_EQUAL(entry.get_property_id_string(), std::string("Reserved (OEM)"));
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(StringPropertyEntry::UefiDevicePath, 0, 0x0001);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(StringPropertyEntry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size()), std::runtime_error);
}

/// StringProperty resolves through SMBiosEntryFactory::create() to a real
/// StringPropertyEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsStringPropertyEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(StringPropertyEntry::UefiDevicePath, 0, 0x0001);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::StringProperty, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<StringPropertyEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
