#include <cstdint>
#include <string>
#include <vector>

#include <smbios/source_legacy/legacy_memory_scan_source.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Link-seam proof for milestone 0005, Story 01.1, Task 03 - proving DD2/DD3
// from ADR 0001 (docs/adr/0001-per-os-smbios-source-libraries.md) in code,
// not just on paper: smbios_source_legacy has zero dependency on any per-OS
// library (smbios_source_windows/_macos/_linux) and compiles/links
// standalone.
//
// This is deliberately a thin re-statement of
// legacy_memory_scan_source_test.cpp's two already-landed assertions
// (is_available()/source_name()), not new behavior coverage - that file
// already owns LegacyMemoryScanSource's behavioral contract (Story 01.0,
// Task 02). What this file adds is the *link-graph* proof itself: this
// translation unit's only #include is
// smbios/source_legacy/legacy_memory_scan_source.h, and
// tests/unit/CMakeLists.txt links smbios_source_legacy explicitly for this
// reason (see that file's own comment) - so if a future change
// accidentally made smbios_source_legacy depend on a per-OS library, this
// test's own link step (not just its assertions) would be where that
// regression first surfaces, on whichever platform CI runs it.

BOOST_AUTO_TEST_SUITE(SmbiosSourceLegacyLinkUnitTests)

/// Normal case: is_available() always reports true, unconditionally - the
/// same contract legacy_memory_scan_source_test.cpp already asserts,
/// re-confirmed here as part of the link-seam proof rather than duplicated
/// behavior coverage.
BOOST_AUTO_TEST_CASE(IsAvailableAlwaysReturnsTrueTestCase)
{
    LegacyMemoryScanSource source;
    BOOST_CHECK(source.is_available());
}

/// Normal case: source_name() reports the exact literal "legacy-scan" -
/// confirmed against LegacyMemoryScanSource::source_name()'s real
/// implementation (smbios/src/source_legacy/legacy_memory_scan_source.cpp),
/// not assumed.
BOOST_AUTO_TEST_CASE(SourceNameReportsLegacyScanTestCase)
{
    LegacyMemoryScanSource source;
    BOOST_CHECK_EQUAL(std::string(source.source_name()), std::string("legacy-scan"));
}

BOOST_AUTO_TEST_SUITE_END()
