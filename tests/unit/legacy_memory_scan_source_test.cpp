#include <cstdint>
#include <string>
#include <vector>

#include <smbios/source_legacy/legacy_memory_scan_source.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests for LegacyMemoryScanSource (milestone 0005, Story 01.0, Task
// 02): deterministic and privilege-free only to the extent the class's own
// contract allows - try_read() ultimately depends on real hardware/root
// access to a raw-physical-memory device, which this CI sandbox (an
// unprivileged macOS host, no /dev/mem) does not have. That is the same
// no-root/no-/dev/mem baseline every other test in this project already
// documents (see tests/perf, tests/integration), not a new finding -
// confirming try_read() degrades to `false` cleanly, without throwing, is
// itself the behavior under test here.

BOOST_AUTO_TEST_SUITE(LegacyMemoryScanSourceUnitTests)

/// Normal case: is_available() always reports true, unconditionally - this
/// source is only ever reached as the chain's last resort, so by contract
/// it always "tries" rather than pre-emptively declining.
BOOST_AUTO_TEST_CASE(IsAvailableAlwaysReturnsTrueTestCase)
{
    LegacyMemoryScanSource source;
    BOOST_CHECK(source.is_available());
}

/// Edge case: on a host with no accessible legacy physical-memory device
/// (this unprivileged sandbox - no /dev/mem at all on modern macOS, or
/// insufficient privilege on *nix/Windows), try_read() must degrade to
/// `false` cleanly rather than letting an exception escape, and must leave
/// out_table in a sane (here: still-empty) state rather than fabricating a
/// result.
BOOST_AUTO_TEST_CASE(TryReadFailsCleanlyWithoutPrivilegedAccessTestCase)
{
    LegacyMemoryScanSource source;

    std::vector<uint8_t> out_table;
    size_t major_version = 0;
    size_t minor_version = 0;

    bool succeeded = false;
    BOOST_CHECK_NO_THROW(succeeded = source.try_read(out_table, major_version, minor_version));

    BOOST_CHECK(!succeeded);
    BOOST_CHECK(out_table.empty());
}

/// Normal case: source_name() reports the diagnostic name
/// SMBios::render_to_description() surfaces when this source is what
/// succeeded.
BOOST_AUTO_TEST_CASE(SourceNameReportsLegacyScanTestCase)
{
    LegacyMemoryScanSource source;
    BOOST_CHECK_EQUAL(std::string(source.source_name()), std::string("legacy-scan"));
}

BOOST_AUTO_TEST_SUITE_END()
