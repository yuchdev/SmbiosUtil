#include <cstdint>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/bios_information_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive BiosInformationEntry (smbios/include/smbios/
// bios_information_entry.h) across the three header.length tiers the field
// table (docs/roadmap/0003-.../02.0-.../01-bios-information-field-table.md)
// names:
//   tier 1: header.length < 0x12   -> every length-guarded getter defaults
//   tier 2: 0x12 <= length <= 0x18 -> fields through firmware_minor_release
//                                     decode, get_extended_rom_size() defaults
//   tier 3: header.length > 0x18   -> get_extended_rom_size() also decodes
//
// Wire layout (see smbios/include/smbios/bios_information_entry.h's
// "@brief 0xNN offset" getter comments, re-verified against
// c-versions/dmidecode_linux/dmidecode.c's dmi_decode() case 0 for this
// task - see 01-bios-information-field-table.md's Step 3):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  vendor_index
//   offset 0x05 : uint8_t  version_index
//   offset 0x06 : uint16_t starting_segment (also backs get_runtime_size())
//   offset 0x08 : uint8_t  release_date_index
//   offset 0x09 : uint8_t  rom_size (0xFF sentinel -> get_rom_size() == 0)
//   offset 0x0A : uint64_t properties
//   offset 0x12 : uint8_t  properties_extension1
//   offset 0x13 : uint8_t  properties_extension2
//   offset 0x14 : uint8_t  bios_major_release
//   offset 0x15 : uint8_t  bios_minor_release
//   offset 0x16 : uint8_t  firmware_major_release
//   offset 0x17 : uint8_t  firmware_minor_release
//   offset 0x18 : uint16_t extended_rom_size

BOOST_AUTO_TEST_SUITE(BiosInformationEntryUnitTests)

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

/// @brief Tier 1: header.length < 0x12 - header-only, no formatted fields
/// at all, so every length-guarded getter must return its default (0).
std::vector<uint8_t> build_tier1_buffer()
{
    std::vector<uint8_t> buffer;
    buffer.push_back(SMBios::BIOSInformation); // type = 0
    buffer.push_back(0x04);                    // length = 4 (header only)
    buffer.push_back(0x01);                    // handle low byte
    buffer.push_back(0x00);                    // handle high byte

    append_string_terminator(buffer);
    return buffer;
}

/// @brief Tier 2: header.length == 0x18 - every field through
/// firmware_minor_release (0x17) decodes; extended_rom_size (0x18) does not
/// (0x18 + sizeof(uint16_t) > 0x18).
std::vector<uint8_t> build_tier2_buffer()
{
    std::vector<uint8_t> buffer;
    buffer.push_back(SMBios::BIOSInformation); // 0x00 type = 0
    buffer.push_back(0x18);                    // 0x01 length = 24
    buffer.push_back(0x01);                    // 0x02 handle low byte
    buffer.push_back(0x00);                    // 0x03 handle high byte

    buffer.push_back(0x02);                    // 0x04 vendor_index
    buffer.push_back(0x03);                    // 0x05 version_index

    buffer.push_back(0x00);                    // 0x06 starting_segment low
    buffer.push_back(0xF0);                    // 0x07 starting_segment high (0xF000)

    buffer.push_back(0x04);                    // 0x08 release_date_index
    buffer.push_back(0x01);                    // 0x09 rom_size (not 0xFF)

    // 0x0A..0x11 properties (u64 LE) -> 0xFEDCBA9876543210
    buffer.push_back(0x10);
    buffer.push_back(0x32);
    buffer.push_back(0x54);
    buffer.push_back(0x76);
    buffer.push_back(0x98);
    buffer.push_back(0xBA);
    buffer.push_back(0xDC);
    buffer.push_back(0xFE);

    buffer.push_back(0x05);                    // 0x12 properties_extension1
    buffer.push_back(0x06);                    // 0x13 properties_extension2
    buffer.push_back(0x07);                    // 0x14 bios_major_release
    buffer.push_back(0x08);                    // 0x15 bios_minor_release
    buffer.push_back(0x09);                    // 0x16 firmware_major_release
    buffer.push_back(0x0A);                    // 0x17 firmware_minor_release

    BOOST_REQUIRE_EQUAL(buffer.size(), static_cast<size_t>(0x18));

    append_string_terminator(buffer);
    return buffer;
}

/// @brief Tier 3: header.length == 0x1A (> 0x18) - extended_rom_size (0x18)
/// also decodes. rom_size's raw byte is 0xFF here to exercise the
/// "see extended ROM size instead" sentinel path alongside it.
std::vector<uint8_t> build_tier3_buffer()
{
    std::vector<uint8_t> buffer;
    buffer.push_back(SMBios::BIOSInformation); // 0x00 type = 0
    buffer.push_back(0x1A);                    // 0x01 length = 26
    buffer.push_back(0x02);                    // 0x02 handle low byte
    buffer.push_back(0x00);                    // 0x03 handle high byte

    buffer.push_back(0x0B);                    // 0x04 vendor_index
    buffer.push_back(0x0C);                    // 0x05 version_index

    buffer.push_back(0x00);                    // 0x06 starting_segment low
    buffer.push_back(0xE0);                    // 0x07 starting_segment high (0xE000)

    buffer.push_back(0x0D);                    // 0x08 release_date_index
    buffer.push_back(0xFF);                    // 0x09 rom_size sentinel

    // 0x0A..0x11 properties (u64 LE) -> 0xEFCDAB8967452301
    buffer.push_back(0x01);
    buffer.push_back(0x23);
    buffer.push_back(0x45);
    buffer.push_back(0x67);
    buffer.push_back(0x89);
    buffer.push_back(0xAB);
    buffer.push_back(0xCD);
    buffer.push_back(0xEF);

    buffer.push_back(0x0E);                    // 0x12 properties_extension1
    buffer.push_back(0x0F);                    // 0x13 properties_extension2
    buffer.push_back(0x10);                    // 0x14 bios_major_release
    buffer.push_back(0x11);                    // 0x15 bios_minor_release
    buffer.push_back(0x12);                    // 0x16 firmware_major_release
    buffer.push_back(0x13);                    // 0x17 firmware_minor_release

    buffer.push_back(0x32);                    // 0x18 extended_rom_size low
    buffer.push_back(0x00);                    // 0x19 extended_rom_size high (50)

    BOOST_REQUIRE_EQUAL(buffer.size(), static_cast<size_t>(0x1A));

    append_string_terminator(buffer);
    return buffer;
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x12 means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(BiosInformationEntryTruncatedBelow0x12DefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_tier1_buffer();
    DMIHeader header = make_header(buffer);

    BiosInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_vendor_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_runtime_size(), static_cast<uint32_t>(0));
    BOOST_CHECK_EQUAL(entry.get_release_date_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_rom_size(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_properties(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_properties_extension1(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_properties_extension2(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bios_major_release(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_bios_minor_release(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_firmware_major_release(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_firmware_minor_release(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_extended_rom_size(), static_cast<uint16_t>(0));
}

/// Normal case: header.length == 0x18 - every field through
/// firmware_minor_release decodes to its known byte value; extended ROM
/// size (which needs length >= 0x1A) still defaults to 0.
BOOST_AUTO_TEST_CASE(BiosInformationEntryMidTierDecodesThroughFirmwareReleaseTestCase)
{
    std::vector<uint8_t> buffer = build_tier2_buffer();
    DMIHeader header = make_header(buffer);

    BiosInformationEntry entry(header, SMBiosVersion{2, 4}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_vendor_index(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0x03));

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint16_t>(0xF000));
    // runtime size = (0x10000 - 0xF000) << 4 = 0x1000 << 4 = 65536 bytes
    BOOST_CHECK_EQUAL(entry.get_runtime_size(), static_cast<uint32_t>(65536));

    BOOST_CHECK_EQUAL(entry.get_release_date_index(), static_cast<uint8_t>(0x04));

    // raw rom_size byte is 0x01 (not the 0xFF sentinel): (0x01 + 1) << 6 = 128
    // (get_rom_size() returns uint8_t, so this value must stay < 256 to be
    // distinguishable from the 0xFF-sentinel-produced 0 - see the
    // 0x3F-would-silently-wrap-to-0 pitfall this test deliberately avoids)
    BOOST_CHECK_EQUAL(entry.get_rom_size(), static_cast<uint8_t>(128));

    BOOST_CHECK_EQUAL(entry.get_properties(), static_cast<uint64_t>(0xFEDCBA9876543210ULL));

    BOOST_CHECK_EQUAL(entry.get_properties_extension1(), static_cast<uint8_t>(0x05));
    BOOST_CHECK_EQUAL(entry.get_properties_extension2(), static_cast<uint8_t>(0x06));
    BOOST_CHECK_EQUAL(entry.get_bios_major_release(), static_cast<uint8_t>(0x07));
    BOOST_CHECK_EQUAL(entry.get_bios_minor_release(), static_cast<uint8_t>(0x08));
    BOOST_CHECK_EQUAL(entry.get_firmware_major_release(), static_cast<uint8_t>(0x09));
    BOOST_CHECK_EQUAL(entry.get_firmware_minor_release(), static_cast<uint8_t>(0x0A));

    // length == 0x18, extended_rom_size needs length >= 0x1A -> defaults
    BOOST_CHECK_EQUAL(entry.get_extended_rom_size(), static_cast<uint16_t>(0));
}

/// Edge case: header.length == 0x1A (> 0x18) - extended_rom_size now
/// decodes too, and the 0xFF rom_size sentinel still yields get_rom_size()
/// == 0 (the deleted-bytes behavior this task must preserve, per R8).
BOOST_AUTO_TEST_CASE(BiosInformationEntryAboveTierDecodesExtendedRomSizeTestCase)
{
    std::vector<uint8_t> buffer = build_tier3_buffer();
    DMIHeader header = make_header(buffer);

    BiosInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_vendor_index(), static_cast<uint8_t>(0x0B));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0x0C));

    BOOST_CHECK_EQUAL(entry.get_starting_address(), static_cast<uint16_t>(0xE000));
    // runtime size = (0x10000 - 0xE000) << 4 = 0x2000 << 4 = 131072 bytes
    BOOST_CHECK_EQUAL(entry.get_runtime_size(), static_cast<uint32_t>(131072));

    BOOST_CHECK_EQUAL(entry.get_release_date_index(), static_cast<uint8_t>(0x0D));

    // raw rom_size byte is the 0xFF sentinel -> get_rom_size() returns 0
    BOOST_CHECK_EQUAL(entry.get_rom_size(), static_cast<uint8_t>(0));

    BOOST_CHECK_EQUAL(entry.get_properties(), static_cast<uint64_t>(0xEFCDAB8967452301ULL));

    BOOST_CHECK_EQUAL(entry.get_properties_extension1(), static_cast<uint8_t>(0x0E));
    BOOST_CHECK_EQUAL(entry.get_properties_extension2(), static_cast<uint8_t>(0x0F));
    BOOST_CHECK_EQUAL(entry.get_bios_major_release(), static_cast<uint8_t>(0x10));
    BOOST_CHECK_EQUAL(entry.get_bios_minor_release(), static_cast<uint8_t>(0x11));
    BOOST_CHECK_EQUAL(entry.get_firmware_major_release(), static_cast<uint8_t>(0x12));
    BOOST_CHECK_EQUAL(entry.get_firmware_minor_release(), static_cast<uint8_t>(0x13));

    // extended ROM size: raw bytes {0x32, 0x00} LE -> 0x0032 == 50 (MB)
    BOOST_CHECK_EQUAL(entry.get_extended_rom_size(), static_cast<uint16_t>(50));
}

BOOST_AUTO_TEST_SUITE_END()
