#include "support/local_only_guard.hpp"

// Local-only GUI test tier (milestone 0011, Story 04.0):
// smbios_gui_local_test, CTest labels gui;gui-local, one CTest entry per
// case (smbios_gui_local_test.<CaseName>). Needs a real display, GL
// context, OS clipboard and the live table; never configured, compiled or
// registered on CI. A case whose resource is missing, or a run as root,
// skips with one `SKIP: <case>: <reason>` line instead of failing.
//
// This file only defines the Boost.Test module. The cases of suite
// SmbiosGuiLocalTests live in the other local/*.cpp files (Task 04); the
// two Task 01 placeholder cases were removed once real cases covered what
// they proved (see docs/test/test-subsystem.md). Keep
// SMBIOS_GUI_LOCAL_TEST_CASES in tests/gui/CMakeLists.txt in sync with the
// cases.

#define BOOST_TEST_MODULE SmbiosGuiLocalTests
#include <boost/test/unit_test.hpp>
