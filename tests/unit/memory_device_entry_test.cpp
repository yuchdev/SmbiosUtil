#include <cstdint>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/memory_device_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive MemoryDeviceEntry (smbios/include/smbios/
// memory_device_entry.h) across the header.length tiers named by
// docs/roadmap/0003-.../02.0-.../02-memory-device-field-table.md:
//   tier 1: header.length < 0x15   -> every length-guarded getter defaults
//   tier 2: 0x15 <= length <= 0x16 -> fields through type_detail (0x13-0x14)
//                                      decode; device_speed onward defaults
//   tier 3: 0x17 <= length <= 0x1A -> device_speed (0x15) decodes at 0x17;
//                                      manufacturer/serial/asset decode as
//                                      length grows toward 0x1A; part_number
//                                      (0x1A) still needs length >= 0x1B
//   tier 4: length == 0x1B          -> part_number also decodes; device_rank
//                                      (0x1B) still needs length >= 0x1C
//   tier 5: 0x1C <= length <= 0x1F  -> device_rank decodes; extended_size
//                                      (0x1C) still needs length >= 0x20
//   tier 5b: length == 0x20         -> extended_size also decodes;
//                                      memory_clock_speed (0x20) still needs
//                                      length >= 0x22
//   tier 6: length >= 0x22          -> memory_clock_speed decodes; minimum/
//                                      maximum/configured voltage decode as
//                                      length reaches 0x24/0x26/0x28
//
// Wire layout (see smbios/include/smbios/memory_device_entry.h's
// "@brief 0xNN offset" getter comments, re-verified against
// c-versions/dmidecode_linux/dmidecode.c's dmi_decode() case 17 for this
// task - see 02-memory-device-field-table.md):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint16_t array_handle
//   offset 0x06 : uint16_t array_error_handle
//   offset 0x08 : uint16_t total_width
//   offset 0x0A : uint16_t data_width
//   offset 0x0C : uint16_t device_size
//   offset 0x0E : uint8_t  device_form_factor
//   offset 0x0F : uint8_t  device_set
//   offset 0x10 : uint8_t  device_locator_index
//   offset 0x11 : uint8_t  bank_locator_index
//   offset 0x12 : uint8_t  device_type
//   offset 0x13 : uint16_t type_detail
//   offset 0x15 : uint16_t device_speed
//   offset 0x17 : uint8_t  manufacturer_index
//   offset 0x18 : uint8_t  serial_number_index
//   offset 0x19 : uint8_t  asset_tag_index
//   offset 0x1A : uint8_t  part_number_index
//   offset 0x1B : uint8_t  device_rank
//   offset 0x1C : uint32_t extended_size
//   offset 0x20 : uint16_t memory_clock_speed
//   offset 0x22 : uint16_t minimum_voltage
//   offset 0x24 : uint16_t maximum_voltage
//   offset 0x26 : uint16_t configured_voltage

BOOST_AUTO_TEST_SUITE(MemoryDeviceEntryUnitTests)

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
/// terminator (double NUL) so AbstractSMBiosEntry::parse_dmi_strings()
/// has a well-formed section to walk and does not run past the buffer.
void append_string_terminator(std::vector<uint8_t>& buffer)
{
    buffer.push_back(0x00);
    buffer.push_back(0x00);
}

/// @brief Build a full-length (0x28) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` and set
/// the header's own length byte accordingly. `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length)
{
    std::vector<uint8_t> full;
    full.push_back(SMBios::MemoryDevice); // 0x00 type = 17
    full.push_back(0x00);                 // 0x01 length (patched below)
    full.push_back(0x01);                 // 0x02 handle low byte
    full.push_back(0x00);                 // 0x03 handle high byte

    full.push_back(0x01); full.push_back(0x00); // 0x04-0x05 array_handle = 0x0001
    full.push_back(0xFF); full.push_back(0xFF); // 0x06-0x07 array_error_handle = NoError
    full.push_back(0x48); full.push_back(0x00); // 0x08-0x09 total_width = 72
    full.push_back(0x40); full.push_back(0x00); // 0x0A-0x0B data_width = 64
    full.push_back(0x00); full.push_back(0x20); // 0x0C-0x0D device_size = 0x2000
    full.push_back(0x09);                        // 0x0E form_factor = DIMM
    full.push_back(0x00);                        // 0x0F device_set = None
    full.push_back(0x01);                        // 0x10 device_locator_index
    full.push_back(0x02);                        // 0x11 bank_locator_index
    full.push_back(0x1A);                        // 0x12 device_type = DDR4
    full.push_back(0x80); full.push_back(0x00); // 0x13-0x14 type_detail = Synchronous

    full.push_back(0x60); full.push_back(0x09); // 0x15-0x16 device_speed = 2400
    full.push_back(0x03);                        // 0x17 manufacturer_index
    full.push_back(0x04);                        // 0x18 serial_number_index
    full.push_back(0x05);                        // 0x19 asset_tag_index
    full.push_back(0x06);                        // 0x1A part_number_index
    full.push_back(0x04);                        // 0x1B device_rank

    full.push_back(0x00); full.push_back(0x00); full.push_back(0x02); full.push_back(0x00); // 0x1C-0x1F extended_size = 0x00020000
    full.push_back(0x75); full.push_back(0x0B); // 0x20-0x21 memory_clock_speed = 2933
    full.push_back(0xB0); full.push_back(0x04); // 0x22-0x23 minimum_voltage = 1200
    full.push_back(0x46); full.push_back(0x05); // 0x24-0x25 maximum_voltage = 1350
    full.push_back(0xB0); full.push_back(0x04); // 0x26-0x27 configured_voltage = 1200

    BOOST_REQUIRE_EQUAL(full.size(), static_cast<size_t>(0x28));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), full.size());

    std::vector<uint8_t> buffer(full.begin(), full.begin() + length);
    buffer[1] = length;

    append_string_terminator(buffer);
    return buffer;
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x15 means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryTruncatedBelow0x15DefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_array_handle(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_error_handle(), static_cast<uint16_t>(MemoryDeviceEntry::ErrorHandleNotProvided));
    BOOST_CHECK_EQUAL(entry.get_total_width(), static_cast<uint16_t>(MemoryDeviceEntry::DataWidthUnknown1));
    BOOST_CHECK_EQUAL(entry.get_data_width(), static_cast<uint16_t>(MemoryDeviceEntry::DataWidthUnknown1));
    BOOST_CHECK_EQUAL(entry.get_device_size(), static_cast<uint16_t>(MemoryDeviceEntry::DeviceSizeUnknown));
    BOOST_CHECK_EQUAL(entry.get_form_factor(), static_cast<uint8_t>(MemoryDeviceEntry::FormFactorOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_device_set(), static_cast<uint8_t>(MemoryDeviceEntry::DeviceSetUnknown));
    BOOST_CHECK_EQUAL(entry.get_device_locator_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bank_locator_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<uint8_t>(MemoryDeviceEntry::DeviceTypeOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_device_detail(), static_cast<uint16_t>(MemoryDeviceEntry::DevicePropertiesOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_device_speed(), static_cast<uint16_t>(MemoryDeviceEntry::DeviceSpeedUnknown));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_part_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_rank(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_extended_size(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_memory_clock_speed(), static_cast<uint16_t>(MemoryDeviceEntry::DeviceSpeedUnknown));
    BOOST_CHECK_EQUAL(entry.get_minimum_voltage(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_maximum_voltage(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_configured_voltage(), static_cast<uint16_t>(0));
}

/// Normal case: header.length == 0x15 - fields through type_detail (0x13-0x14)
/// decode; device_speed (needs length >= 0x17) still defaults.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryTier0x15DecodesThroughTypeDetailTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x15);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_array_handle(), static_cast<uint16_t>(0x0001));
    BOOST_CHECK_EQUAL(entry.get_error_handle(), static_cast<uint16_t>(MemoryDeviceEntry::ErrorHandleNoError));
    BOOST_CHECK_EQUAL(entry.get_total_width(), static_cast<uint16_t>(72));
    BOOST_CHECK_EQUAL(entry.get_data_width(), static_cast<uint16_t>(64));
    BOOST_CHECK_EQUAL(entry.get_device_size(), static_cast<uint16_t>(0x2000));
    BOOST_CHECK_EQUAL(entry.get_form_factor(), static_cast<uint8_t>(MemoryDeviceEntry::DIMM));
    BOOST_CHECK_EQUAL(entry.get_device_set(), static_cast<uint8_t>(MemoryDeviceEntry::DeviceSetNone));
    BOOST_CHECK_EQUAL(entry.get_device_locator_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_bank_locator_index(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<uint8_t>(MemoryDeviceEntry::DDR4));
    BOOST_CHECK_EQUAL(entry.get_device_detail(), static_cast<uint16_t>(MemoryDeviceEntry::Synchronous));

    // length == 0x15, device_speed needs length >= 0x17 -> defaults
    BOOST_CHECK_EQUAL(entry.get_device_speed(), static_cast<uint16_t>(MemoryDeviceEntry::DeviceSpeedUnknown));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_rank(), static_cast<uint8_t>(0));
}

/// Regression test for the OOB-read bug this redesign closes: the OLD
/// constructor assigned `memory_device_v23_` (struct size 0x1B) once
/// `header.length >= 0x17`, so manufacturer/serial/asset/part_number
/// (offsets 0x17-0x1A) could be read out of bounds whenever
/// 0x17 <= header.length < 0x1B. With length == 0x17 (the old threshold
/// itself), device_speed (0x15, needs length >= 0x17) now correctly decodes,
/// but manufacturer_index (0x17, needs length >= 0x18) correctly still
/// defaults instead of reading past the buffer.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryTier0x17DecodesSpeedNotManufacturerTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x17);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_device_speed(), static_cast<uint16_t>(2400));

    // the formerly out-of-bounds-readable fields must all default here
    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_part_number_index(), static_cast<uint8_t>(0));
}

/// Edge case: header.length == 0x1A - manufacturer/serial/asset decode;
/// part_number (0x1A, needs length >= 0x1B) and device_rank still default.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryTier0x1ADecodesThroughAssetTagTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0x04));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0x05));

    // length == 0x1A, part_number needs length >= 0x1B -> defaults
    BOOST_CHECK_EQUAL(entry.get_part_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_device_rank(), static_cast<uint8_t>(0));
}

/// Edge case: header.length == 0x1B - part_number also decodes;
/// device_rank (0x1B, needs length >= 0x1C) still defaults.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryTier0x1BDecodesPartNumberTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1B);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_part_number_index(), static_cast<uint8_t>(0x06));

    // length == 0x1B, device_rank needs length >= 0x1C -> defaults
    BOOST_CHECK_EQUAL(entry.get_device_rank(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_extended_size(), static_cast<uint32_t>(0));
}

/// Edge case: header.length == 0x1C - device_rank also decodes;
/// extended_size (0x1C, needs length >= 0x20) still defaults.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryTier0x1CDecodesDeviceRankTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1C);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_device_rank(), static_cast<uint8_t>(0x04));

    // length == 0x1C, extended_size needs length >= 0x20 -> defaults
    BOOST_CHECK_EQUAL(entry.get_extended_size(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_memory_clock_speed(), static_cast<uint16_t>(MemoryDeviceEntry::DeviceSpeedUnknown));
}

/// Edge case: header.length == 0x20 - extended_size also decodes;
/// memory_clock_speed (0x20, needs length >= 0x22) still defaults.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryTier0x20DecodesExtendedSizeTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x20);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 7}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_extended_size(), static_cast<uint32_t>(0x00020000));

    // length == 0x20, memory_clock_speed needs length >= 0x22 -> defaults
    BOOST_CHECK_EQUAL(entry.get_memory_clock_speed(), static_cast<uint16_t>(MemoryDeviceEntry::DeviceSpeedUnknown));
    BOOST_CHECK_EQUAL(entry.get_minimum_voltage(), static_cast<uint16_t>(0));
}

/// Normal case: header.length == 0x28 (full buffer, >= 0x22 tier) - every
/// field including memory_clock_speed and minimum/maximum/configured
/// voltage decodes to its known byte value.
BOOST_AUTO_TEST_CASE(MemoryDeviceEntryFullBufferDecodesAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x28);
    DMIHeader header = make_header(buffer);

    MemoryDeviceEntry entry(header, SMBiosVersion{2, 8}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_array_handle(), static_cast<uint16_t>(0x0001));
    BOOST_CHECK_EQUAL(entry.get_error_handle(), static_cast<uint16_t>(MemoryDeviceEntry::ErrorHandleNoError));
    BOOST_CHECK_EQUAL(entry.get_total_width(), static_cast<uint16_t>(72));
    BOOST_CHECK_EQUAL(entry.get_data_width(), static_cast<uint16_t>(64));
    BOOST_CHECK_EQUAL(entry.get_device_size(), static_cast<uint16_t>(0x2000));
    BOOST_CHECK_EQUAL(entry.get_form_factor(), static_cast<uint8_t>(MemoryDeviceEntry::DIMM));
    BOOST_CHECK_EQUAL(entry.get_device_set(), static_cast<uint8_t>(MemoryDeviceEntry::DeviceSetNone));
    BOOST_CHECK_EQUAL(entry.get_device_locator_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_bank_locator_index(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_device_type(), static_cast<uint8_t>(MemoryDeviceEntry::DDR4));
    BOOST_CHECK_EQUAL(entry.get_device_detail(), static_cast<uint16_t>(MemoryDeviceEntry::Synchronous));
    BOOST_CHECK_EQUAL(entry.get_device_speed(), static_cast<uint16_t>(2400));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0x04));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0x05));
    BOOST_CHECK_EQUAL(entry.get_part_number_index(), static_cast<uint8_t>(0x06));
    BOOST_CHECK_EQUAL(entry.get_device_rank(), static_cast<uint8_t>(0x04));
    BOOST_CHECK_EQUAL(entry.get_extended_size(), static_cast<uint32_t>(0x00020000));
    BOOST_CHECK_EQUAL(entry.get_memory_clock_speed(), static_cast<uint16_t>(2933));
    BOOST_CHECK_EQUAL(entry.get_minimum_voltage(), static_cast<uint16_t>(1200));
    BOOST_CHECK_EQUAL(entry.get_maximum_voltage(), static_cast<uint16_t>(1350));
    BOOST_CHECK_EQUAL(entry.get_configured_voltage(), static_cast<uint16_t>(1200));
}

BOOST_AUTO_TEST_SUITE_END()
