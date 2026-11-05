#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

// This suite is intentionally empty today.
//
// A "mock" test substitutes a test double for an external/platform boundary
// at a real seam (dependency injection, an interface). This project has no
// such seam yet: SMBios's native source and PhysicalMemory's /dev/mem access
// are both selected by a compile-time #if inside their own constructors, not
// through an injectable interface. Writing a "mock" test today would mean
// either faking a boundary that doesn't exist, or testing production code
// directly (which belongs in tests/unit/, not here).
//
// The real seam arrives with the ISmbiosSource strategy interface
// (docs/roadmap/0005-uefi-and-native-source-abstraction/plan.md) - once
// SMBios depends on that interface instead of a hardcoded #if chain, this
// file is where a test substitutes a fake ISmbiosSource and asserts SMBios's
// behavior against it (e.g. "falls through to the next source when
// is_available() is false").

BOOST_AUTO_TEST_SUITE(SmbiosMockTests)

/// @brief Not a test of production behavior: documents why this suite has no
/// real cases yet, and keeps the executable from reporting an empty test
/// tree as a setup error (Boost.Test treats a suite with zero test cases as
/// a framework-level error, not a pass). Remove this once ISmbiosSource
/// lands and real fake-substitution test cases are added above.
BOOST_AUTO_TEST_CASE(NoMockableSeamYetTestCase)
{
    BOOST_TEST_MESSAGE(
        "No injectable seam exists yet (SMBios/PhysicalMemory select their "
        "platform source via compile-time #if); see "
        "docs/roadmap/0005-uefi-and-native-source-abstraction/plan.md");
}

BOOST_AUTO_TEST_SUITE_END()
