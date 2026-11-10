#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/system_slots_entry.hpp>
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
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive SystemSlotsEntry (smbios/include/smbios/
// system_slots_entry.hpp) across the header.length tiers named by
// docs/roadmap/0004-.../02.0-.../03-system-slots-and-memory-array.md,
// cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 9
// ("System Slot Information"):
//   tier 1: header.length < 0x0C   -> every length-guarded getter defaults
//   tier 2: header.length == 0x0C  -> designation..characteristics1 decode;
//                                      characteristics2 reads as 0 (not
//                                      skipped); segment/bus/device-func
//                                      still default
//   tier 3: header.length == 0x0D  -> characteristics2 also decodes from
//                                      its own byte; segment/bus/device-func
//                                      still default
//   tier 4: header.length == 0x11  -> segment/bus/device-func also decode
//
// Wire layout (see smbios/include/smbios/system_slots_entry.hpp's
// "@brief 0xNN offset" getter comments):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  designation_index
//   offset 0x05 : uint8_t  slot_type
//   offset 0x06 : uint8_t  data_bus_width
//   offset 0x07 : uint8_t  current_usage
//   offset 0x08 : uint8_t  slot_length
//   offset 0x09 : uint8_t  slot_id_low
//   offset 0x0A : uint8_t  slot_id_high
//   offset 0x0B : uint8_t  characteristics1
//   offset 0x0C : uint8_t  characteristics2
//   offset 0x0D : uint16_t segment_group_number
//   offset 0x0F : uint8_t  bus_number
//   offset 0x10 : uint8_t  device_function_number

BOOST_AUTO_TEST_SUITE(SystemSlotsEntryUnitTests)

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

/// @brief Build a full-length (0x11) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` (via
/// SyntheticTableBuilder::add_structure(), which computes header.length
/// automatically from the formatted area's size). characteristics1/
/// characteristics2/segment/bus/device_function are overridable so each
/// test case can drive the exact branch it needs; `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length,
                                   uint8_t characteristics1 = 0x24, // 3.3V + Cardbus
                                   uint8_t characteristics2 = 0x02, // Hot-plug
                                   uint16_t segment = 0x0000,
                                   uint8_t bus = 0x01,
                                   uint8_t device_function = 0x13) // device 2, function 3
{
    std::vector<uint8_t> full_formatted_area{
        0x01,                                      // 0x04 designation_index
        0x06,                                      // 0x05 slot_type = 0x06 (PCI)
        SystemSlotsEntry::DataBusWidth32Bit,        // 0x06 data_bus_width
        SystemSlotsEntry::Available,                // 0x07 current_usage
        SystemSlotsEntry::Long,                     // 0x08 slot_length
        0x07,                                       // 0x09 slot_id_low
        0x00,                                       // 0x0A slot_id_high
        characteristics1,                           // 0x0B characteristics1
        characteristics2,                           // 0x0C characteristics2

        static_cast<uint8_t>(segment & 0xFF),       // 0x0D segment_group_number low
        static_cast<uint8_t>(segment >> 8),         // 0x0E segment_group_number high
        bus,                                        // 0x0F bus_number
        device_function,                            // 0x10 device_function_number
    };

    BOOST_REQUIRE_EQUAL(full_formatted_area.size(), static_cast<size_t>(0x11 - 4));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), static_cast<size_t>(0x11));
    BOOST_REQUIRE_GE(static_cast<size_t>(length), static_cast<size_t>(4));

    std::vector<uint8_t> formatted_area(full_formatted_area.begin(),
                                         full_formatted_area.begin() + (length - 4));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemSlots, formatted_area);
    return builder.build();
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x0C means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(SystemSlotsEntryTruncatedBelow0x0CDefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_designation_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_slot_type(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_data_bus_width(), static_cast<uint8_t>(SystemSlotsEntry::DataBusWidthOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_current_usage(), static_cast<uint8_t>(SystemSlotsEntry::CurrentUsageOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_slot_length(), static_cast<uint8_t>(SystemSlotsEntry::SlotLengthOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_slot_id_low(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_slot_id_high(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_characteristics1(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_characteristics2(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bus_number(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_function_number(), static_cast<uint8_t>(0));
}

/// Normal case: header.length == 0x0C - designation..characteristics1
/// decode.
BOOST_AUTO_TEST_CASE(SystemSlotsEntryTier0x0CDecodesBaseFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0C);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_designation_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_slot_type(), static_cast<uint8_t>(0x06));
    BOOST_CHECK_EQUAL(entry.get_slot_type_string(), std::string("PCI"));
    BOOST_CHECK_EQUAL(entry.get_data_bus_width(), static_cast<uint8_t>(SystemSlotsEntry::DataBusWidth32Bit));
    BOOST_CHECK_EQUAL(entry.get_current_usage(), static_cast<uint8_t>(SystemSlotsEntry::Available));
    BOOST_CHECK_EQUAL(entry.get_slot_length(), static_cast<uint8_t>(SystemSlotsEntry::Long));
    BOOST_CHECK_EQUAL(entry.get_slot_id_low(), static_cast<uint8_t>(0x07));
    BOOST_CHECK_EQUAL(entry.get_characteristics1(), static_cast<uint8_t>(0x24));

    // length == 0x0C, characteristics2 reads as 0 (not skipped) per the
    // reference's own fallback, segment/bus/device-func (need length >=
    // 0x11) default.
    BOOST_CHECK_EQUAL(entry.get_characteristics2(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(0));
}

/// Edge case: header.length == 0x0D - characteristics2 decodes from its own
/// byte; segment/bus/device-func (need length >= 0x11) still default.
BOOST_AUTO_TEST_CASE(SystemSlotsEntryTier0x0DDecodesCharacteristics2TestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0D);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_characteristics2(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bus_number(), static_cast<uint8_t>(0));
}

/// Normal case: header.length == 0x11 (full buffer) - segment/bus/device-
/// func also decode.
BOOST_AUTO_TEST_CASE(SystemSlotsEntryTier0x11DecodesSegmentBusDeviceFuncTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x11);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_segment_group_number(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bus_number(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_device_function_number(), static_cast<uint8_t>(0x13));
    BOOST_CHECK_EQUAL(entry.get_device_number(), static_cast<uint8_t>(2));
    BOOST_CHECK_EQUAL(entry.get_function_number(), static_cast<uint8_t>(3));
    BOOST_CHECK_EQUAL(entry.get_bus_address_string(), std::string("0000:01:02.3"));
}

/// Regression/edge case: bus address all-FF/all-FFFF sentinel means "not
/// applicable" - get_bus_address_string() must render empty, not a literal
/// "ffff:ff:1f.7" address.
BOOST_AUTO_TEST_CASE(BusAddressAllFFSentinelRendersEmptyTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x11, 0x24, 0x02, 0xFFFF, 0xFF, 0xFF);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_bus_address_string(), std::string(""));
}

/// Characteristics rendering, branch 1: bit 0 of characteristics1 set means
/// the whole field is unknown, regardless of other bits.
BOOST_AUTO_TEST_CASE(CharacteristicsUnknownBitRendersUnknownTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0D, 0x01, 0x00);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_characteristics_string(), std::string("\tUnknown\n"));
}

/// Characteristics rendering, branch 2: both bytes entirely zero (apart
/// from the unknown bit, which is clear here) renders "None".
BOOST_AUTO_TEST_CASE(CharacteristicsAllZeroRendersNoneTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0D, 0x00, 0x00);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_characteristics_string(), std::string("\tNone\n"));
}

/// Characteristics rendering, branch 3: a normal combination of
/// characteristics1/characteristics2 bits lists each set flag.
BOOST_AUTO_TEST_CASE(CharacteristicsListsSetBitsFromBothBytesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0D, 0x24, 0x02);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    const std::string characteristics = entry.get_characteristics_string();
    BOOST_CHECK(characteristics.find("3.3 V is provided") != std::string::npos);
    BOOST_CHECK(characteristics.find("Cardbus is supported") != std::string::npos);
    BOOST_CHECK(characteristics.find("Hot-plug devices are supported") != std::string::npos);
}

/// Slot ID string representation, branch 1: high byte zero renders the low
/// byte alone.
BOOST_AUTO_TEST_CASE(SlotIdStringRendersLowByteAloneWhenHighIsZeroTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0C);
    DMIHeader header = make_header(buffer);

    SystemSlotsEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_slot_id_string(), std::string("7"));
}

/// Invalid-input case: constructing with a non-SystemSlots header type must
/// throw, not silently misparse.
BOOST_AUTO_TEST_CASE(WrongHeaderTypeThrowsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0C);
    buffer[0] = SMBios::BIOSInformation; // deliberately wrong type
    DMIHeader header = make_header(buffer);

    BOOST_CHECK_THROW(SystemSlotsEntry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0004 Story 02.0's own success criterion: SystemSlots resolves
/// through SMBiosEntryFactory::create() to a real SystemSlotsEntry, not
/// just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemSlotsEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x11);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemSlotsEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009, Story 01.0, Task 04)
//
// Type: unit - deterministic, hand-built buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description() field by field;
// enumerated values, Slot ID and Bus Address stay the printed strings, and
// Characteristics is either the text's single "Unknown"/"None" sentinel
// field or one {label, true} field per set bit of characteristics 1 then 2.
// This class stops at the 2.6 segment/bus/device-function fields (see its
// header's scope cutoff), so there are no peer-group fields to index.
// Boundaries: the Unknown bit, all-zero characteristics, every labelled bit
// of both bytes, the 0x0C/0x0D/0x11 length tiers, the all-FF bus address.
// On failure first check: SystemSlotsEntry::get_fields() and
// characteristics_sentinel() in libsmbios/src/system_slots_entry.cpp.

namespace {

/// @brief Re-emit build_buffer()'s formatted area as a well-formed structure
/// carrying one designation DMI string.
std::vector<uint8_t> with_strings(const std::vector<uint8_t>& buffer)
{
    const std::vector<uint8_t> formatted_area(buffer.begin() + 4, buffer.begin() + buffer[1]);
    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemSlots, formatted_area, std::vector<std::string>{"PCIE1"});
    return builder.build();
}

} // namespace

/// Normal case: a full (0x11) structure - every text label becomes a field,
/// in text order, with one true field per set characteristics bit.
BOOST_AUTO_TEST_CASE(SystemSlotsEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    std::vector<uint8_t> buffer = with_strings(build_buffer(0x11));
    buffer[0x0A] = 0x02; // slot_id_high -> "7/2"
    SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Designation", str("PCIE1")},
        {"Type", str("PCI")},
        {"Data Bus Width", str("32 bit")},
        {"Current Usage", str("Available")},
        {"Length", str("Long")},
        {"ID", str("7/2")},
        {"3.3 V is provided", true},
        {"Cardbus is supported", true},
        {"Hot-plug devices are supported", true},
        {"Bus Address", str("0000:01:02.3")},
    });
}

/// Edge cases: every labelled bit of both bytes expands to its own true
/// field in text order, never a joined string; the Unknown bit and the
/// all-zero value each become the single "Characteristics" sentinel field.
BOOST_AUTO_TEST_CASE(SystemSlotsEntryGetFieldsExpandsBitmaskTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> buffer = with_strings(build_buffer(0x11, 0xFE, 0xFF));
        SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 6}, buffer.data() + buffer.size());

        const std::vector<NamedField> fields = entry.get_fields();
        BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(6 + 7 + 8 + 1));
        const std::vector<NamedField> bits(fields.begin() + 6, fields.end() - 1);
        check_fields_equal(bits, {
            {"5.0 V is provided", true},
            {"3.3 V is provided", true},
            {"Opening is shared", true},
            {"PC Card-16 is supported", true},
            {"Cardbus is supported", true},
            {"Zoom Video is supported", true},
            {"Modem ring resume is supported", true},
            {"PME signal is supported", true},
            {"Hot-plug devices are supported", true},
            {"SMBus signal is supported", true},
            {"PCIe slot bifurcation is supported", true},
            {"Async/surprise removal is supported", true},
            {"Flexbus slot, CXL 1.0 capable", true},
            {"Flexbus slot, CXL 2.0 capable", true},
            {"Flexbus slot, CXL 3.0 capable", true},
        });
        check_unique_field_names(fields);
        check_no_joined_string_fields(fields);
    }
    {
        // Bit 0 ("Unknown") wins over every other set bit, as in the text.
        std::vector<uint8_t> buffer = with_strings(build_buffer(0x11, 0x25, 0x02));
        SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 6}, buffer.data() + buffer.size());

        const std::vector<NamedField> fields = entry.get_fields();
        BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(8));
        BOOST_CHECK_EQUAL(fields[6].name, std::string("Characteristics"));
        BOOST_CHECK(fields[6].value == str("Unknown"));
        check_unique_field_names(fields);
        check_no_joined_string_fields(fields);
    }
    {
        std::vector<uint8_t> buffer = with_strings(build_buffer(0x11, 0x00, 0x00));
        SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 6}, buffer.data() + buffer.size());

        const std::vector<NamedField> fields = entry.get_fields();
        BOOST_REQUIRE_EQUAL(fields.size(), static_cast<size_t>(8));
        BOOST_CHECK_EQUAL(fields[6].name, std::string("Characteristics"));
        BOOST_CHECK(fields[6].value == str("None"));
        check_unique_field_names(fields);
        check_no_joined_string_fields(fields);
    }
}

/// Boundary cases: the text prints every line unconditionally. At 0x0C
/// characteristics2 reads as 0 (its bit drops out) and the Bus Address line
/// is present but empty; at 0x0D characteristics2 participates; an all-FF
/// address at 0x11 is also the empty string.
BOOST_AUTO_TEST_CASE(SystemSlotsEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> buffer = with_strings(build_buffer(0x0C));
        SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Designation", str("PCIE1")},
            {"Type", str("PCI")},
            {"Data Bus Width", str("32 bit")},
            {"Current Usage", str("Available")},
            {"Length", str("Long")},
            {"ID", str("7")},
            {"3.3 V is provided", true},
            {"Cardbus is supported", true},
            {"Bus Address", str("")},
        });
    }
    {
        std::vector<uint8_t> buffer = with_strings(build_buffer(0x0D));
        SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 1}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Designation", str("PCIE1")},
            {"Type", str("PCI")},
            {"Data Bus Width", str("32 bit")},
            {"Current Usage", str("Available")},
            {"Length", str("Long")},
            {"ID", str("7")},
            {"3.3 V is provided", true},
            {"Cardbus is supported", true},
            {"Hot-plug devices are supported", true},
            {"Bus Address", str("")},
        });
    }
    {
        std::vector<uint8_t> buffer = with_strings(build_buffer(0x11, 0x24, 0x02, 0xFFFF, 0xFF, 0xFF));
        SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 6}, buffer.data() + buffer.size());

        const std::vector<NamedField> fields = entry.get_fields();
        BOOST_REQUIRE(!fields.empty());
        BOOST_CHECK_EQUAL(fields.back().name, std::string("Bus Address"));
        BOOST_CHECK(fields.back().value == str(""));
        check_unique_field_names(fields);
    }
    {
        // Bare header: every getter defaults, every line still prints.
        std::vector<uint8_t> buffer = build_buffer(0x04);
        SystemSlotsEntry entry(make_header(buffer), SMBiosVersion{2, 0}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Designation", str("Not Specified")},
            {"Type", str("OutOfSpec")},
            {"Data Bus Width", str("OutOfSpec")},
            {"Current Usage", str("OutOfSpec")},
            {"Length", str("OutOfSpec")},
            {"ID", str("0")},
            {"Characteristics", str("None")},
            {"Bus Address", str("")},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
