#pragma once

#include "support/local_only_guard.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

/// @file process_spawn.hpp
/// @brief The local-only GUI tier's one and only way to start a child
/// process (milestone 0011, Story 04.0 Task 04; threat model
/// `docs/security/2026-11-11-gui-test-tiers.md`, T-1/T-8, R-16).
///
/// Used to launch the `smbios` CLI and the `smbios_gui_autoexit` test twin.
/// Rules it enforces:
/// - **No shell, no PATH search.** POSIX uses `fork` + `execv` (no shell,
///   no libc process helpers that search PATH); Windows uses
///   `CreateProcessW` with `lpApplicationName` set. The executable must be
///   an absolute path naming an existing regular file.
/// - **No inherited descriptors.** The POSIX child closes every descriptor
///   above 2 before `execv` and resets SIGPIPE to its default; on Windows
///   only the three standard handles are inherited.
/// - **Literal arguments only.** The executable path and every argument are
///   LiteralArg values, whose `consteval` constructor accepts only
///   compile-time constant strings (string literals or macros such as
///   `SMBIOS_GUI_AUTOEXIT_PATH` that expand to one). A registry-derived or
///   otherwise runtime string does not compile.
/// - **Working directory through the spawn API.** POSIX `chdir` runs in the
///   forked child, Windows passes `lpCurrentDirectory`; the test process's
///   own current directory is never changed.
/// - **Bounded.** A 30 s deadline (kSpawnDeadline), after which the child
///   (and, on POSIX, its process group; on Windows, its Job Object) is
///   killed and reaped. stdout and stderr are drained concurrently into
///   memory, capped at 64 MiB together (kSpawnOutputCap); past the cap the
///   child is killed. stdin is the null device.
/// - **In memory only.** Captured output is returned to the caller and is
///   never written to a file or printed by this helper. Callers must not
///   print it either (threat model T-3): assert on sizes and booleans only.
///
/// @warning The Windows implementation (CreateProcessW + Job Object) is
/// UNVERIFIED: the local-only tier is never compiled on CI and has only
/// been built and run on macOS. Treat a first Windows run as untested code.

namespace smbios::test::gui {

/// @brief A compile-time constant, NUL-terminated string: an executable
/// path or a command-line argument for spawn_and_capture().
///
/// The constructor is `consteval`, so only constant expressions (string
/// literals, or macros expanding to one) can create a LiteralArg. This makes
/// "a fixed list of literal arguments" (R-16) a compile-time property.
class LiteralArg {
public:
    /// @brief Wraps a string literal.
    /// @param text a compile-time constant, NUL-terminated string.
    consteval LiteralArg(const char* text) noexcept // NOLINT(google-explicit-constructor)
        : text_(text) {}

    /// @return the wrapped NUL-terminated string (static storage).
    [[nodiscard]] const char* c_str() const noexcept { return text_; }

private:
    const char* text_;
};

/// Deadline for one child process, from spawn to reap (R-16, T-8). Below
/// the 120 s CTest TIMEOUT of every local case.
inline constexpr std::chrono::seconds kSpawnDeadline{30};

/// Cap on stdout + stderr bytes captured from one child (T-8).
inline constexpr std::size_t kSpawnOutputCap = std::size_t{64} * 1024 * 1024;

/// @brief Outcome of spawn_and_capture().
struct SpawnResult {
    /// How the child ended (or why it never ran).
    enum class Status {
        /// The child exited by itself; exit_code is valid.
        Exited,
        /// POSIX only: the child was ended by a signal it did not get from
        /// this helper; signal_number is valid.
        Signaled,
        /// The child was still running at kSpawnDeadline and was killed.
        TimedOut,
        /// The child wrote more than kSpawnOutputCap bytes and was killed.
        OutputLimitExceeded,
        /// The child never ran: invalid path or working directory, or an
        /// OS call failed. Nothing was captured.
        SpawnFailed,
    };

    /// How the child ended.
    Status status = Status::SpawnFailed;
    /// The child's exit status; meaningful only for Status::Exited.
    int exit_code = -1;
    /// The terminating signal; meaningful only for Status::Signaled.
    int signal_number = 0;
    /// Everything the child wrote to stdout (in memory only; never print).
    std::string stdout_bytes;
    /// Everything the child wrote to stderr (in memory only; never print).
    std::string stderr_bytes;
    /// Wall-clock time from spawn to reap.
    std::chrono::milliseconds elapsed{0};
};

/// @brief Literal name of a SpawnResult::Status, for failure messages.
/// @param status the status to name.
/// @return a static string such as `"Exited"`; never caller data.
[[nodiscard]] std::string_view status_name(SpawnResult::Status status) noexcept;

/// @brief Runs `executable` with `arguments` in `working_directory`, waits
/// for it (at most kSpawnDeadline), and returns its exit status and output.
///
/// See the file comment for the rules. argv[0] is `executable` itself.
/// @param executable absolute path of an existing regular file.
/// @param arguments the arguments after argv[0], all literals.
/// @param working_directory absolute path of an existing directory, set as
///        the child's working directory through the spawn API.
/// @return the outcome; Status::SpawnFailed (with empty output) if the
///         preconditions on the paths do not hold or an OS call fails.
[[nodiscard]] SpawnResult spawn_and_capture(LiteralArg executable,
                                            std::span<const LiteralArg> arguments,
                                            const std::filesystem::path& working_directory);

/// @brief Convenience overload taking an inline argument list, e.g.
/// `spawn_and_capture(SMBIOS_GUI_AUTOEXIT_PATH, {"--exit-after-frames", "30"}, dir)`.
/// @copydetails spawn_and_capture(LiteralArg, std::span<const LiteralArg>, const std::filesystem::path&)
[[nodiscard]] inline SpawnResult spawn_and_capture(LiteralArg executable,
                                                   std::initializer_list<LiteralArg> arguments,
                                                   const std::filesystem::path& working_directory) {
    return spawn_and_capture(executable,
                             std::span<const LiteralArg>(arguments.begin(), arguments.size()),
                             working_directory);
}

} // namespace smbios::test::gui
