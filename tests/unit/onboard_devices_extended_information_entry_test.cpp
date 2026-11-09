#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.h>
#include <libsmbios/tables/onboard_devices_extended_information_entry.h>
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
// smbios::test::SyntheticTableBuilder) drive
// OnboardDevicesExtendedInformationEntry
// (smbios/include/smbios/tables/onboard_devices_extended_information_entry.h),
// cross-checked directly against DSP0134 3.9.0's own 7.42/7.42.1-7.42.4
// (Table 120/121).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  reference_designation (STRING)
//   offset 0x05 : uint8_t  device_type (bit 7 enabled/disabled, bits 6:0 type enum)
//   offset 0x06 : uint8_t  device_type_instance
//   offset 0x07 : uint16_t segment_group_number
//   offset 0x09 : uint8_t  bus_number
//   offset 0x0A : uint8_t  device_function_number (bits 7:3 device, bits 2:0 function)
//
// Minimum structure length is 0x0B (Table 120's own "0Bh").
//
// Bus-address "not applicable" sentinel confirmed as the same all-FF/
// all-FFFF convention as SystemSlotsEntry's own 7.10.8 - see
// onboard_devices_extended_information_entry.h's own leading comment block
// for the direct 7.42.4-vs-7.10.8 text comparison.

BOOST_AUTO_TEST_SUITE(OnboardDevicesExtendedInformationEntryUnitTests)

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

void append_u16(std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

/// Build the full (minimum-length, 0x0B) formatted area.
std::vector<uint8_t> make_formatted_area(uint8_t device_type_byte, uint8_t device_type_instance,
                                          uint16_t segment_group_number, uint8_t bus_number,
                                          uint8_t device_function_number)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(0x01);                  // 0x04 Reference Designation (STRING)
    formatted_area.push_back(device_type_byte);       // 0x05
    formatted_area.push_back(device_type_instance);   // 0x06
    append_u16(formatted_area, segment_group_number); // 0x07
    formatted_area.push_back(bus_number);              // 0x09
    formatted_area.push_back(device_function_number);  // 0x0A
    return formatted_area;
}

} // namespace

/// Normal case: device enabled, type Ethernet, plausible bus address.
BOOST_AUTO_TEST_CASE(DeviceEnabledEthernetTestCase)
{
    // bit7=1 (enabled) | type=Ethernet(0x05) -> 0x85
    std::vector<uint8_t> formatted_area = make_formatted_area(0x85, 0x02, 0x0000, 0x03, (0x00 << 3) | 0x00);
    BOOST_REQUIRE_EQUAL(formatted_area.size() + 4, static_cast<size_t>(0x0B));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area, {"J_LAN1"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OnboardDevicesExtendedInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_reference_designation_string(), std::string("J_LAN1"));
    BOOST_CHECK(entry.is_device_enabled());
    BOOST_CHECK_EQUAL(entry.get_device_type(), OnboardDevicesExtendedInformationEntry::DeviceTypeEthernet);
    BOOST_CHECK_EQUAL(entry.get_device_type_string(), std::string("Ethernet"));
    BOOST_CHECK_EQUAL(entry.get_device_type_instance(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bus_number(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_device_number(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_function_number(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bus_address_string(), std::string("0000:03:00.0"));
}

/// Normal case: device disabled, type SATA Controller, non-trivial
/// device/function split.
BOOST_AUTO_TEST_CASE(DeviceDisabledSataControllerTestCase)
{
    // bit7=0 (disabled) | type=SATA Controller(0x09) -> 0x09
    // device=2 (bits 7:3), function=5 (bits 2:0) -> (2<<3)|5 = 0x15
    std::vector<uint8_t> formatted_area = make_formatted_area(0x09, 0x01, 0x0001, 0x04, 0x15);
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area, {"J_SATA0"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OnboardDevicesExtendedInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_device_enabled());
    BOOST_CHECK_EQUAL(entry.get_device_type(), OnboardDevicesExtendedInformationEntry::DeviceTypeSataController);
    BOOST_CHECK_EQUAL(entry.get_device_type_string(), std::string("SATA Controller"));
    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(1));
    BOOST_CHECK_EQUAL(entry.get_bus_number(), static_cast<uint8_t>(4));
    BOOST_CHECK_EQUAL(entry.get_device_number(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_function_number(), static_cast<uint8_t>(5));
    BOOST_CHECK_EQUAL(entry.get_bus_address_string(), std::string("0001:04:02.5"));
}

/// CRITICAL constraint test: the enabled/disabled bit (bit 7) and the
/// device-type bits (bits 6:0) must decode entirely independently of one
/// another - set bit 7 while varying the type value, and confirm both are
/// correct in every combination, to catch any accidental shared-mask bug
/// (e.g. a mistaken `& 0xFF`/no-mask combined accessor).
BOOST_AUTO_TEST_CASE(EnabledBitAndDeviceTypeDecodeIndependentlyTestCase)
{
    struct Case {
        uint8_t device_type_byte;
        bool expected_enabled;
        OnboardDevicesExtendedInformationEntry::DeviceType expected_type;
    };

    const Case cases[] = {
        // bit 7 set (enabled), type varies.
        {0x80 | OnboardDevicesExtendedInformationEntry::DeviceTypeOther, true, OnboardDevicesExtendedInformationEntry::DeviceTypeOther},
        {0x80 | OnboardDevicesExtendedInformationEntry::DeviceTypeVideo, true, OnboardDevicesExtendedInformationEntry::DeviceTypeVideo},
        {0x80 | OnboardDevicesExtendedInformationEntry::DeviceTypeUfsController, true, OnboardDevicesExtendedInformationEntry::DeviceTypeUfsController},
        // bit 7 clear (disabled), the exact same type values.
        {0x00 | OnboardDevicesExtendedInformationEntry::DeviceTypeOther, false, OnboardDevicesExtendedInformationEntry::DeviceTypeOther},
        {0x00 | OnboardDevicesExtendedInformationEntry::DeviceTypeVideo, false, OnboardDevicesExtendedInformationEntry::DeviceTypeVideo},
        {0x00 | OnboardDevicesExtendedInformationEntry::DeviceTypeUfsController, false, OnboardDevicesExtendedInformationEntry::DeviceTypeUfsController},
    };

    for (const Case& current : cases) {
        std::vector<uint8_t> formatted_area = make_formatted_area(current.device_type_byte, 0x00, 0x0000, 0x00, 0x00);
        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area, {"Device"});
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        OnboardDevicesExtendedInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.is_device_enabled(), current.expected_enabled);
        BOOST_CHECK_EQUAL(entry.get_device_type(), current.expected_type);
    }
}

/// Edge case: every documented Device Type enum value (7.42.2, Table 121)
/// decodes to the correct string, combined with the enabled bit set so
/// this also exercises the enum table in full.
BOOST_AUTO_TEST_CASE(EveryDocumentedDeviceTypeValueDecodesTestCase)
{
    const std::pair<uint8_t, std::string> expectations[] = {
        {OnboardDevicesExtendedInformationEntry::DeviceTypeOther, "Other"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeUnknown, "Unknown"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeVideo, "Video"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeScsiController, "SCSI Controller"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeEthernet, "Ethernet"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeTokenRing, "Token Ring"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeSound, "Sound"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypePataController, "PATA Controller"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeSataController, "SATA Controller"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeSasController, "SAS Controller"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeWirelessLan, "Wireless LAN"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeBluetooth, "Bluetooth"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeWwan, "WWAN"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeEmmc, "eMMC (embedded Multi-Media Controller)"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeNvmeController, "NVMe Controller"},
        {OnboardDevicesExtendedInformationEntry::DeviceTypeUfsController, "UFS Controller"},
    };

    for (const auto& expectation : expectations) {
        std::vector<uint8_t> formatted_area = make_formatted_area(static_cast<uint8_t>(0x80 | expectation.first), 0x00, 0x0000, 0x00, 0x00);
        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area, {"Device"});
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        OnboardDevicesExtendedInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<OnboardDevicesExtendedInformationEntry::DeviceType>(expectation.first));
        BOOST_CHECK_EQUAL(entry.get_device_type_string(), expectation.second);
    }
}

/// Edge case: an undocumented/out-of-spec device type value (bits 6:0)
/// falls back to "OutOfSpec".
BOOST_AUTO_TEST_CASE(OutOfSpecDeviceTypeTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(0x7F /* bits 6:0 = 0x7F, undocumented */, 0x00, 0x0000, 0x00, 0x00);
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area, {"Device"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OnboardDevicesExtendedInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_device_type_string(), std::string("OutOfSpec"));
}

/// Edge case: "not applicable" bus-address sentinel - Segment Group Number
/// == 0xFFFF, Bus Number == 0xFF, Device/Function Number == 0xFF, all
/// simultaneously, confirmed as the same convention as SystemSlotsEntry's
/// own 7.10.8 sentinel (see this entry's header doc comment).
BOOST_AUTO_TEST_CASE(BusAddressNotApplicableSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(0x80 | OnboardDevicesExtendedInformationEntry::DeviceTypeSound,
                                                                0x01, 0xFFFF, 0xFF, 0xFF);
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area, {"Onboard Audio"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OnboardDevicesExtendedInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(0xFFFF));
    BOOST_CHECK_EQUAL(entry.get_bus_number(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK_EQUAL(entry.get_device_function_number(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK_EQUAL(entry.get_bus_address_string(), std::string(""));
}

/// Edge case: structure shorter than the minimum 0x0B length - every
/// accessor must fall back cleanly rather than reading past the buffer.
BOOST_AUTO_TEST_CASE(TruncatedStructureTestCase)
{
    // Only Reference Designation + Device Type present (0x06 total,
    // length < 0x0B).
    std::vector<uint8_t> formatted_area = {0x01, static_cast<uint8_t>(0x80 | OnboardDevicesExtendedInformationEntry::DeviceTypeVideo)};
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area, {"Onboard Video"});
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    OnboardDevicesExtendedInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    // Reference Designation and Device Type are still in bounds.
    BOOST_CHECK_EQUAL(entry.get_reference_designation_string(), std::string("Onboard Video"));
    BOOST_CHECK(entry.is_device_enabled());
    BOOST_CHECK_EQUAL(entry.get_device_type(), OnboardDevicesExtendedInformationEntry::DeviceTypeVideo);

    // Everything beyond the buffer falls back to 0/empty.
    BOOST_CHECK_EQUAL(entry.get_device_type_instance(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bus_number(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_function_number(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bus_address_string(), std::string(""));
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(0x00, 0x00, 0x0000, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(OnboardDevicesExtendedInformationEntry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size()), std::runtime_error);
}

/// OnboardDevicesExtendedInformation resolves through
/// SMBiosEntryFactory::create() to a real
/// OnboardDevicesExtendedInformationEntry, not just via direct
/// construction.
BOOST_AUTO_TEST_CASE(CreateReturnsOnboardDevicesExtendedInformationEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(0x00, 0x00, 0x0000, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::OnboardDevicesExtendedInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<OnboardDevicesExtendedInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
