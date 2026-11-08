#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/tables/processor_information_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive ProcessorInformationEntry (smbios/include/smbios/
// processor_information_entry.h) across the header.length tiers named by
// docs/roadmap/0004-.../02.0-.../02-processor-and-cache-information.md,
// cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 4
// ("Processor Information"):
//   tier 1: header.length < 0x1A   -> every length-guarded getter defaults
//   tier 2: header.length == 0x1A  -> socket_designation..upgrade decode;
//                                      cache handles still default (absent)
//   tier 3: header.length == 0x20  -> L1/L2/L3 cache handle decode;
//                                      serial/asset/part still default
//   tier 4: header.length == 0x23  -> serial/asset/part decode;
//                                      core_count..characteristics still default
//   tier 5: header.length == 0x28  -> core_count/core_enabled/thread_count/
//                                      characteristics decode (no sentinel);
//                                      family2/widened core fields default
//   plus dedicated cases for all four 0xFE/0xFF sentinel-widening
//   relationships (processor_family, core_count, core_enabled,
//   thread_count), proven both without and with the sentinel set, and the
//   voltage encoding's two branches.
//
// Wire layout (see smbios/include/smbios/processor_information_entry.h's
// "@brief 0xNN offset" getter comments):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  socket_designation_index
//   offset 0x05 : uint8_t  processor_type
//   offset 0x06 : uint8_t  processor_family (0xFE sentinel -> widened word at 0x28)
//   offset 0x07 : uint8_t  manufacturer_index
//   offset 0x08 : uint64_t processor_id (8 raw bytes)
//   offset 0x10 : uint8_t  version_index
//   offset 0x11 : uint8_t  voltage
//   offset 0x12 : uint16_t external_clock
//   offset 0x14 : uint16_t max_speed
//   offset 0x16 : uint16_t current_speed
//   offset 0x18 : uint8_t  status
//   offset 0x19 : uint8_t  processor_upgrade
//   offset 0x1A : uint16_t l1_cache_handle
//   offset 0x1C : uint16_t l2_cache_handle
//   offset 0x1E : uint16_t l3_cache_handle
//   offset 0x20 : uint8_t  serial_number_index
//   offset 0x21 : uint8_t  asset_tag_index
//   offset 0x22 : uint8_t  part_number_index
//   offset 0x23 : uint8_t  core_count (0xFF sentinel -> widened word at 0x2A)
//   offset 0x24 : uint8_t  core_enabled (0xFF sentinel -> widened word at 0x2C)
//   offset 0x25 : uint8_t  thread_count (0xFF sentinel -> widened word at 0x2E)
//   offset 0x26 : uint16_t characteristics
//   offset 0x28 : uint16_t processor_family_2 (widened)
//   offset 0x2A : uint16_t core_count_2 (widened)
//   offset 0x2C : uint16_t core_enabled_2 (widened)
//   offset 0x2E : uint16_t thread_count_2 (widened)
//   offset 0x30 : uint16_t thread_enabled

BOOST_AUTO_TEST_SUITE(ProcessorInformationEntryUnitTests)

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

/// @brief Build a full-length (0x32) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` (via
/// SyntheticTableBuilder::add_structure(), which computes header.length
/// automatically from the formatted area's size). The four sentinel-governed
/// bytes (processor_family at 0x06, core_count at 0x23, core_enabled at
/// 0x24, thread_count at 0x25) and voltage/status are overridable so each
/// test case can drive the exact branch it needs; `length` must be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length,
                                   uint8_t processor_family_byte = 0x0B, // Pentium
                                   uint8_t core_count_byte = 0x05,
                                   uint8_t core_enabled_byte = 0x04,
                                   uint8_t thread_count_byte = 0x08,
                                   uint8_t voltage_byte = 0x00,
                                   uint8_t status_byte = 0x41) // populated, Enabled
{
    std::vector<uint8_t> full_formatted_area{
        0x01,                                         // 0x04 socket_designation_index
        ProcessorInformationEntry::CentralProcessor,  // 0x05 processor_type
        processor_family_byte,                        // 0x06 processor_family
        0x02,                                         // 0x07 manufacturer_index

        // 0x08-0x0F processor_id (8 bytes, LE) -> recognizable pattern
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,

        0x03,          // 0x10 version_index
        voltage_byte,  // 0x11 voltage

        0x64, 0x00, // 0x12 external_clock = 100
        0xB8, 0x0B, // 0x14 max_speed = 3000
        0xC4, 0x09, // 0x16 current_speed = 2500

        status_byte, // 0x18 status
        0x31,        // 0x19 processor_upgrade = Socket AM4

        0x01, 0x00, // 0x1A l1_cache_handle = 0x0001
        0x02, 0x00, // 0x1C l2_cache_handle = 0x0002
        0xFF, 0xFF, // 0x1E l3_cache_handle = 0xFFFF (absent)

        0x04, // 0x20 serial_number_index
        0x05, // 0x21 asset_tag_index
        0x06, // 0x22 part_number_index

        core_count_byte,   // 0x23 core_count
        core_enabled_byte, // 0x24 core_enabled
        thread_count_byte, // 0x25 thread_count

        0x0C, 0x00, // 0x26 characteristics = Capable64Bit | MultiCore

        0x00, 0x01, // 0x28 processor_family_2 = 0x0100 (ARMv7)
        0x30, 0x00, // 0x2A core_count_2 = 48
        0x28, 0x00, // 0x2C core_enabled_2 = 40
        0x60, 0x00, // 0x2E thread_count_2 = 96
        0x50, 0x00, // 0x30 thread_enabled = 80
    };

    BOOST_REQUIRE_EQUAL(full_formatted_area.size(), static_cast<size_t>(0x32 - 4));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), static_cast<size_t>(0x32));
    BOOST_REQUIRE_GE(static_cast<size_t>(length), static_cast<size_t>(4));

    std::vector<uint8_t> formatted_area(full_formatted_area.begin(),
                                         full_formatted_area.begin() + (length - 4));

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::ProcessorInformation, formatted_area);
    return builder.build();
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x1A means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(ProcessorInformationEntryTruncatedBelow0x1ADefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_socket_designation_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_processor_type(), static_cast<uint8_t>(ProcessorInformationEntry::ProcessorTypeOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_processor_family(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_processor_id(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_voltage(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_external_clock(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_max_speed(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_current_speed(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_populated(), false);
    BOOST_CHECK_EQUAL(entry.get_processor_upgrade(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_l1_cache_handle(), static_cast<uint16_t>(0xFFFF));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_core_count(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_characteristics(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_thread_enabled(), static_cast<uint16_t>(0));
}

/// Normal case: header.length == 0x1A - every base field through
/// processor_upgrade decodes; cache handles (need length >= 0x20) default.
BOOST_AUTO_TEST_CASE(ProcessorInformationEntryTier0x1ADecodesBaseFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_socket_designation_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_processor_type(), static_cast<uint8_t>(ProcessorInformationEntry::CentralProcessor));
    BOOST_CHECK_EQUAL(entry.get_processor_family(), static_cast<uint16_t>(0x0B));
    BOOST_CHECK_EQUAL(entry.get_manufacturer_index(), static_cast<uint8_t>(0x02));
    BOOST_CHECK_EQUAL(entry.get_processor_id(), static_cast<uint64_t>(0x1122334455667788ULL));
    BOOST_CHECK_EQUAL(entry.get_version_index(), static_cast<uint8_t>(0x03));
    BOOST_CHECK_EQUAL(entry.get_external_clock(), static_cast<uint16_t>(100));
    BOOST_CHECK_EQUAL(entry.get_max_speed(), static_cast<uint16_t>(3000));
    BOOST_CHECK_EQUAL(entry.get_current_speed(), static_cast<uint16_t>(2500));
    BOOST_CHECK_EQUAL(entry.get_populated(), true);
    BOOST_CHECK_EQUAL(entry.get_status_code(), static_cast<uint8_t>(ProcessorInformationEntry::StatusEnabled));
    BOOST_CHECK_EQUAL(entry.get_processor_upgrade(), static_cast<uint8_t>(0x31));

    // length == 0x1A, cache handles need length >= 0x20 -> default
    BOOST_CHECK_EQUAL(entry.get_l1_cache_handle(), static_cast<uint16_t>(0xFFFF));
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0));
}

/// Edge case: header.length == 0x20 - L1/L2/L3 cache handle decode; the
/// serial/asset/part string indices (need length >= 0x23) still default.
BOOST_AUTO_TEST_CASE(ProcessorInformationEntryTier0x20DecodesCacheHandlesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x20);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_l1_cache_handle(), static_cast<uint16_t>(0x0001));
    BOOST_CHECK_EQUAL(entry.get_l2_cache_handle(), static_cast<uint16_t>(0x0002));
    BOOST_CHECK_EQUAL(entry.get_l3_cache_handle(), static_cast<uint16_t>(0xFFFF));

    // length == 0x20, serial/asset/part need length >= 0x23 -> default
    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_core_count(), static_cast<uint16_t>(0));
}

/// Normal case: header.length == 0x23 - serial/asset/part string indices
/// decode; core_count/core_enabled/thread_count/characteristics (need
/// length >= 0x28) still default.
BOOST_AUTO_TEST_CASE(ProcessorInformationEntryTier0x23DecodesStringIndicesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x23);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_serial_number_index(), static_cast<uint8_t>(0x04));
    BOOST_CHECK_EQUAL(entry.get_asset_tag_index(), static_cast<uint8_t>(0x05));
    BOOST_CHECK_EQUAL(entry.get_part_number_index(), static_cast<uint8_t>(0x06));

    // length == 0x23, core_count etc need length >= 0x28 -> default
    BOOST_CHECK_EQUAL(entry.get_core_count(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_characteristics(), static_cast<uint16_t>(0));
}

/// Normal case: header.length == 0x28 - core_count/core_enabled/
/// thread_count/characteristics decode, none of the three bytes is the
/// 0xFF sentinel here, so each getter returns the plain byte value.
BOOST_AUTO_TEST_CASE(ProcessorInformationEntryTier0x28DecodesCoreFieldsWithoutSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x28, 0x0B, 0x05, 0x04, 0x08);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 5}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_core_count(), static_cast<uint16_t>(5));
    BOOST_CHECK_EQUAL(entry.get_core_enabled(), static_cast<uint16_t>(4));
    BOOST_CHECK_EQUAL(entry.get_thread_count(), static_cast<uint16_t>(8));
    BOOST_CHECK_EQUAL(entry.get_characteristics(), static_cast<uint16_t>(0x000C));

    // length == 0x28, widened fields need length >= 0x2A/0x2C/0x2E -> n/a here
    BOOST_CHECK_EQUAL(entry.get_thread_enabled(), static_cast<uint16_t>(0));
}

/// Milestone 0004 Story 02.0's own success criterion, case 1 of 4:
/// processor_family WITHOUT the 0xFE sentinel uses the plain byte value,
/// even when the buffer is long enough for the widened field.
BOOST_AUTO_TEST_CASE(ProcessorFamilyUsesPlainByteWhenNotSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2A, 0x0B /* Pentium, not 0xFE */);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_processor_family(), static_cast<uint16_t>(0x0B));
    BOOST_CHECK_EQUAL(entry.get_processor_family_string(), std::string("Pentium"));
}

/// Case 2 of 4: processor_family WITH the 0xFE sentinel and a long-enough
/// buffer consults the widened word at 0x28 instead of the raw byte.
BOOST_AUTO_TEST_CASE(ProcessorFamilySentinelConsultsWidenedFieldTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2A, 0xFE);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    // widened field encodes 0x0100 == ARMv7
    BOOST_CHECK_EQUAL(entry.get_processor_family(), static_cast<uint16_t>(0x0100));
    BOOST_CHECK_EQUAL(entry.get_processor_family_string(), std::string("ARMv7"));
}

/// Regression/edge case: the 0xFE sentinel byte is present, but the buffer
/// is one byte too short for the widened field (length == 0x29, needs
/// 0x2A) - the raw 0xFE byte must be returned as-is, not read past the
/// formatted area.
BOOST_AUTO_TEST_CASE(ProcessorFamilySentinelFallsBackWhenLengthInsufficientTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x29, 0xFE);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 6}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_processor_family(), static_cast<uint16_t>(0xFE));
}

/// Core count sentinel, case without: plain byte value used even when the
/// buffer is long enough for the widened field.
BOOST_AUTO_TEST_CASE(CoreCountUsesPlainByteWhenNotSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2C, 0x0B, 0x05 /* not 0xFF */);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_core_count(), static_cast<uint16_t>(5));
}

/// Core count sentinel, case with: 0xFF at the legacy byte plus a
/// long-enough buffer (length >= 0x2C) consults the widened word at 0x2A.
BOOST_AUTO_TEST_CASE(CoreCountSentinelConsultsWidenedFieldTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2C, 0x0B, 0xFF);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    // widened field encodes 48
    BOOST_CHECK_EQUAL(entry.get_core_count(), static_cast<uint16_t>(48));
}

/// Regression/edge case: 0xFF sentinel present but buffer one byte short of
/// the widened field's own threshold (length == 0x2B, needs 0x2C) - the raw
/// 0xFF byte (255) must be returned, not read past the formatted area.
BOOST_AUTO_TEST_CASE(CoreCountSentinelFallsBackWhenLengthInsufficientTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2B, 0x0B, 0xFF);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_core_count(), static_cast<uint16_t>(0xFF));
}

/// Core enabled sentinel, case without.
BOOST_AUTO_TEST_CASE(CoreEnabledUsesPlainByteWhenNotSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2E, 0x0B, 0x05, 0x04 /* not 0xFF */);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_core_enabled(), static_cast<uint16_t>(4));
}

/// Core enabled sentinel, case with: 0xFF at the legacy byte plus a
/// long-enough buffer (length >= 0x2E) consults the widened word at 0x2C.
BOOST_AUTO_TEST_CASE(CoreEnabledSentinelConsultsWidenedFieldTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2E, 0x0B, 0x05, 0xFF);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    // widened field encodes 40
    BOOST_CHECK_EQUAL(entry.get_core_enabled(), static_cast<uint16_t>(40));
}

/// Regression/edge case: 0xFF sentinel present but buffer one byte short of
/// the widened field's own threshold (length == 0x2D, needs 0x2E).
BOOST_AUTO_TEST_CASE(CoreEnabledSentinelFallsBackWhenLengthInsufficientTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2D, 0x0B, 0x05, 0xFF);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_core_enabled(), static_cast<uint16_t>(0xFF));
}

/// Thread count sentinel, case without.
BOOST_AUTO_TEST_CASE(ThreadCountUsesPlainByteWhenNotSentinelTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x30, 0x0B, 0x05, 0x04, 0x08 /* not 0xFF */);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_thread_count(), static_cast<uint16_t>(8));
}

/// Thread count sentinel, case with: 0xFF at the legacy byte plus a
/// long-enough buffer (length >= 0x30) consults the widened word at 0x2E.
BOOST_AUTO_TEST_CASE(ThreadCountSentinelConsultsWidenedFieldTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x30, 0x0B, 0x05, 0x04, 0xFF);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    // widened field encodes 96
    BOOST_CHECK_EQUAL(entry.get_thread_count(), static_cast<uint16_t>(96));
}

/// Regression/edge case: 0xFF sentinel present but buffer one byte short of
/// the widened field's own threshold (length == 0x2F, needs 0x30).
BOOST_AUTO_TEST_CASE(ThreadCountSentinelFallsBackWhenLengthInsufficientTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x2F, 0x0B, 0x05, 0x04, 0xFF);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_thread_count(), static_cast<uint16_t>(0xFF));
}

/// Normal case: header.length == 0x32 (full buffer) - thread_enabled, the
/// last field, also decodes.
BOOST_AUTO_TEST_CASE(ProcessorInformationEntryFullBufferDecodesThreadEnabledTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x32);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_thread_enabled(), static_cast<uint16_t>(80));
}

/// Voltage encoding, branch 1: bit 7 set means an explicit voltage in bits
/// 0-6, tenths of a volt. code = 0x8D -> (0x0D == 13) -> "1.3 V".
BOOST_AUTO_TEST_CASE(VoltageExplicitBit7DecodesTenthsOfAVoltTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A, 0x0B, 0x05, 0x04, 0x08, 0x8D);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_voltage(), static_cast<uint8_t>(0x8D));
    BOOST_CHECK_EQUAL(entry.get_voltage_string(), std::string("1.3 V"));
}

/// Voltage encoding, branch 2: bit 7 clear means bits 0-2 are a bitmask of
/// legacy supported voltages; here bits 0 and 1 are both set (5.0 V and
/// 3.3 V both supported).
BOOST_AUTO_TEST_CASE(VoltageLegacyBitmaskDecodesMultipleSupportedVoltagesTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A, 0x0B, 0x05, 0x04, 0x08, 0x03);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_voltage_string(), std::string("5.0 V 3.3 V"));
}

/// Voltage encoding, edge case: bit 7 clear and bits 0-2 all clear too ->
/// "Unknown".
BOOST_AUTO_TEST_CASE(VoltageUnknownWhenNoBitsSetTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A, 0x0B, 0x05, 0x04, 0x08, 0x00);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_voltage_string(), std::string("Unknown"));
}

/// Status decode, branch 1: bit 6 set (populated) - bits 0-2 give a status
/// code, rendered as "Populated, <status>".
BOOST_AUTO_TEST_CASE(StatusPopulatedRendersStatusCodeTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A, 0x0B, 0x05, 0x04, 0x08, 0x00, 0x41 /* populated, Enabled */);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_populated(), true);
    BOOST_CHECK_EQUAL(entry.get_status_code(), static_cast<uint8_t>(ProcessorInformationEntry::StatusEnabled));
    BOOST_CHECK_EQUAL(entry.get_status_string(), std::string("Populated, Enabled"));
}

/// Status decode, branch 2: bit 6 clear (unpopulated) - the status code
/// bits are not rendered, regardless of their value.
BOOST_AUTO_TEST_CASE(StatusUnpopulatedDoesNotRenderStatusCodeTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A, 0x0B, 0x05, 0x04, 0x08, 0x00, 0x02 /* unpopulated, code bits = 2 */);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_populated(), false);
    BOOST_CHECK_EQUAL(entry.get_status_string(), std::string("Unpopulated"));
}

/// Absent-cache-handle rendering is SMBIOS-version-dependent: 2.3+ renders
/// "Not Provided", pre-2.3 renders "No Cache" - see dmi_processor_cache()'s
/// own `ver >= 0x0203` check.
BOOST_AUTO_TEST_CASE(AbsentCacheHandleRendersNotProvidedFrom2Dot3TestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x20);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_l3_cache_handle_string(), std::string("Not Provided"));
}

BOOST_AUTO_TEST_CASE(AbsentCacheHandleRendersNoCacheBefore2Dot3TestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x20);
    DMIHeader header = make_header(buffer);

    ProcessorInformationEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_l3_cache_handle_string(), std::string("No Cache"));
}

/// Invalid-input case: constructing with a non-ProcessorInformation header
/// type must throw, not silently misparse.
BOOST_AUTO_TEST_CASE(WrongHeaderTypeThrowsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A);
    buffer[0] = SMBios::BIOSInformation; // deliberately wrong type
    DMIHeader header = make_header(buffer);

    BOOST_CHECK_THROW(ProcessorInformationEntry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0004 Story 02.0's own success criterion: ProcessorInformation
/// resolves through SMBiosEntryFactory::create() to a real
/// ProcessorInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsProcessorInformationEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x32);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<ProcessorInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
