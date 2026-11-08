#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/tables/pointing_device_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive PointingDeviceEntry
// (smbios/include/smbios/tables/pointing_device_entry.h), cross-checked
// against DSP0134 3.9.0's own 7.22/7.22.1/7.22.2 (Table 87/88/89):
//
//   - a normal-case full buffer decoding every documented Type (Table 88)
//     and Interface (Table 89) enum value
//   - an edge-case minimum-length buffer (header.length == 0x04, the field
//     itself absent) -> every getter defaults to its *OutOfSpec sentinel
//   - an invalid-input case: unrecognized (not-in-spec) Type/Interface byte
//     values -> OutOfSpec, not a crash or a bogus enum
//
// Wire layout (see the header's "@brief 0xNN offset" getter comments):
//   offset 0x00 : uint8_t type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t device_type
//   offset 0x05 : uint8_t interface
//   offset 0x06 : uint8_t button_count

BOOST_AUTO_TEST_SUITE(PointingDeviceEntryUnitTests)

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

} // namespace

/// Normal case: full buffer, every documented Device Type (Table 88) value
/// decodes to its own enum/string.
BOOST_AUTO_TEST_CASE(EveryDocumentedDeviceTypeValueDecodesTestCase)
{
    const std::vector<std::pair<uint8_t, std::string>> cases{
        {PointingDeviceEntry::Other, "Other"},
        {PointingDeviceEntry::Unknown, "Unknown"},
        {PointingDeviceEntry::Mouse, "Mouse"},
        {PointingDeviceEntry::TrackBall, "Track Ball"},
        {PointingDeviceEntry::TrackPoint, "Track Point"},
        {PointingDeviceEntry::GlidePoint, "Glide Point"},
        {PointingDeviceEntry::TouchPad, "Touch Pad"},
        {PointingDeviceEntry::TouchScreen, "Touch Screen"},
        {PointingDeviceEntry::OpticalSensor, "Optical Sensor"},
    };

    for (const auto& device_type_case : cases) {
        std::vector<uint8_t> formatted_area{
            device_type_case.first, // 0x04 device_type
            PointingDeviceEntry::PS2, // 0x05 interface
            0x03, // 0x06 button_count
        };

        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::PointingDevice, formatted_area);
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        PointingDeviceEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.get_device_type(), device_type_case.first);
        BOOST_CHECK_EQUAL(entry.get_device_type_string(), device_type_case.second);
    }
}

/// Normal case: full buffer, every documented Interface (Table 89) value
/// decodes to its own enum/string.
BOOST_AUTO_TEST_CASE(EveryDocumentedInterfaceValueDecodesTestCase)
{
    const std::vector<std::pair<uint8_t, std::string>> cases{
        {PointingDeviceEntry::InterfaceOther, "Other"},
        {PointingDeviceEntry::InterfaceUnknown, "Unknown"},
        {PointingDeviceEntry::Serial, "Serial"},
        {PointingDeviceEntry::PS2, "PS/2"},
        {PointingDeviceEntry::Infrared, "Infrared"},
        {PointingDeviceEntry::HPHIL, "HP-HIL"},
        {PointingDeviceEntry::BusMouse, "Bus mouse"},
        {PointingDeviceEntry::ADB, "ADB (Apple Desktop Bus)"},
        {PointingDeviceEntry::BusMouseDB9, "Bus mouse DB-9"},
        {PointingDeviceEntry::BusMouseMicroDIN, "Bus mouse micro-DIN"},
        {PointingDeviceEntry::USB, "USB"},
        {PointingDeviceEntry::I2C, "I2C"},
        {PointingDeviceEntry::SPI, "SPI"},
    };

    for (const auto& interface_case : cases) {
        std::vector<uint8_t> formatted_area{
            PointingDeviceEntry::Mouse, // 0x04 device_type
            interface_case.first, // 0x05 interface
            0x02, // 0x06 button_count
        };

        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::PointingDevice, formatted_area);
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        PointingDeviceEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.get_interface(), interface_case.first);
        BOOST_CHECK_EQUAL(entry.get_interface_string(), interface_case.second);
    }
}

/// Normal case: three-button mouse, full buffer, matching the spec's own
/// worked example ("if the device has three buttons, the field value is
/// 03h").
BOOST_AUTO_TEST_CASE(ThreeButtonMouseDecodesTestCase)
{
    std::vector<uint8_t> formatted_area{
        PointingDeviceEntry::Mouse, // 0x04 device_type
        PointingDeviceEntry::USB, // 0x05 interface
        0x03, // 0x06 button_count
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PointingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PointingDeviceEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_type(), std::string("Built-in Pointing Device"));
    BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<uint8_t>(PointingDeviceEntry::Mouse));
    BOOST_CHECK_EQUAL(entry.get_interface(), static_cast<uint8_t>(PointingDeviceEntry::USB));
    BOOST_CHECK_EQUAL(entry.get_button_count(), static_cast<uint8_t>(0x03));
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults to its OutOfSpec sentinel/0.
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PointingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PointingDeviceEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<uint8_t>(PointingDeviceEntry::DeviceTypeOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_device_type_string(), std::string("OutOfSpec"));
    BOOST_CHECK_EQUAL(entry.get_interface(), static_cast<uint8_t>(PointingDeviceEntry::InterfaceOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_interface_string(), std::string("OutOfSpec"));
    BOOST_CHECK_EQUAL(entry.get_button_count(), static_cast<uint8_t>(0));
}

/// Invalid-input case: Device Type/Interface bytes not present in Table
/// 88/89 -> OutOfSpec, not a crash or a bogus enum value.
BOOST_AUTO_TEST_CASE(UnrecognizedDeviceTypeAndInterfaceValuesReportOutOfSpecTestCase)
{
    std::vector<uint8_t> formatted_area{
        0x7F, // 0x04 device_type - not in Table 88
        0x7E, // 0x05 interface - not in Table 89
        0x05, // 0x06 button_count
    };

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PointingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    PointingDeviceEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<uint8_t>(PointingDeviceEntry::DeviceTypeOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_device_type_string(), std::string("OutOfSpec"));
    BOOST_CHECK_EQUAL(entry.get_interface(), static_cast<uint8_t>(PointingDeviceEntry::InterfaceOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_interface_string(), std::string("OutOfSpec"));
    BOOST_CHECK_EQUAL(entry.get_button_count(), static_cast<uint8_t>(0x05));
}

/// Milestone 0010 Story 04.0 Task 01's own success criterion:
/// PointingDevice resolves through SMBiosEntryFactory::create() to a real
/// PointingDeviceEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsPointingDeviceEntryTestCase)
{
    std::vector<uint8_t> formatted_area{
        PointingDeviceEntry::TouchPad, 0x04 /* PS2 */, 0x01
    };
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::PointingDevice, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<PointingDeviceEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
