#include "support/local_only_guard.hpp"

/// @file launch_autoexit_twin_test.cpp
/// @brief Local-only launch of the `smbios_gui_autoexit` test twin over the
/// live table, checking for stray output and files (milestone 0011, Story
/// 04.0 Task 04; manual-testing.md M-01 launch/exit and M-14).
///
/// - Type: integration (local-only tier, labels `gui;gui-local`). Needs a
///   display and GL; a window briefly appears (about 2 s: 120 frames at
///   vsync) and shows live data - close screen shares first (threat model
///   T-11).
/// - Scenario: spawn the twin through spawn_and_capture() (no shell,
///   absolute path, literal argv) with `--exit-after-frames 120`, in a fresh
///   `mkdtemp` working directory (ScopedTempDirectory, R-17).
/// - Asserts: it exits by itself (status Exited) with exit status 0, writes
///   0 bytes to stdout and stderr together, and leaves the working directory
///   empty (no `imgui.ini`, no log, nothing else).
/// - Differs from `AutoExitTwinSmoke` (part A, 30 frames) by the M-14
///   assertions (no output, no files) and the spec's 120 frames.
/// - Output on failure: status names, exit status, byte and entry counts
///   only, e.g. `stdout+stderr bytes: 0 -> 57`; the child's output stays in
///   memory (T-3).
/// - Skips: as root (R-19); no display/GL (same hidden-window probe as
///   `HiddenWindowSmoke`, run before spawning).

#include <boost/test/unit_test.hpp>

#include "support/display_probe.hpp"
#include "support/local_skip.hpp"
#include "support/process_spawn.hpp"
#include "support/scoped_temp_directory.hpp"

#if !defined(SMBIOS_GUI_AUTOEXIT_PATH)
#error "SMBIOS_GUI_AUTOEXIT_PATH must name the smbios_gui_autoexit binary; see tests/gui/CMakeLists.txt"
#endif

namespace utf = boost::unit_test;
using smbios::test::gui::display_and_gl_available;
using smbios::test::gui::ScopedTempDirectory;
using smbios::test::gui::skip_if_root;
using smbios::test::gui::skip_unless;
using smbios::test::gui::spawn_and_capture;
using smbios::test::gui::SpawnResult;
using smbios::test::gui::status_name;

namespace {

constexpr const char* kCaseName = "LaunchAutoExitTwin";

} // namespace

BOOST_AUTO_TEST_SUITE(SmbiosGuiLocalTests)

/// Normal case: launch, 120 frames, clean exit 0, no output, no files.
BOOST_AUTO_TEST_CASE(LaunchAutoExitTwin,
    *utf::precondition([](utf::test_unit_id) {
        auto not_root = skip_if_root(kCaseName);
        if (!not_root) {
            return not_root;
        }
        return skip_unless(display_and_gl_available(), kCaseName,
                           "no display or OpenGL context (hidden GLFW window creation failed)");
    }))
{
    const ScopedTempDirectory working_directory;
    BOOST_REQUIRE_MESSAGE(working_directory.valid(), "temporary working directory: created -> not created");

    const SpawnResult result =
        spawn_and_capture(SMBIOS_GUI_AUTOEXIT_PATH, {"--exit-after-frames", "120"}, working_directory.path());

    BOOST_TEST((result.status == SpawnResult::Status::Exited), "status: Exited -> " << status_name(result.status));
    BOOST_TEST((result.exit_code == 0), "exit status: 0 -> " << result.exit_code);
    const std::size_t output_bytes = result.stdout_bytes.size() + result.stderr_bytes.size();
    BOOST_TEST((output_bytes == 0), "stdout+stderr bytes: 0 -> " << output_bytes);
    const auto entries = working_directory.entry_count();
    BOOST_TEST((entries.has_value() && *entries == 0),
               "working directory entries: 0 -> " << (entries ? static_cast<long long>(*entries) : -1LL));
}

BOOST_AUTO_TEST_SUITE_END()
