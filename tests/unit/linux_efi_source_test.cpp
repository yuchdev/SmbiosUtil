#include <cstdint>
#include <string>
#include <vector>

// Milestone 0005, Story 02.0, Task 02 (ADR 0001): LinuxEfiSource lives in
// the Linux-only smbios_linux library, so this entire file is a
// no-op translation unit on every other platform - matching how
// tests/unit/linux_sysfs_source_test.cpp (Task 01's template for this
// file's structure/guard style) narrows its own coverage to __linux__ for
// the identical reason.
#if defined(__linux__)

#include <fstream>
#include <filesystem>

#include <unistd.h>

#include <smbios/linux/linux_efi_source.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

namespace {

/// @brief RAII temp-directory fixture holding one candidate systab file,
/// so `LinuxEfiSource(primary_path, fallback_path)` can be pointed at it
/// instead of the real `/sys/firmware/efi/systab` / `/proc/efi/systab`
/// paths (see that constructor's doc comment in
/// `smbios/include/smbios/linux/linux_efi_source.h` for why it
/// exists). Only the parsing stage (Stage 1) is exercised here - the
/// `/dev/mem` mapping stage is an explicit real-hardware manual check per
/// the task spec, not something a portable fixture can substitute for.
class SystabFixtureDir {
public:
    SystabFixtureDir()
        : dir_(std::filesystem::temp_directory_path() /
               std::filesystem::path("smbios_efi_fixture_" + std::to_string(::getpid())))
    {
        std::filesystem::create_directories(dir_);
    }

    ~SystabFixtureDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(dir_, ignored);
    }

    /// @brief A path inside the fixture directory that is never written
    /// to - stands in for "this candidate path does not exist/open".
    std::string missing_path() const { return (dir_ / "does_not_exist").string(); }

    std::string systab_path() const { return (dir_ / "systab").string(); }

private:
    std::filesystem::path dir_;
};

/// @brief Write `contents` to `path`, truncating/creating as needed.
void write_file(const std::string& path, const std::string& contents)
{
    std::ofstream file(path, std::ios::trunc);
    file << contents;
}

} // namespace

BOOST_AUTO_TEST_SUITE(LinuxEfiSourceUnitTests)

/// Normal case: a systab file with a 32-bit `SMBIOS=0x...` line (plus
/// unrelated lines before/after, matching a real systab's shape) -
/// is_available() must report true and try_read()'s Stage-1 parse must
/// recover the exact address. (try_read() itself still returns false
/// here, since there is no real /dev/mem behind this address in a unit
/// test - that failure is expected and exercised separately below.)
BOOST_AUTO_TEST_CASE(Smbios32LineIsParsedTestCase)
{
    SystabFixtureDir fixture;
    write_file(fixture.systab_path(),
               "ACPI20=0x7f8ff000\n"
               "SMBIOS=0xf0000\n"
               "MPS=0x7f8fe000\n");

    LinuxEfiSource source(fixture.systab_path(), fixture.missing_path());

    BOOST_CHECK(source.is_available());

    std::vector<uint8_t> out_table;
    size_t major_version = 0;
    size_t minor_version = 0;
    bool succeeded = true;
    BOOST_CHECK_NO_THROW(succeeded = source.try_read(out_table, major_version, minor_version));
    // No accessible /dev/mem behind this synthetic address in a unit
    // test environment - degrades to false, never throws.
    BOOST_CHECK(!succeeded);
}

/// Normal case: a systab file with a 64-bit `SMBIOS3=0x...` line -
/// is_available() must report true for the SMBIOS3 key too.
BOOST_AUTO_TEST_CASE(Smbios64LineIsParsedTestCase)
{
    SystabFixtureDir fixture;
    write_file(fixture.systab_path(), "SMBIOS3=0x3fed2000\n");

    LinuxEfiSource source(fixture.systab_path(), fixture.missing_path());

    BOOST_CHECK(source.is_available());
}

/// Edge case: the systab file opens but contains no SMBIOS/SMBIOS3
/// line - "found interface but no SMBIOS pointer" is still a failure for
/// this source.
BOOST_AUTO_TEST_CASE(NoMatchingLineReportsUnavailableTestCase)
{
    SystabFixtureDir fixture;
    write_file(fixture.systab_path(),
               "ACPI20=0x7f8ff000\n"
               "MPS=0x7f8fe000\n");

    LinuxEfiSource source(fixture.systab_path(), fixture.missing_path());

    BOOST_CHECK(!source.is_available());

    std::vector<uint8_t> out_table;
    size_t major_version = 0;
    size_t minor_version = 0;
    bool succeeded = true;
    BOOST_CHECK_NO_THROW(succeeded = source.try_read(out_table, major_version, minor_version));
    BOOST_CHECK(!succeeded);
}

/// Edge case: neither the primary nor the fallback systab path opens at
/// all - "no EFI interface at all" - is_available() must report false.
BOOST_AUTO_TEST_CASE(NeitherPathOpensReportsUnavailableTestCase)
{
    SystabFixtureDir fixture;
    // Neither path is ever written.

    LinuxEfiSource source(fixture.missing_path(), fixture.missing_path());

    BOOST_CHECK(!source.is_available());

    std::vector<uint8_t> out_table;
    size_t major_version = 0;
    size_t minor_version = 0;
    bool succeeded = true;
    BOOST_CHECK_NO_THROW(succeeded = source.try_read(out_table, major_version, minor_version));
    BOOST_CHECK(!succeeded);
}

/// Normal case: the primary path does not open, but the fallback does -
/// mirrors address_from_efi()'s "/sys/firmware/efi/systab, else
/// /proc/efi/systab" fallback order.
BOOST_AUTO_TEST_CASE(FallbackPathUsedWhenPrimaryMissingTestCase)
{
    SystabFixtureDir fixture;
    write_file(fixture.systab_path(), "SMBIOS=0xf0000\n");

    LinuxEfiSource source(fixture.missing_path(), fixture.systab_path());

    BOOST_CHECK(source.is_available());
}

/// Normal case: source_name() reports the exact literal
/// "linux-efi-systab".
BOOST_AUTO_TEST_CASE(SourceNameReportsLinuxEfiSystabTestCase)
{
    LinuxEfiSource source;
    BOOST_CHECK_EQUAL(std::string(source.source_name()), std::string("linux-efi-systab"));
}

BOOST_AUTO_TEST_SUITE_END()

#endif // defined(__linux__)
