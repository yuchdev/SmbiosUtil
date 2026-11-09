#include <cstdint>
#include <string>
#include <vector>

// Milestone 0005, Story 02.0, Task 01 (ADR 0001): LinuxSysfsSource lives in
// the Linux-only smbios_linux library, so this entire file is a
// no-op translation unit on every other platform - matching how
// tests/unit/linux_efi_source_test.cpp (Task 02) guards its own coverage
// for the identical reason.
#if defined(__linux__)

#include <fstream>
#include <filesystem>

#include <sys/stat.h>
#include <unistd.h>

#include <smbios/linux/linux_sysfs_source.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

namespace {

/// @brief RAII temp-directory fixture mimicking
/// `/sys/firmware/dmi/tables/`'s two-file layout, so
/// `LinuxSysfsSource(entry_point_path, table_path)` can be pointed at it
/// instead of the real sysfs paths (see that constructor's doc comment in
/// `smbios/include/smbios/linux/linux_sysfs_source.h` for why it
/// exists).
class SysfsFixtureDir {
public:
    SysfsFixtureDir()
        : dir_(std::filesystem::temp_directory_path() /
               std::filesystem::path("smbios_sysfs_fixture_" + std::to_string(::getpid())))
    {
        std::filesystem::create_directories(dir_);
    }

    ~SysfsFixtureDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(dir_, ignored);
    }

    std::string entry_point_path() const { return (dir_ / "smbios_entry_point").string(); }
    std::string table_path() const { return (dir_ / "DMI").string(); }

private:
    std::filesystem::path dir_;
};

/// @brief Write `bytes` to `path`, truncating/creating as needed.
void write_file(const std::string& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

/// @brief Build a synthetic 32-bit (`_SM_`) entry-point buffer with the
/// given major/minor at their documented offsets (0x06/0x07) - matches
/// `LinuxSysfsSource`'s own decode offsets, confirmed against
/// `c-versions/dmidecode_linux/dmidecode.c`'s `smbios_decode()`.
std::vector<uint8_t> make_32bit_entry_point(uint8_t major, uint8_t minor)
{
    std::vector<uint8_t> entry_point(0x1E, 0);
    entry_point[0] = '_';
    entry_point[1] = 'S';
    entry_point[2] = 'M';
    entry_point[3] = '_';
    entry_point[0x06] = major;
    entry_point[0x07] = minor;
    return entry_point;
}

/// @brief True when running as root (uid 0) - some probes in this suite
/// (unreadable-file via chmod 000) are meaningless for root, which bypasses
/// file permission bits entirely.
bool running_as_root()
{
    return ::geteuid() == 0;
}

} // namespace

BOOST_AUTO_TEST_SUITE(LinuxSysfsSourceUnitTests)

/// Normal case: both fixture files present and readable - is_available()
/// reports true and try_read() successfully parses the version out of the
/// synthetic entry point and returns the DMI file's bytes verbatim.
BOOST_AUTO_TEST_CASE(BothFilesPresentAndReadableSucceedTestCase)
{
    SysfsFixtureDir fixture;

    const std::vector<uint8_t> expected_table{0x01, 0x02, 0x03, 0x04, 0x05};
    write_file(fixture.entry_point_path(), make_32bit_entry_point(2, 7));
    write_file(fixture.table_path(), expected_table);

    LinuxSysfsSource source(fixture.entry_point_path(), fixture.table_path());

    BOOST_CHECK(source.is_available());

    std::vector<uint8_t> out_table;
    size_t major_version = 0;
    size_t minor_version = 0;
    bool succeeded = false;
    BOOST_CHECK_NO_THROW(succeeded = source.try_read(out_table, major_version, minor_version));

    BOOST_CHECK(succeeded);
    BOOST_CHECK_EQUAL(major_version, static_cast<size_t>(2));
    BOOST_CHECK_EQUAL(minor_version, static_cast<size_t>(7));
    BOOST_CHECK_EQUAL_COLLECTIONS(out_table.begin(), out_table.end(), expected_table.begin(), expected_table.end());
}

/// Edge case: the entry point file is missing entirely (DMI present) -
/// is_available() must report false, and try_read() must fail cleanly
/// rather than throwing.
BOOST_AUTO_TEST_CASE(EntryPointFileMissingFallsThroughTestCase)
{
    SysfsFixtureDir fixture;
    write_file(fixture.table_path(), std::vector<uint8_t>{0x01});
    // entry_point_path() deliberately never written.

    LinuxSysfsSource source(fixture.entry_point_path(), fixture.table_path());

    BOOST_CHECK(!source.is_available());

    std::vector<uint8_t> out_table;
    size_t major_version = 0;
    size_t minor_version = 0;
    bool succeeded = true;
    BOOST_CHECK_NO_THROW(succeeded = source.try_read(out_table, major_version, minor_version));
    BOOST_CHECK(!succeeded);
}

/// Edge case: the DMI table file is missing entirely (entry point
/// present) - same contract as the entry-point-missing case above.
BOOST_AUTO_TEST_CASE(TableFileMissingFallsThroughTestCase)
{
    SysfsFixtureDir fixture;
    write_file(fixture.entry_point_path(), make_32bit_entry_point(2, 7));
    // table_path() deliberately never written.

    LinuxSysfsSource source(fixture.entry_point_path(), fixture.table_path());

    BOOST_CHECK(!source.is_available());

    std::vector<uint8_t> out_table;
    size_t major_version = 0;
    size_t minor_version = 0;
    bool succeeded = true;
    BOOST_CHECK_NO_THROW(succeeded = source.try_read(out_table, major_version, minor_version));
    BOOST_CHECK(!succeeded);
}

/// Edge case: both files missing (fixture directory created but empty) -
/// is_available() must report false.
BOOST_AUTO_TEST_CASE(BothFilesMissingFallsThroughTestCase)
{
    SysfsFixtureDir fixture;

    LinuxSysfsSource source(fixture.entry_point_path(), fixture.table_path());

    BOOST_CHECK(!source.is_available());
}

/// Edge case: the entry point file exists but is unreadable (chmod 000) -
/// is_available() must report false rather than the
/// std::filesystem::exists()-style false positive this source's
/// is_available() is specifically designed to avoid. Skipped when running
/// as root (CI may run privileged), since root bypasses file permission
/// bits and chmod 000 would not actually block the read.
BOOST_AUTO_TEST_CASE(EntryPointFileUnreadableReportsUnavailableTestCase)
{
    if (running_as_root()) {
        return;
    }

    SysfsFixtureDir fixture;
    write_file(fixture.entry_point_path(), make_32bit_entry_point(2, 7));
    write_file(fixture.table_path(), std::vector<uint8_t>{0x01});

    BOOST_REQUIRE(::chmod(fixture.entry_point_path().c_str(), 0) == 0);

    LinuxSysfsSource source(fixture.entry_point_path(), fixture.table_path());
    BOOST_CHECK(!source.is_available());

    // Restore permissions so the fixture directory's own destructor can
    // remove the file.
    ::chmod(fixture.entry_point_path().c_str(), 0644);
}

/// Normal case: source_name() reports the exact literal "linux-sysfs".
BOOST_AUTO_TEST_CASE(SourceNameReportsLinuxSysfsTestCase)
{
    LinuxSysfsSource source;
    BOOST_CHECK_EQUAL(std::string(source.source_name()), std::string("linux-sysfs"));
}

BOOST_AUTO_TEST_SUITE_END()

#endif // defined(__linux__)
