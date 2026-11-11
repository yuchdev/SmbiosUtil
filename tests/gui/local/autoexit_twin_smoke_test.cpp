#include "support/local_only_guard.hpp"

/// @file autoexit_twin_smoke_test.cpp
/// @brief Local-only smoke cases for the `smbios_gui_autoexit` test twin
/// and the spawn helper (milestone 0011, Story 04.0 Task 04, part A).
///
/// - Type: integration (local-only tier, labels `gui;gui-local`).
/// - Scenario: launch the twin through spawn_and_capture() - once with a
///   valid `--exit-after-frames 30` (needs a real display and GL context;
///   a window briefly appears), and with a set of malformed command lines
///   that the hook must reject before any window or table read.
/// - Boundaries: the twin's argv grammar (threat model R-14); the spawn
///   helper's 30 s deadline (R-16). Root skips every case (R-19).
/// - Output: status names, exit codes, byte counts and indices only. The
///   child's stdout/stderr stay in memory and are never printed (T-3).
/// - On failure, first check: that `smbios_gui_autoexit` was rebuilt (it
///   is a dependency of this target), that a display session exists, and
///   whether a modal macOS permission prompt held the window open.

#include <boost/test/unit_test.hpp>

#include <string_view>
#include <vector>

#include "support/display_probe.hpp"
#include "support/local_skip.hpp"
#include "support/process_spawn.hpp"
#include "support/scoped_temp_directory.hpp"

#if !defined(SMBIOS_GUI_AUTOEXIT_PATH)
#error "SMBIOS_GUI_AUTOEXIT_PATH must name the smbios_gui_autoexit binary; see tests/gui/CMakeLists.txt"
#endif

namespace utf = boost::unit_test;
using smbios::test::gui::display_and_gl_available;
using smbios::test::gui::kSpawnDeadline;
using smbios::test::gui::LiteralArg;
using smbios::test::gui::ScopedTempDirectory;
using smbios::test::gui::skip_if_root;
using smbios::test::gui::skip_unless;
using smbios::test::gui::spawn_and_capture;
using smbios::test::gui::SpawnResult;
using smbios::test::gui::status_name;

namespace {

/// The twin's fixed usage line (smbios_gui/src/main.cpp, test hook part 1).
constexpr std::string_view kExpectedUsage = "smbios_gui: usage: --exit-after-frames <1-100000>\n";

} // namespace

BOOST_AUTO_TEST_SUITE(SmbiosGuiLocalTests)

/// Normal case: the twin opens its window, renders 30 frames, closes itself
/// through glfwSetWindowShouldClose() and exits 0 well before the deadline.
BOOST_AUTO_TEST_CASE(AutoExitTwinSmoke,
    *utf::precondition([](utf::test_unit_id) {
        auto not_root = skip_if_root("AutoExitTwinSmoke");
        if (!not_root) {
            return not_root;
        }
        return skip_unless(display_and_gl_available(), "AutoExitTwinSmoke",
                           "no display or OpenGL context (hidden GLFW window creation failed)");
    }))
{
    const ScopedTempDirectory working_directory;
    BOOST_REQUIRE_MESSAGE(working_directory.valid(), "AutoExitTwinSmoke: temporary directory not created");

    const SpawnResult result = spawn_and_capture(SMBIOS_GUI_AUTOEXIT_PATH,
                                                 {"--exit-after-frames", "30"},
                                                 working_directory.path());

    BOOST_TEST((result.status == SpawnResult::Status::Exited),
               "AutoExitTwinSmoke: status: Exited -> " << status_name(result.status));
    BOOST_TEST((result.exit_code == 0), "AutoExitTwinSmoke: exit status: 0 -> " << result.exit_code);
    BOOST_TEST((result.elapsed < kSpawnDeadline),
               "AutoExitTwinSmoke: elapsed ms: < " << kSpawnDeadline.count() * 1000 << " -> "
                                                   << result.elapsed.count());
}

/// Invalid input: every malformed command line is rejected before any
/// window or table read, with exit status 2, the fixed usage line on stderr
/// (argv never echoed) and nothing on stdout. Needs no display.
BOOST_AUTO_TEST_CASE(AutoExitTwinRejectsBadArguments,
    *utf::precondition([](utf::test_unit_id) { return skip_if_root("AutoExitTwinRejectsBadArguments"); }))
{
    const ScopedTempDirectory working_directory;
    BOOST_REQUIRE_MESSAGE(working_directory.valid(),
                          "AutoExitTwinRejectsBadArguments: temporary directory not created");

    // Each row is one rejected argv (after argv[0]); index = row number.
    const std::vector<std::vector<LiteralArg>> rejected = {
        {},                                       // 0: no arguments
        {"--exit-after-frames"},                  // 1: missing N
        {"--exit-after-frames", "0"},             // 2: below range
        {"--exit-after-frames", "100001"},        // 3: above range
        {"--exit-after-frames", "1234567"},       // 4: 7 digits
        {"--exit-after-frames", "+5"},            // 5: sign
        {"--exit-after-frames", " 5"},            // 6: whitespace
        {"--exit-after-frames", "5x"},            // 7: suffix
        {"--exit-after-frames", "0x10"},          // 8: hex prefix
        {"--exit-after-frames", ""},              // 9: empty
        {"--exit-after-frames", "-1"},            // 10: negative
        {"--exit-after-frame", "5"},              // 11: wrong flag
        {"--exit-after-frames=5"},                // 12: joined form
        {"--exit-after-frames", "5", "5"},        // 13: extra argument
        {"5", "--exit-after-frames"},             // 14: swapped order
    };
    for (std::size_t row = 0; row < rejected.size(); ++row) {
        const SpawnResult result = spawn_and_capture(SMBIOS_GUI_AUTOEXIT_PATH, rejected[row],
                                                     working_directory.path());
        BOOST_TEST((result.status == SpawnResult::Status::Exited),
                   "rejected argv #" << row << ": status: Exited -> " << status_name(result.status));
        BOOST_TEST((result.exit_code == 2), "rejected argv #" << row << ": exit status: 2 -> " << result.exit_code);
        BOOST_TEST((result.stdout_bytes.empty()),
                   "rejected argv #" << row << ": stdout bytes: 0 -> " << result.stdout_bytes.size());
        BOOST_TEST((result.stderr_bytes == kExpectedUsage),
                   "rejected argv #" << row << ": stderr is not the fixed usage line (bytes: "
                                     << kExpectedUsage.size() << " -> " << result.stderr_bytes.size() << ")");
    }
    const auto entries = working_directory.entry_count();
    BOOST_TEST((entries && *entries == 0),
               "AutoExitTwinRejectsBadArguments: working directory entries: 0 -> "
                   << (entries ? static_cast<long long>(*entries) : -1LL));
}

BOOST_AUTO_TEST_SUITE_END()
