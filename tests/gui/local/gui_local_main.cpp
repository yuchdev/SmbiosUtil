#include "support/local_only_guard.hpp"

// Local-only GUI test tier (milestone 0011, Story 04.0):
// smbios_gui_local_test, CTest labels gui;gui-local, one CTest entry per
// case (smbios_gui_local_test.<CaseName>). Needs a real display, GL
// context, OS clipboard and the live table; never configured, compiled or
// registered on CI. A case whose resource is missing skips through
// skip_unless() instead of failing.
//
// Task 01 scaffolding: two placeholder cases. Task 04 replaces them with
// the real local cases. Keep SMBIOS_GUI_LOCAL_TEST_CASES in
// tests/gui/CMakeLists.txt in sync with the cases below.

#define BOOST_TEST_MODULE SmbiosGuiLocalTests
#include <boost/test/unit_test.hpp>

#include "support/local_skip.hpp"

namespace utf = boost::unit_test;
using smbios::test::gui::skip_unless;

BOOST_AUTO_TEST_SUITE(SmbiosGuiLocalTests)

/// Placeholder: proves the tier builds and runs, and that a passing case
/// is reported Passed next to a skipped sibling.
BOOST_AUTO_TEST_CASE(ScaffoldingPlaceholder)
{
    BOOST_TEST(true);
}

/// Placeholder: always skips, to prove skip_unless() prints one SKIP: line
/// and CTest reports the case as Skipped (not Passed, not Failed).
BOOST_AUTO_TEST_CASE(SkipHelperPlaceholder,
    *utf::precondition([](utf::test_unit_id) {
        return skip_unless(false, "SkipHelperPlaceholder",
                           "placeholder case, always skips (Task 01 scaffolding)");
    }))
{
    BOOST_TEST(false); // never reached: the precondition always skips
}

BOOST_AUTO_TEST_SUITE_END()
