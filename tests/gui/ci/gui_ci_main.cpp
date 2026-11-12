// CI-safe GUI test tier (milestone 0011, Story 04.0): smbios_gui_ci_test,
// CTest labels gui;gui-ci. Hermetic (no window, no GL, no OS clipboard,
// synthetic tables only) and it never skips: a case that cannot run is a
// failure. Nothing in this directory may include the local tier's guard
// header, a windowing-library header or an ImGui platform/renderer backend
// header (tests/gui/CMakeLists.txt; Task 05 greps for them).
//
// This file only defines the Boost.Test module. The cases live in the
// other ci/*.cpp files: headless_spike_test.cpp (Task 02, spike items
// S1-S6, on the support/headless_imgui.hpp harness), and Task 03's widget
// suites entry_browser_headless_test.cpp (GuiEntryBrowserHeadlessTests) and
// copy_as_headless_test.cpp (GuiCopyAsHeadlessTests), which share
// support/browser_fixture.hpp.

#define BOOST_TEST_MODULE SmbiosGuiCiTests
#include <boost/test/unit_test.hpp>
