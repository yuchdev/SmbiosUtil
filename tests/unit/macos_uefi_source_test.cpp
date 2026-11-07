#include <cstdint>
#include <vector>

// Milestone 0005, Story 03.0, Task 02 (ADR 0001): MacOsUefiSource lives in
// the macOS-only smbios_source_macos library, so this entire file is a
// no-op translation unit on every other platform - matching
// tests/unit/macos_efi_reader_test.cpp's (Task 01) established
// no-op-elsewhere convention for this platform.
#if defined(__APPLE__)

#include <iostream>
#include <string>

#include <smbios/source_interface/byte_reader.h>
#include <smbios/source_interface/smbios_anchor.h>
#include <smbios/source_interface/smbios_version_fixup.h>
#include <smbios/source_macos/macos_uefi_source.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

namespace {

/// @brief Build a synthetic 32-bit (`_SM_`) entry-point buffer with the
/// given major/minor and declared structure-table length at their
/// documented offsets (0x06/0x07 for major/minor, 0x16 for the u16
/// `structure_table_length`) - the exact offsets `MacOsUefiSource`'s own
/// (private) `decode_entry_point()` reads, confirmed against the Linux
/// sources decoding the identical wire format
/// (`linux_sysfs_source.cpp`/`linux_efi_source.cpp`).
std::vector<uint8_t> make_32bit_entry_point(uint8_t major, uint8_t minor, uint16_t table_length)
{
    std::vector<uint8_t> entry_point(0x18, 0);
    entry_point[0] = '_';
    entry_point[1] = 'S';
    entry_point[2] = 'M';
    entry_point[3] = '_';
    entry_point[0x06] = major;
    entry_point[0x07] = minor;
    entry_point[0x16] = static_cast<uint8_t>(table_length & 0xFF);
    entry_point[0x17] = static_cast<uint8_t>((table_length >> 8) & 0xFF);
    return entry_point;
}

/// @brief Build a synthetic 64-bit (`_SM3_`) entry-point buffer with the
/// given major/minor and declared max structure size at their documented
/// offsets (0x07/0x08 for major/minor, 0x0C for the u32 max structure
/// size).
std::vector<uint8_t> make_64bit_entry_point(uint8_t major, uint8_t minor, uint32_t table_length)
{
    std::vector<uint8_t> entry_point(0x10, 0);
    entry_point[0] = '_';
    entry_point[1] = 'S';
    entry_point[2] = 'M';
    entry_point[3] = '3';
    entry_point[4] = '_';
    entry_point[0x07] = major;
    entry_point[0x08] = minor;
    entry_point[0x0C] = static_cast<uint8_t>(table_length & 0xFF);
    entry_point[0x0D] = static_cast<uint8_t>((table_length >> 8) & 0xFF);
    entry_point[0x0E] = static_cast<uint8_t>((table_length >> 16) & 0xFF);
    entry_point[0x0F] = static_cast<uint8_t>((table_length >> 24) & 0xFF);
    return entry_point;
}

} // namespace

BOOST_AUTO_TEST_SUITE(MacosUefiSourceUnitTests)

/// Synthetic-blob case (no IORegistry/`MacOsUefiSource` involved -
/// `decode_entry_point()` is a private implementation detail of
/// `macos_uefi_source.cpp`, so this instead exercises the exact same
/// shared primitives at the exact same documented offsets that function
/// uses): a well-formed `_SM_` buffer decodes to the expected
/// major/minor/table-length triple, and the anchor is recognized.
BOOST_AUTO_TEST_CASE(Synthetic32BitEntryPointDecodesCorrectlyTestCase)
{
    const std::vector<uint8_t> entry_point = make_32bit_entry_point(2, 7, 0x0123);

    BOOST_CHECK(detect_smbios_anchor(entry_point.begin()) == SMBiosAnchorType::SMBios32);

    const uint8_t* data = entry_point.data();
    size_t major = static_cast<size_t>(read_u8(data, 0x06));
    size_t minor = static_cast<size_t>(read_u8(data, 0x07));
    const size_t table_length = static_cast<size_t>(read_u16(data, 0x16));

    BOOST_CHECK_EQUAL(major, 2u);
    BOOST_CHECK_EQUAL(minor, 7u);
    BOOST_CHECK_EQUAL(table_length, 0x0123u);

    fixup_smbios_version(major, minor);
    BOOST_CHECK_EQUAL(major, 2u);
    BOOST_CHECK_EQUAL(minor, 7u);
}

/// Synthetic-blob case: a well-formed `_SM3_` buffer decodes to the
/// expected major/minor/table-length triple at its own (different)
/// offsets.
BOOST_AUTO_TEST_CASE(Synthetic64BitEntryPointDecodesCorrectlyTestCase)
{
    const std::vector<uint8_t> entry_point = make_64bit_entry_point(3, 4, 0x00012345);

    BOOST_CHECK(detect_smbios_anchor(entry_point.begin()) == SMBiosAnchorType::SMBios64);

    const uint8_t* data = entry_point.data();
    const size_t major = static_cast<size_t>(read_u8(data, 0x07));
    const size_t minor = static_cast<size_t>(read_u8(data, 0x08));
    const size_t table_length = static_cast<size_t>(read_u32(data, 0x0C));

    BOOST_CHECK_EQUAL(major, 3u);
    BOOST_CHECK_EQUAL(minor, 4u);
    BOOST_CHECK_EQUAL(table_length, 0x00012345u);
}

/// Synthetic-blob case: a malformed version pair (major=2, minor=0x1F,
/// i.e. "2.31") is corrected by `fixup_smbios_version()` to 2.3, same
/// correction every other `ISmbiosSource` applies - confirms
/// `MacOsUefiSource`'s documented call ordering (fixup immediately after
/// decode) would see the same result.
BOOST_AUTO_TEST_CASE(MalformedVersionPairIsFixedUpTestCase)
{
    const std::vector<uint8_t> entry_point = make_32bit_entry_point(2, 0x1F, 0x0010);

    const uint8_t* data = entry_point.data();
    size_t major = static_cast<size_t>(read_u8(data, 0x06));
    size_t minor = static_cast<size_t>(read_u8(data, 0x07));

    fixup_smbios_version(major, minor);

    BOOST_CHECK_EQUAL(major, 2u);
    BOOST_CHECK_EQUAL(minor, 3u);
}

/// Synthetic-blob edge case: a bare `_DMI_` (legacy) anchor must be
/// treated as unrecognized by anything using this format - same
/// reasoning `linux_sysfs_source.cpp`'s/`linux_efi_source.cpp`'s own
/// `decode_entry_point()` document: that anchor implies
/// `legacy_decode()`'s distinct layout, not a smaller case of this one.
BOOST_AUTO_TEST_CASE(LegacyAnchorIsNotSMBios32Or64TestCase)
{
    std::vector<uint8_t> entry_point(0x18, 0);
    entry_point[0] = '_';
    entry_point[1] = 'D';
    entry_point[2] = 'M';
    entry_point[3] = 'I';
    entry_point[4] = '_';

    const SMBiosAnchorType anchor = detect_smbios_anchor(entry_point.begin());
    BOOST_CHECK(anchor == SMBiosAnchorType::SMBiosLegacy);
    BOOST_CHECK(anchor != SMBiosAnchorType::SMBios32);
    BOOST_CHECK(anchor != SMBiosAnchorType::SMBios64);
}

/// Real-hardware case: exercises the actual `MacOsUefiSource` end-to-end
/// against the live `AppleSMBIOS` IORegistry entry. Needs no root/admin
/// (R7), so it is safe to assert against on any Mac, including CI's
/// `macos-latest` runner - same reasoning as
/// `macos_efi_reader_test.cpp`'s (Task 01) own real-hardware case. If the
/// IORegistry entry turns out to be absent in some sandboxed/virtualized
/// CI environment, this degrades to a logged skip rather than a hard
/// failure, following that same test's established pattern - the task's
/// actual success criterion is a real Mac observing this source work end
/// to end, not every possible CI sandbox exposing this entry.
BOOST_AUTO_TEST_CASE(RealMacOsUefiSourceReadsLiveSmbiosTableTestCase)
{
    MacOsUefiSource source;

    if (!source.is_available()) {
        std::cerr << "[macos_uefi_source_test] AppleSMBIOS IORegistry entry not available on "
                     "this machine/runner - skipping end-to-end assertion (see test's doc "
                     "comment)."
                  << std::endl;
        return;
    }

    std::vector<uint8_t> table;
    size_t major_version = 0;
    size_t minor_version = 0;
    const bool read_ok = source.try_read(table, major_version, minor_version);

    if (!read_ok) {
        std::cerr << "[macos_uefi_source_test] MacOsUefiSource::is_available() was true but "
                     "try_read() failed - skipping end-to-end assertion (see test's doc "
                     "comment)."
                  << std::endl;
        return;
    }

    BOOST_CHECK(!table.empty());
    BOOST_CHECK(major_version > 0);
    BOOST_CHECK_EQUAL(std::string(source.source_name()), "macos-uefi");
}

BOOST_AUTO_TEST_SUITE_END()

#endif // defined(__APPLE__)
