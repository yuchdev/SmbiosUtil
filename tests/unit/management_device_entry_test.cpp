#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.h>
#include <libsmbios/tables/management_device_entry.h>
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
// smbios::test::SyntheticTableBuilder) drive ManagementDeviceEntry
// (smbios/include/smbios/tables/management_device_entry.h), cross-checked
// against DSP0134 3.9.0's own 7.35 (Table 107), 7.35.1 (Table 108), and
// 7.35.2 (Table 109) directly.
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  description string index
//   offset 0x05 : uint8_t  device type (enum, Table 108)
//   offset 0x06 : uint32_t address
//   offset 0x0A : uint8_t  address type (enum, Table 109)

BOOST_AUTO_TEST_SUITE(ManagementDeviceEntryUnitTests)

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

std::vector<uint8_t> build_formatted_area(uint8_t device_type, uint32_t address, uint8_t address_type)
{
    return std::vector<uint8_t>{
        1, // 0x04 Description string index
        device_type, // 0x05
        static_cast<uint8_t>(address & 0xFF), static_cast<uint8_t>((address >> 8) & 0xFF), // 0x06-0x07
        static_cast<uint8_t>((address >> 16) & 0xFF), static_cast<uint8_t>((address >> 24) & 0xFF), // 0x08-0x09
        address_type, // 0x0A
    };
}

} // namespace

/// Normal case: full-length buffer, all fields decode as written.
BOOST_AUTO_TEST_CASE(NormalValuesDecodeTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(ManagementDeviceEntry::DeviceTypeNationalSemiconductorLm75, 0x12345678, ManagementDeviceEntry::AddressTypeSmBus);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDevice, formatted_area, {"Fan Controller"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), "Fan Controller");
    BOOST_CHECK_EQUAL(entry.get_device_type(), ManagementDeviceEntry::DeviceTypeNationalSemiconductorLm75);
    BOOST_CHECK_EQUAL(entry.get_device_type_string(), "National Semiconductor LM75");
    BOOST_CHECK_EQUAL(entry.get_address(), static_cast<uint32_t>(0x12345678));
    BOOST_CHECK_EQUAL(entry.get_address_type(), ManagementDeviceEntry::AddressTypeSmBus);
    BOOST_CHECK_EQUAL(entry.get_address_type_string(), "SM Bus");
}

/// Per-documented-enum-value coverage: every Device Type value in Table 108.
BOOST_AUTO_TEST_CASE(EveryDeviceTypeValueDecodesTestCase)
{
    const std::vector<std::pair<uint8_t, std::string>> values{
        {ManagementDeviceEntry::DeviceTypeOther, "Other"},
        {ManagementDeviceEntry::DeviceTypeUnknown, "Unknown"},
        {ManagementDeviceEntry::DeviceTypeNationalSemiconductorLm75, "National Semiconductor LM75"},
        {ManagementDeviceEntry::DeviceTypeNationalSemiconductorLm78, "National Semiconductor LM78"},
        {ManagementDeviceEntry::DeviceTypeNationalSemiconductorLm79, "National Semiconductor LM79"},
        {ManagementDeviceEntry::DeviceTypeNationalSemiconductorLm80, "National Semiconductor LM80"},
        {ManagementDeviceEntry::DeviceTypeNationalSemiconductorLm81, "National Semiconductor LM81"},
        {ManagementDeviceEntry::DeviceTypeAnalogDevicesAdm9240, "Analog Devices ADM9240"},
        {ManagementDeviceEntry::DeviceTypeDallasSemiconductorDs1780, "Dallas Semiconductor DS1780"},
        {ManagementDeviceEntry::DeviceTypeMaxim1617, "Maxim 1617"},
        {ManagementDeviceEntry::DeviceTypeGenesysGl518sm, "Genesys GL518SM"},
        {ManagementDeviceEntry::DeviceTypeWinbondW83781d, "Winbond W83781D"},
        {ManagementDeviceEntry::DeviceTypeHoltekHt82h791, "Holtek HT82H791"},
    };

    for (const auto& value : values) {
        std::vector<uint8_t> formatted_area = build_formatted_area(value.first, 0, ManagementDeviceEntry::AddressTypeOther);

        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ManagementDevice, formatted_area);
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        ManagementDeviceEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<ManagementDeviceEntry::DeviceType>(value.first));
        BOOST_CHECK_EQUAL(entry.get_device_type_string(), value.second);
    }
}

/// Per-documented-enum-value coverage: every Address Type value in Table 109.
BOOST_AUTO_TEST_CASE(EveryAddressTypeValueDecodesTestCase)
{
    const std::vector<std::pair<uint8_t, std::string>> values{
        {ManagementDeviceEntry::AddressTypeOther, "Other"},
        {ManagementDeviceEntry::AddressTypeUnknown, "Unknown"},
        {ManagementDeviceEntry::AddressTypeIoPort, "I/O Port"},
        {ManagementDeviceEntry::AddressTypeMemory, "Memory"},
        {ManagementDeviceEntry::AddressTypeSmBus, "SM Bus"},
    };

    for (const auto& value : values) {
        std::vector<uint8_t> formatted_area = build_formatted_area(ManagementDeviceEntry::DeviceTypeOther, 0, value.first);

        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::ManagementDevice, formatted_area);
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        ManagementDeviceEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.get_address_type(), static_cast<ManagementDeviceEntry::AddressType>(value.first));
        BOOST_CHECK_EQUAL(entry.get_address_type_string(), value.second);
    }
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults (empty description, OutOfSpec
/// device/address type, 0 address).
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    ManagementDeviceEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_description_string(), std::string());
    BOOST_CHECK_EQUAL(entry.get_device_type(), ManagementDeviceEntry::DeviceTypeOutOfSpec);
    BOOST_CHECK_EQUAL(entry.get_device_type_string(), "Reserved");
    BOOST_CHECK_EQUAL(entry.get_address(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_address_type(), ManagementDeviceEntry::AddressTypeOutOfSpec);
    BOOST_CHECK_EQUAL(entry.get_address_type_string(), "Reserved");
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(ManagementDeviceEntry::DeviceTypeOther, 0, ManagementDeviceEntry::AddressTypeOther);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDeviceComponent, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(ManagementDeviceEntry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 05.0 Task 02's own success criterion:
/// ManagementDevice resolves through SMBiosEntryFactory::create() to a real
/// ManagementDeviceEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsManagementDeviceEntryTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(ManagementDeviceEntry::DeviceTypeOther, 0, ManagementDeviceEntry::AddressTypeOther);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ManagementDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<ManagementDeviceEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
