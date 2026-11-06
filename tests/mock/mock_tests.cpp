#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

// This suite is intentionally empty today.
//
// A "mock" test substitutes a test double for an external/platform boundary
// at a real seam (dependency injection, an interface). As of milestone 0005,
// Story 01.1, that seam now exists architecturally: SMBios's constructor
// walks an ordered vector<unique_ptr<ISmbiosSource>> returned by
// smbios::make_default_source_chain() (smbios/include/smbios/
// source_interface/ismbios_source.h, smbios_source_chain.h), with zero
// remaining compile-time #if inside SMBios itself - the per-platform choice
// is now a CMake-time link-seam (which per-OS library gets linked), not a
// preprocessor branch. The library boundary this test target would need to
// substitute a fake source behind is also in place
// (smbios_source_interface/smbios_source_legacy/smbios_source_windows/
// _macos/_linux - see docs/adr/0001-per-os-smbios-source-libraries.md).
//
// What is still missing is not the seam itself but a concrete fake
// ISmbiosSource to substitute through it - that is Stories 02.0/03.0/04.0's
// job (docs/roadmap/0005-uefi-and-native-source-abstraction/plan.md), once
// each platform's real source classes land and a test double can stand in
// for one of them. Writing a "mock" test before then would mean either
// faking a concrete source that doesn't exist yet, or testing production
// code directly (which belongs in tests/unit/, not here - see
// tests/unit/smbios_source_chain_test.cpp for the chain-walking logic
// already covered there with fakes at the ISmbiosSource seam itself).

BOOST_AUTO_TEST_SUITE(SmbiosMockTests)

/// @brief Not a test of production behavior: documents why this suite has no
/// real cases yet, and keeps the executable from reporting an empty test
/// tree as a setup error (Boost.Test treats a suite with zero test cases as
/// a framework-level error, not a pass). Remove this once ISmbiosSource
/// lands and real fake-substitution test cases are added above.
BOOST_AUTO_TEST_CASE(NoMockableSeamYetTestCase)
{
    BOOST_TEST_MESSAGE(
        "The ISmbiosSource seam and its library boundary now exist "
        "(milestone 0005, Story 01.1); this suite's remaining blocker is a "
        "concrete fake ISmbiosSource to substitute, not a missing seam - "
        "see docs/roadmap/0005-uefi-and-native-source-abstraction/plan.md");
}

BOOST_AUTO_TEST_SUITE_END()
