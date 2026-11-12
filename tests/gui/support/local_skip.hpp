#pragma once

#include "support/local_only_guard.hpp"

#include <iostream>
#include <string_view>

#include <boost/test/unit_test.hpp>

#if !defined(_WIN32)
#include <unistd.h>
#endif

/// @file local_skip.hpp
/// @brief Skip helper for the local-only GUI test tier (milestone 0011,
/// Story 04.0). Local-only: guarded by local_only_guard.hpp.

namespace smbios::test::gui {

/// @brief Precondition result for a local-only case that needs a resource
/// (display, GL context, OS clipboard, readable live table) that may be
/// missing on this machine.
///
/// Use it inside a `boost::unit_test::precondition`:
/// @code
/// BOOST_AUTO_TEST_CASE(HiddenWindowSmoke,
///     *boost::unit_test::precondition([](boost::unit_test::test_unit_id) {
///         return skip_unless(probe_display(), "HiddenWindowSmoke",
///                            "no display (glfwInit failed)");
///     }))
/// @endcode
///
/// When the resource is missing it prints exactly one line to stdout,
/// `SKIP: <case_name>: <reason>`, and returns false, so Boost.Test skips
/// the case instead of failing it. tests/gui/CMakeLists.txt's
/// `SKIP_REGULAR_EXPRESSION` turns that line into a CTest *Skipped*
/// result. When the resource is available it prints nothing.
///
/// @param available whether the resource the case needs is present.
/// @param case_name the case's own name. A literal, never live or captured
///        data (threat model T-3).
/// @param reason a short literal reason, e.g. `no display (glfwInit failed)`.
///        Never live or captured data, never advice to elevate privileges.
/// @return true when `available`; otherwise false, with `reason` as the
///         Boost.Test skip message.
inline boost::test_tools::assertion_result skip_unless(bool available,
                                                       std::string_view case_name,
                                                       std::string_view reason)
{
    if (available) {
        return true;
    }
    std::cout << "SKIP: " << case_name << ": " << reason << std::endl;
    boost::test_tools::assertion_result result(false);
    result.message() << reason;
    return result;
}

/// Fixed reason of the running-as-root skip (threat model
/// `docs/security/2026-11-11-gui-test-tiers.md`, R-19). It deliberately
/// suggests no privilege change of any kind (G-8, M-12).
inline constexpr std::string_view kRunningAsRootSkipReason = "local GUI tests do not run as root";

/// @brief Reports whether the test process runs with root's effective
/// user ID.
/// @return true iff `geteuid() == 0` on POSIX; always false on Windows.
inline bool running_as_root() noexcept
{
#if !defined(_WIN32)
    return ::geteuid() == 0;
#else
    return false;
#endif
}

/// @brief Precondition that skips a local-only case when the process runs
/// as root (R-19): prints `SKIP: <case_name>: local GUI tests do not run as
/// root` through skip_unless(). Every local case that touches a display, a
/// child process, the clipboard or the live table checks this first.
/// @param case_name the case's own name (a literal).
/// @return true when not running as root; otherwise the skip result.
inline boost::test_tools::assertion_result skip_if_root(std::string_view case_name)
{
    return skip_unless(!running_as_root(), case_name, kRunningAsRootSkipReason);
}

} // namespace smbios::test::gui
