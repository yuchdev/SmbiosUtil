#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/system_slots_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive SystemSlotsEntry (smbios/include/smbios/
// system_slots_entry.h) across the header.length tiers named by
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
// Wire layout (see smbios/include/smbios/system_slots_entry.h's
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

/// @brief Append a minimal well-formed (empty) DMI string-section
/// terminator (double NUL) so AbstractSMBiosEntry::parse_dmi_strings() has a
/// well-formed section to walk and does not run past the buffer.
void append_string_terminator(std::vector<uint8_t>& buffer)
{
    buffer.push_back(0x00);
    buffer.push_back(0x00);
}

/// @brief Build a full-length (0x11) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` and patch
/// the header's own length byte accordingly. characteristics1/
/// characteristics2/segment/bus/device_function are overridable so each
/// test case can drive the exact branch it needs; `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length,
                                   uint8_t characteristics1 = 0x24, // 3.3V + Cardbus
                                   uint8_t characteristics2 = 0x02, // Hot-plug
                                   uint16_t segment = 0x0000,
                                   uint8_t bus = 0x01,
                                   uint8_t device_function = 0x13) // device 2, function 3
{
    std::vector<uint8_t> full;
    full.push_back(SMBios::SystemSlots); // 0x00 type = 9
    full.push_back(0x00);                // 0x01 length (patched below)
    full.push_back(0x01);                // 0x02 handle low byte
    full.push_back(0x00);                // 0x03 handle high byte

    full.push_back(0x01);                                         // 0x04 designation_index
    full.push_back(0x06);                                          // 0x05 slot_type = 0x06 (PCI)
    full.push_back(SystemSlotsEntry::DataBusWidth32Bit);           // 0x06 data_bus_width
    full.push_back(SystemSlotsEntry::Available);                   // 0x07 current_usage
    full.push_back(SystemSlotsEntry::Long);                        // 0x08 slot_length
    full.push_back(0x07);                                          // 0x09 slot_id_low
    full.push_back(0x00);                                          // 0x0A slot_id_high
    full.push_back(characteristics1);                              // 0x0B characteristics1
    full.push_back(characteristics2);                              // 0x0C characteristics2

    full.push_back(static_cast<uint8_t>(segment & 0xFF));          // 0x0D segment_group_number low
    full.push_back(static_cast<uint8_t>(segment >> 8));            // 0x0E segment_group_number high
    full.push_back(bus);                                           // 0x0F bus_number
    full.push_back(device_function);                               // 0x10 device_function_number

    BOOST_REQUIRE_EQUAL(full.size(), static_cast<size_t>(0x11));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), full.size());

    std::vector<uint8_t> buffer(full.begin(), full.begin() + length);
    buffer[1] = length;

    append_string_terminator(buffer);
    return buffer;
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

BOOST_AUTO_TEST_SUITE_END()
