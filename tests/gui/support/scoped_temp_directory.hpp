#pragma once

#include "support/local_only_guard.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>

/// @file scoped_temp_directory.hpp
/// @brief A fresh, private, empty temporary directory for one local-only
/// GUI test case (milestone 0011, Story 04.0 Task 04; threat model
/// `docs/security/2026-11-11-gui-test-tiers.md`, R-17). Used as the working
/// directory of the `smbios_gui_autoexit` test twin, so a stray file (for
/// example an `imgui.ini`) would show up in it.

namespace smbios::test::gui {

/// @brief Owns a newly created, uniquely named temporary directory and
/// removes it (recursively) on destruction.
///
/// POSIX: created by `mkdtemp()`, so it is new and has mode 0700.
/// Windows (UNVERIFIED, the local tier is never compiled on CI): a random
/// name under `std::filesystem::temp_directory_path()`, created with
/// `create_directory()` and retried if the name already exists.
///
/// Destroy it only after every child process that used it has been reaped
/// (spawn_and_capture() always reaps before returning).
class ScopedTempDirectory {
public:
    /// @brief Creates the directory. Check valid() before using path().
    ScopedTempDirectory();
    ScopedTempDirectory(const ScopedTempDirectory&) = delete;
    ScopedTempDirectory& operator=(const ScopedTempDirectory&) = delete;
    /// @brief Removes the directory and everything in it; errors ignored.
    ~ScopedTempDirectory();

    /// @return true if the directory was created.
    [[nodiscard]] bool valid() const noexcept { return !path_.empty(); }

    /// @return the directory's absolute path; empty if !valid().
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    /// @brief Counts the directory's entries (not recursive). Report this
    /// count only, never the entries' names (R-17).
    /// @return the number of entries, or std::nullopt if it cannot be read.
    [[nodiscard]] std::optional<std::size_t> entry_count() const;

private:
    std::filesystem::path path_;
};

} // namespace smbios::test::gui
