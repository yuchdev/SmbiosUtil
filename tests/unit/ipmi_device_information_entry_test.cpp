#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/ipmi_device_information_entry.hpp>
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
// smbios::test::SyntheticTableBuilder) drive IpmiDeviceInformationEntry
// (smbios/include/smbios/tables/ipmi_device_information_entry.hpp),
// cross-checked directly against DSP0134 3.9.0's own 7.39/7.39.1 (Table
// 114/115), including the confirmed 0x10/0x11 redacted-gap resolution (see
// the header's own doc comment).
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  interface_type (enum, Table 115)
//   offset 0x05 : uint8_t  specification_revision_bcd
//   offset 0x06 : uint8_t  i2c_target_address
//   offset 0x07 : uint8_t  nv_storage_device_address (0xFF = none)
//   offset 0x08 : uint64_t base_address (bit 0 = I/O space flag)
//   offset 0x10 : uint8_t  base_address_modifier / interrupt_info
//   offset 0x11 : uint8_t  interrupt_number (0x00 = unspecified/unsupported)

BOOST_AUTO_TEST_SUITE(IpmiDeviceInformationEntryUnitTests)

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

std::vector<uint8_t> make_formatted_area(uint8_t interface_type, uint8_t revision_bcd,
                                          uint8_t i2c_address, uint8_t nv_storage_address,
                                          uint64_t base_address, uint8_t modifier_and_interrupt_info,
                                          uint8_t interrupt_number)
{
    std::vector<uint8_t> formatted_area;
    formatted_area.push_back(interface_type);       // 0x04
    formatted_area.push_back(revision_bcd);          // 0x05
    formatted_area.push_back(i2c_address);            // 0x06
    formatted_area.push_back(nv_storage_address);     // 0x07
    for (int i = 0; i < 8; ++i) {                      // 0x08-0x0F
        formatted_area.push_back(static_cast<uint8_t>((base_address >> (8 * i)) & 0xFF));
    }
    formatted_area.push_back(modifier_and_interrupt_info); // 0x10
    formatted_area.push_back(interrupt_number);             // 0x11
    return formatted_area;
}

} // namespace

/// Normal case: Base Address bit 0 set -> I/O space.
BOOST_AUTO_TEST_CASE(BaseAddressIoSpaceTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeKcs, 0x10, 0x20, 0xFF,
        /*base_address*/ 0xCA1 | 0x1, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    IpmiDeviceInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interface_type(), IpmiDeviceInformationEntry::InterfaceTypeKcs);
    BOOST_CHECK_EQUAL(entry.get_interface_type_string(), std::string("KCS: Keyboard Controller Style"));
    BOOST_CHECK_EQUAL(entry.get_specification_revision_bcd(), static_cast<uint8_t>(0x10));
    BOOST_CHECK_EQUAL(entry.get_specification_revision(), static_cast<uint8_t>(10));
    BOOST_CHECK(entry.is_base_address_io_space());
}

/// Normal case: Base Address bit 0 clear -> memory-mapped.
BOOST_AUTO_TEST_CASE(BaseAddressMemoryMappedTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeBt, 0x20, 0x30, 0xFF,
        /*base_address*/ 0xE000'0000ULL, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    IpmiDeviceInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interface_type(), IpmiDeviceInformationEntry::InterfaceTypeBt);
    BOOST_CHECK_EQUAL(entry.get_base_address(), static_cast<uint64_t>(0xE000'0000ULL));
    BOOST_CHECK(!entry.is_base_address_io_space());
}

/// Sentinel case: NV Storage Device Address == 0xFF -> has_nv_storage_device() false.
BOOST_AUTO_TEST_CASE(NoNvStorageDeviceSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeSsif, 0x11, 0x10, 0xFF, 0x0, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    IpmiDeviceInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_nv_storage_device_address(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK(!entry.has_nv_storage_device());
}

/// Normal case (distinct from the sentinel case): a real NV storage device
/// address -> has_nv_storage_device() true.
BOOST_AUTO_TEST_CASE(HasNvStorageDeviceTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeSsif, 0x11, 0x10, 0x42, 0x0, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    IpmiDeviceInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_nv_storage_device_address(), static_cast<uint8_t>(0x42));
    BOOST_CHECK(entry.has_nv_storage_device());
}

/// Sentinel case: Interrupt Number == 0x00 -> has_interrupt() false.
BOOST_AUTO_TEST_CASE(NoInterruptSentinelTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeKcs, 0x10, 0x10, 0xFF, 0x0, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    IpmiDeviceInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interrupt_number(), static_cast<uint8_t>(0x00));
    BOOST_CHECK(!entry.has_interrupt());
}

/// Normal case (distinct from the sentinel case): a real interrupt number
/// -> has_interrupt() true, plus the Interrupt Info sub-fields of the
/// confirmed 0x10 byte (REDACTED-GAP RESOLUTION).
BOOST_AUTO_TEST_CASE(HasInterruptAndInterruptInfoSubFieldsTestCase)
{
    // 0x10 byte: bits 7:6 register spacing = 01b (32-bit boundaries),
    // bit 4 address bit0 = 1, bit 3 interrupt info specified = 1,
    // bit 1 polarity = 1 (active high), bit 0 trigger mode = 1 (level).
    const uint8_t modifier = 0b0101'1011;
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeKcs, 0x10, 0x10, 0xFF, 0x0, modifier, 0x09);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    IpmiDeviceInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interrupt_number(), static_cast<uint8_t>(0x09));
    BOOST_CHECK(entry.has_interrupt());

    BOOST_CHECK_EQUAL(entry.get_base_address_modifier_and_interrupt_info(), modifier);
    BOOST_CHECK_EQUAL(entry.get_register_spacing(), static_cast<uint8_t>(0b01));
    BOOST_CHECK(entry.get_address_bit0());
    BOOST_CHECK(entry.has_interrupt_info());
    BOOST_CHECK(entry.get_interrupt_polarity_active_high());
    BOOST_CHECK(entry.get_interrupt_trigger_mode_level());
}

/// Edge case: minimum-length buffer (no formatted fields at all) -> every
/// getter defaults safely.
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    IpmiDeviceInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_interface_type(), IpmiDeviceInformationEntry::InterfaceTypeOutOfSpec);
    BOOST_CHECK_EQUAL(entry.get_specification_revision_bcd(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_i2c_target_address(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_nv_storage_device_address(), static_cast<uint8_t>(0xFF));
    BOOST_CHECK(!entry.has_nv_storage_device());
    BOOST_CHECK_EQUAL(entry.get_base_address(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_interrupt_number(), static_cast<uint8_t>(0));
    BOOST_CHECK(!entry.has_interrupt());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeKcs, 0x10, 0x10, 0xFF, 0x0, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(IpmiDeviceInformationEntry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 05.0 Task 03's own success criterion:
/// IpmiDeviceInformation resolves through SMBiosEntryFactory::create() to a
/// real IpmiDeviceInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsIpmiDeviceInformationEntryTestCase)
{
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeKcs, 0x10, 0x10, 0xFF, 0x0, 0x00, 0x00);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<IpmiDeviceInformationEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009 Story 01.0 Task 05)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description(); plain decimals
// are uint64_t, the three decorated lines stay their printed strings.
// Boundaries: NV storage / interrupt sentinels, I/O vs memory-mapped base
// address, header-only structure. The Base Address Modifier bits are not
// printed, so they are not fields and there is no ExpandsBitmask case.
// On failure first check: IpmiDeviceInformationEntry::get_fields() against
// IpmiDeviceInformationEntry::render_to_description() in libsmbios/src/ipmi_device_information_entry.cpp.

/// Normal case: every field present, NV storage and interrupt real, base
/// address in I/O space.
BOOST_AUTO_TEST_CASE(IpmiDeviceInformationEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> formatted_area = make_formatted_area(
        IpmiDeviceInformationEntry::InterfaceTypeKcs, 0x20, 0x20, 0x10,
        /*base_address*/ 0xCA3, 0x98, 0x0A);
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
    std::vector<uint8_t> buffer = builder.build();
    IpmiDeviceInformationEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Interface Type", str("KCS: Keyboard Controller Style")},
        {"Specification Revision", num(20)},
        {"I2C Target Address", num(0x20)},
        {"NV Storage Device Address", str("16")},
        {"Base Address", str("3235 (I/O space)")},
        {"Interrupt Number", str("10")},
    });
}

/// Boundary cases: the 0xFF / 0x00 sentinels and a memory-mapped base
/// address keep the text's decorations; a header-only structure keeps all
/// six always-printed fields with their defaults.
BOOST_AUTO_TEST_CASE(IpmiDeviceInformationEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> formatted_area = make_formatted_area(
            IpmiDeviceInformationEntry::InterfaceTypeBt, 0x15, 0x30, 0xFF,
            /*base_address*/ 0xE0000000ULL, 0x00, 0x00);
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::IpmiDeviceInformation, formatted_area);
        std::vector<uint8_t> buffer = builder.build();
        IpmiDeviceInformationEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Interface Type", str("BT: Block Transfer")},
            {"Specification Revision", num(15)},
            {"I2C Target Address", num(0x30)},
            {"NV Storage Device Address", str("255 (none)")},
            {"Base Address", str("3758096384 (memory-mapped)")},
            {"Interrupt Number", str("0 (unspecified/unsupported)")},
        });
    }
    {
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::IpmiDeviceInformation, std::vector<uint8_t>{});
        std::vector<uint8_t> buffer = builder.build();
        IpmiDeviceInformationEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Interface Type", str("Reserved")},
            {"Specification Revision", num(0)},
            {"I2C Target Address", num(0)},
            {"NV Storage Device Address", str("255 (none)")},
            {"Base Address", str("0 (memory-mapped)")},
            {"Interrupt Number", str("0 (unspecified/unsupported)")},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
