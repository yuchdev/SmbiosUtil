#include <cstddef>

#include <smbios/os_interface/smbios_version_fixup.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests for fixup_smbios_version() (milestone 0005, plan.md R9),
// ported from c-versions/dmidecode_linux/dmidecode.c's dmi_fixup_version():
// deterministic, pure-function, no hardware/privileges needed.

BOOST_AUTO_TEST_SUITE(SmbiosVersionFixupUnitTests)

/// Normal case: a BIOS reporting the real-world malformed "2.31" pair
/// (major=2, raw minor byte 0x1F == 31 decimal, packing to 0x021F) is
/// corrected to 2.3.
BOOST_AUTO_TEST_CASE(FixupCorrects0x021FTo2Dot3TestCase)
{
    size_t major = 2;
    size_t minor = 0x1F;

    fixup_smbios_version(major, minor);

    BOOST_CHECK_EQUAL(major, static_cast<size_t>(2));
    BOOST_CHECK_EQUAL(minor, static_cast<size_t>(3));
}

/// Edge case: the second malformed pair that maps to the same corrected
/// value as 0x021F - "2.33" (raw minor byte 0x21 == 33 decimal, packing to
/// 0x0221) - is also corrected to 2.3, confirming both case labels in the
/// reference's switch are reproduced, not just one of them.
BOOST_AUTO_TEST_CASE(FixupCorrects0x0221To2Dot3TestCase)
{
    size_t major = 2;
    size_t minor = 0x21;

    fixup_smbios_version(major, minor);

    BOOST_CHECK_EQUAL(major, static_cast<size_t>(2));
    BOOST_CHECK_EQUAL(minor, static_cast<size_t>(3));
}

/// Normal case: a BIOS reporting the real-world malformed "2.51" pair
/// (raw minor byte 0x33 == 51 decimal, packing to 0x0233) is corrected to
/// 2.6.
BOOST_AUTO_TEST_CASE(FixupCorrects0x0233To2Dot6TestCase)
{
    size_t major = 2;
    size_t minor = 0x33;

    fixup_smbios_version(major, minor);

    BOOST_CHECK_EQUAL(major, static_cast<size_t>(2));
    BOOST_CHECK_EQUAL(minor, static_cast<size_t>(6));
}

/// Invalid-input/pass-through case: an already-valid version (major=2,
/// minor=0) is not in the malformed table and must pass through entirely
/// unchanged.
BOOST_AUTO_TEST_CASE(FixupLeavesAlreadyValidVersionUnchangedTestCase)
{
    size_t major = 2;
    size_t minor = 0;

    fixup_smbios_version(major, minor);

    BOOST_CHECK_EQUAL(major, static_cast<size_t>(2));
    BOOST_CHECK_EQUAL(minor, static_cast<size_t>(0));
}

/// Edge case: a version with a major other than 2 (e.g. 3.0), which could
/// never collide with any of the malformed (major=2, ...) packed values,
/// is also left unchanged.
BOOST_AUTO_TEST_CASE(FixupLeavesDifferentMajorVersionUnchangedTestCase)
{
    size_t major = 3;
    size_t minor = 0;

    fixup_smbios_version(major, minor);

    BOOST_CHECK_EQUAL(major, static_cast<size_t>(3));
    BOOST_CHECK_EQUAL(minor, static_cast<size_t>(0));
}

BOOST_AUTO_TEST_SUITE_END()
