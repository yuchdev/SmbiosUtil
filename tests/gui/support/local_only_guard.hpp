#pragma once

/// @file local_only_guard.hpp
/// @brief Compile-time guard for the local-only GUI test tier (milestone
/// 0011, Story 04.0).
///
/// Every source of the local-only tier (`tests/gui/local/*.cpp` and the
/// local-only helpers in `tests/gui/support/`) includes this header FIRST.
/// It fails the compile unless `SMBIOS_GUI_LOCAL_ONLY_TESTS` is defined.
/// Only `tests/gui/CMakeLists.txt` defines it, as a PRIVATE definition of
/// the `smbios_gui_local_test` target, and that target is never created on
/// CI. This is the second gate behind the CMake one: a local source pulled
/// into a CI build by mistake fails to compile instead of trying to open a
/// display, the OS clipboard or the live SMBIOS table there.
///
/// The CI-safe tier (`tests/gui/ci/`) must never include this header.

#if !defined(SMBIOS_GUI_LOCAL_ONLY_TESTS)
#error "local-only GUI test compiled without SMBIOS_GUI_LOCAL_ONLY_TESTS; see tests/gui/CMakeLists.txt"
#endif
