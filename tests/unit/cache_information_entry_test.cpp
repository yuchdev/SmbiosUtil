#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/cache_information_entry.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: hand-built DMIHeader/raw
// buffers drive CacheInformationEntry (smbios/include/smbios/
// cache_information_entry.h) across the header.length tiers named by
// docs/roadmap/0004-.../02.0-.../02-processor-and-cache-information.md,
// cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 7
// ("Cache Information"):
//   tier 1: header.length < 0x0F   -> every length-guarded getter defaults
//   tier 2: header.length == 0x0F  -> socket_designation..installed_sram_type
//                                      decode (legacy size fields only);
//                                      speed/ec_type/system_type/
//                                      associativity still default
//   tier 3: header.length == 0x13  -> speed/ec_type/system_type/
//                                      associativity also decode
//   plus dedicated cases for the legacy-vs-widened size-field selection at
//   both length thresholds (0x17 for max size, 0x1B for installed size),
//   and both granularity branches (1 KB / 64 KB) of the size encoding.
//
// Wire layout (see smbios/include/smbios/cache_information_entry.h's
// "@brief 0xNN offset" getter comments):
//   offset 0x00 : uint8_t  type   (common DMIHeader, parsed separately)
//   offset 0x01 : uint8_t  length
//   offset 0x02 : uint16_t handle
//   offset 0x04 : uint8_t  socket_designation_index
//   offset 0x05 : uint16_t cache_configuration (enabled/socketed/level/mode/location)
//   offset 0x07 : uint16_t max_cache_size (legacy)
//   offset 0x09 : uint16_t installed_size (legacy)
//   offset 0x0B : uint16_t supported_sram_type
//   offset 0x0D : uint16_t installed_sram_type
//   offset 0x0F : uint8_t  cache_speed
//   offset 0x10 : uint8_t  error_correction_type
//   offset 0x11 : uint8_t  system_cache_type
//   offset 0x12 : uint8_t  associativity
//   offset 0x13 : uint32_t max_cache_size_2 (widened, supersedes 0x07 when length >= 0x17)
//   offset 0x17 : uint32_t installed_size_2 (widened, supersedes 0x09 when length >= 0x1B)

BOOST_AUTO_TEST_SUITE(CacheInformationEntryUnitTests)

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

void push_u16(std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void push_u32(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

/// @brief Build a full-length (0x1B) formatted-area buffer with distinct
/// known byte values at every offset, then truncate to `length` and patch
/// the header's own length byte accordingly. The legacy (0x07/0x09) and
/// widened (0x13/0x17) size fields are overridable so tests can drive the
/// legacy-vs-widened and granularity-branch cases precisely; `length` must
/// be >= 0x04.
std::vector<uint8_t> build_buffer(uint8_t length,
                                   uint16_t cache_configuration = 0x0082, // enabled, level 3, Internal
                                   uint16_t legacy_max_size = 0x0010,     // 16 KB, 1 KB granularity
                                   uint16_t legacy_installed_size = 0x0008, // 8 KB, 1 KB granularity
                                   uint32_t widened_max_size = 0x00000020,   // 32 KB, 1 KB granularity
                                   uint32_t widened_installed_size = 0x00000010) // 16 KB, 1 KB granularity
{
    std::vector<uint8_t> full;
    full.push_back(SMBios::CacheInformation); // 0x00 type = 7
    full.push_back(0x00);                     // 0x01 length (patched below)
    full.push_back(0x01);                     // 0x02 handle low byte
    full.push_back(0x00);                     // 0x03 handle high byte

    full.push_back(0x01);                      // 0x04 socket_designation_index
    push_u16(full, cache_configuration);       // 0x05 cache_configuration
    push_u16(full, legacy_max_size);           // 0x07 max_cache_size (legacy)
    push_u16(full, legacy_installed_size);     // 0x09 installed_size (legacy)
    push_u16(full, CacheInformationEntry::Burst | CacheInformationEntry::Synchronous);   // 0x0B supported_sram_type
    push_u16(full, CacheInformationEntry::Synchronous);                                  // 0x0D installed_sram_type

    full.push_back(0x0A); // 0x0F cache_speed = 10 ns
    full.push_back(CacheInformationEntry::SingleBitECC); // 0x10 error_correction_type
    full.push_back(CacheInformationEntry::Unified);      // 0x11 system_cache_type
    full.push_back(CacheInformationEntry::EightWaySetAssociative); // 0x12 associativity

    push_u32(full, widened_max_size);       // 0x13 max_cache_size_2 (widened)
    push_u32(full, widened_installed_size); // 0x17 installed_size_2 (widened)

    BOOST_REQUIRE_EQUAL(full.size(), static_cast<size_t>(0x1B));
    BOOST_REQUIRE_LE(static_cast<size_t>(length), full.size());

    std::vector<uint8_t> buffer(full.begin(), full.begin() + length);
    buffer[1] = length;

    append_string_terminator(buffer);
    return buffer;
}

} // namespace

/// Invalid-input/truncated case: header.length < 0x0F means no formatted
/// field is actually present, so every length-guarded getter must default.
BOOST_AUTO_TEST_CASE(CacheInformationEntryTruncatedBelow0x0FDefaultsAllFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x04);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_socket_designation_index(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_enabled(), false);
    BOOST_CHECK_EQUAL(entry.get_socketed(), false);
    BOOST_CHECK_EQUAL(entry.get_operational_mode(), static_cast<uint8_t>(CacheInformationEntry::OperationalModeUnknown));
    BOOST_CHECK_EQUAL(entry.get_location(), static_cast<uint8_t>(CacheInformationEntry::LocationUnknown));
    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_installed_cache_size(), static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(entry.get_supported_sram_type(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_installed_sram_type(), static_cast<uint16_t>(0));
    BOOST_CHECK_EQUAL(entry.get_cache_speed(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_error_correction_type(), static_cast<uint8_t>(CacheInformationEntry::ErrorCorrectionOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_system_cache_type(), static_cast<uint8_t>(CacheInformationEntry::SystemCacheTypeOutOfSpec));
    BOOST_CHECK_EQUAL(entry.get_associativity(), static_cast<uint8_t>(CacheInformationEntry::AssociativityOutOfSpec));
}

/// Normal case: header.length == 0x0F - socket designation, cache
/// configuration sub-fields, legacy size fields, and SRAM type bitmasks all
/// decode; speed/ec_type/system_type/associativity (need length >= 0x13)
/// still default.
BOOST_AUTO_TEST_CASE(CacheInformationEntryTier0x0FDecodesBaseFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_socket_designation_index(), static_cast<uint8_t>(0x01));
    BOOST_CHECK_EQUAL(entry.get_enabled(), true);
    BOOST_CHECK_EQUAL(entry.get_socketed(), false);
    BOOST_CHECK_EQUAL(entry.get_cache_level(), static_cast<uint8_t>(3)); // 0x0082 & 0x0007 == 2, +1 == 3
    BOOST_CHECK_EQUAL(entry.get_location(), static_cast<uint8_t>(CacheInformationEntry::Internal));

    // length == 0x0F (< 0x17), legacy size field is authoritative: 0x0010 ->
    // bit 15 clear -> 1 KB granularity -> 16 KB
    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(16));
    // legacy installed size: 0x0008 -> 8 KB
    BOOST_CHECK_EQUAL(entry.get_installed_cache_size(), static_cast<uint64_t>(8));

    BOOST_CHECK_EQUAL(entry.get_supported_sram_type(),
                       static_cast<uint16_t>(CacheInformationEntry::Burst | CacheInformationEntry::Synchronous));
    BOOST_CHECK_EQUAL(entry.get_installed_sram_type(), static_cast<uint16_t>(CacheInformationEntry::Synchronous));

    // length == 0x0F, speed/ec_type/system_type/associativity need length >= 0x13
    BOOST_CHECK_EQUAL(entry.get_cache_speed(), static_cast<uint8_t>(0));
    BOOST_CHECK_EQUAL(entry.get_error_correction_type(), static_cast<uint8_t>(CacheInformationEntry::ErrorCorrectionOutOfSpec));
}

/// Normal case: header.length == 0x13 - speed/ec_type/system_type/
/// associativity also decode.
BOOST_AUTO_TEST_CASE(CacheInformationEntryTier0x13DecodesSpeedAndTypeFieldsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_cache_speed(), static_cast<uint8_t>(10));
    BOOST_CHECK_EQUAL(entry.get_error_correction_type(), static_cast<uint8_t>(CacheInformationEntry::SingleBitECC));
    BOOST_CHECK_EQUAL(entry.get_system_cache_type(), static_cast<uint8_t>(CacheInformationEntry::Unified));
    BOOST_CHECK_EQUAL(entry.get_associativity(), static_cast<uint8_t>(CacheInformationEntry::EightWaySetAssociative));

    // length == 0x13 (< 0x17), the widened field must not be consulted yet
    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(16));
}

/// Cache configuration bitfield decode, dedicated case: enabled/socketed
/// both set, level and operational mode and location all distinct from the
/// other tests' values.
BOOST_AUTO_TEST_CASE(CacheConfigurationDecodesAllSubFieldsTestCase)
{
    // bit7 enabled=1, bit3 socketed=1, bits0-2 level code=5 (level 6),
    // bits8-9 mode=VariesWithMemoryAddress(2), bits5-6 location=External(1)
    const uint16_t configuration = 0x0080 | 0x0008 | 0x0005 | (0x02 << 8) | (0x01 << 5);
    std::vector<uint8_t> buffer = build_buffer(0x0F, configuration);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_enabled(), true);
    BOOST_CHECK_EQUAL(entry.get_socketed(), true);
    BOOST_CHECK_EQUAL(entry.get_cache_level(), static_cast<uint8_t>(6));
    BOOST_CHECK_EQUAL(entry.get_operational_mode(), static_cast<uint8_t>(CacheInformationEntry::VariesWithMemoryAddress));
    BOOST_CHECK_EQUAL(entry.get_location(), static_cast<uint8_t>(CacheInformationEntry::External));
    BOOST_CHECK_EQUAL(entry.get_operational_mode_string(), std::string("Varies With Memory Address"));
    BOOST_CHECK_EQUAL(entry.get_location_string(), std::string("External"));
}

/// Cache configuration bitfield decode, edge case: disabled/not-socketed.
BOOST_AUTO_TEST_CASE(CacheConfigurationDecodesDisabledNotSocketedTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F, 0x0000);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_enabled_string(), std::string("Disabled"));
    BOOST_CHECK_EQUAL(entry.get_socketed_string(), std::string("Not Socketed"));
    BOOST_CHECK_EQUAL(entry.get_cache_level(), static_cast<uint8_t>(1));
}

/// Milestone 0004 Story 02.0's own success criterion, max size, case 1 of
/// 2: header.length < 0x17 - the legacy uint16_t field at 0x07 is
/// authoritative, not the widened one, even though the widened field is
/// physically present in this buffer with a different value.
BOOST_AUTO_TEST_CASE(MaxCacheSizeUsesLegacyFieldBelowWidenedThresholdTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x16, 0x0082, 0x0010 /* legacy: 16 KB */,
                                                0x0008, 0x00000080 /* widened: 128 KB, if consulted */);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(16));
}

/// Max size, case 2 of 2: header.length >= 0x17 - the widened uint32_t
/// field at 0x13 supersedes the legacy one.
BOOST_AUTO_TEST_CASE(MaxCacheSizeUsesWidenedFieldAtThresholdTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x17, 0x0082, 0x0010 /* legacy: 16 KB, ignored */,
                                                0x0008, 0x00000080 /* widened: 128 KB */);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(128));
}

/// Installed size, case 1 of 2: header.length < 0x1B - the legacy uint16_t
/// field at 0x09 is authoritative, even with the widened field present and
/// differing.
BOOST_AUTO_TEST_CASE(InstalledCacheSizeUsesLegacyFieldBelowWidenedThresholdTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1A, 0x0082, 0x0010, 0x0008 /* legacy: 8 KB */,
                                                0x00000020, 0x00000040 /* widened: 64 KB, if consulted */);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_installed_cache_size(), static_cast<uint64_t>(8));
}

/// Installed size, case 2 of 2: header.length >= 0x1B (full buffer) - the
/// widened uint32_t field at 0x17 supersedes the legacy one.
BOOST_AUTO_TEST_CASE(InstalledCacheSizeUsesWidenedFieldAtThresholdTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1B, 0x0082, 0x0010, 0x0008 /* legacy: 8 KB, ignored */,
                                                0x00000020, 0x00000040 /* widened: 64 KB */);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_installed_cache_size(), static_cast<uint64_t>(64));
}

/// Granularity decode, legacy field, branch 1: bit 15 clear -> 1 KB
/// granularity, value is the raw count (already covered implicitly by the
/// other legacy-field tests, exercised explicitly here for clarity).
BOOST_AUTO_TEST_CASE(LegacySizeGranularity1KBTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F, 0x0082, 0x0019 /* 25, bit15 clear -> 25 KB */);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(25));
}

/// Granularity decode, legacy field, branch 2: bit 15 set -> 64 KB
/// granularity. code = 0x8002 -> count = 2 -> 2 * 64 = 128 KB.
BOOST_AUTO_TEST_CASE(LegacySizeGranularity64KBTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F, 0x0082, 0x8002);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(128));
}

/// Granularity decode, widened field, branch 1: bit 31 clear -> 1 KB
/// granularity, value is the raw count.
BOOST_AUTO_TEST_CASE(WidenedSizeGranularity1KBTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x17, 0x0082, 0x0010, 0x0008, 0x00000190 /* 400 KB */);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(400));
}

/// Granularity decode, widened field, branch 2: bit 31 set -> 64 KB
/// granularity. code = 0x80000005 -> count = 5 -> 5 * 64 = 320 KB.
BOOST_AUTO_TEST_CASE(WidenedSizeGranularity64KBTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x17, 0x0082, 0x0010, 0x0008, 0x80000005u);
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_max_cache_size(), static_cast<uint64_t>(320));
}

/// SRAM type bitmask, edge case: "None" rendering when the bitmask's
/// meaningful bits (0-6) are all clear.
BOOST_AUTO_TEST_CASE(SupportedSramTypeRendersNoneWhenEmptyTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F);
    // Overwrite supported_sram_type (0x0B) to 0 directly - build_buffer's
    // own parameters don't expose it, so patch the raw byte here.
    buffer[0x0B] = 0x00;
    buffer[0x0C] = 0x00;
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_supported_sram_type_string(), std::string("\tNone\n"));
}

/// Error correction / system type / associativity string decode, dedicated
/// case with different codes from the other tests.
BOOST_AUTO_TEST_CASE(ErrorCorrectionSystemTypeAssociativityStringsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x13);
    buffer[0x10] = CacheInformationEntry::Parity;
    buffer[0x11] = CacheInformationEntry::Instruction;
    buffer[0x12] = CacheInformationEntry::FullyAssociative;
    DMIHeader header = make_header(buffer);

    CacheInformationEntry entry(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_error_correction_type_string(), std::string("Parity"));
    BOOST_CHECK_EQUAL(entry.get_system_cache_type_string(), std::string("Instruction"));
    BOOST_CHECK_EQUAL(entry.get_associativity_string(), std::string("Fully Associative"));
}

/// Invalid-input case: constructing with a non-CacheInformation header type
/// must throw, not silently misparse.
BOOST_AUTO_TEST_CASE(WrongHeaderTypeThrowsTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x0F);
    buffer[0] = SMBios::BIOSInformation; // deliberately wrong type
    DMIHeader header = make_header(buffer);

    BOOST_CHECK_THROW(CacheInformationEntry(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0004 Story 02.0's own success criterion: CacheInformation
/// resolves through SMBiosEntryFactory::create() to a real
/// CacheInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsCacheInformationEntryTestCase)
{
    std::vector<uint8_t> buffer = build_buffer(0x1B);
    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<CacheInformationEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
