#include <limits>
#include <memory>
#include <vector>

#include <smbios/sys_interface/smbios_source_chain.hpp>
#include <smbios/legacy/legacy_memory_scan_source.hpp>
#include <smbios/windows/win_bios.hpp>

using namespace smbios;

namespace {

/// @brief Given an already-constructed, already-attempted native
/// `SMBiosImpl`, copy its result out into `ISmbiosSource::try_read()`'s
/// out-parameters exactly as the pre-refactor `SMBios::get_table_base()`/
/// `get_table_size()`/`get_smbios_version()` would have exposed it: the
/// major/minor version comes from the native implementation only when it
/// actually supplies one (Windows does; the `*nix` `SMBiosImpl` always
/// reports `numeric_limits<size_t>::max()`, which the pre-refactor
/// `get_smbios_version()` treated as "no version from native" and defaulted
/// to `0`/`0`, since `extract_dmi_version()` was only ever invoked on the
/// legacy-scan branch).
///
/// Milestone 0005, Story 01.1 (ADR 0001): relocated verbatim out of the
/// single, `#if`-guarded `smbios_source_chain.cpp` into this library's own
/// translation unit - duplicated byte-for-byte in
/// `smbios/src/linux/linux_source_chain.cpp` rather than shared,
/// since `smbios_windows` and `smbios_linux` are never linked
/// into the same build and sharing it would require a third library for a
/// ~20-line adapter helper that Story 04.0 (Windows) is expected to replace
/// outright.
bool copy_native_result(const SMBiosImpl& native_impl, std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version)
{
    const uint8_t* base = native_impl.get_table_base();
    size_t size = native_impl.get_table_size();
    if (nullptr == base || 0 == size) {
        return false;
    }
    out_table.assign(base, base + size);

    size_t native_major = native_impl.get_major_version();
    size_t native_minor = native_impl.get_minor_version();
    if (native_major != std::numeric_limits<size_t>::max() && native_minor != std::numeric_limits<size_t>::max()) {
        major_version = native_major;
        minor_version = native_minor;
    }
    else {
        major_version = 0;
        minor_version = 0;
    }
    return true;
}

/// @brief Thin `ISmbiosSource` adapter around the existing Windows-native
/// `SMBiosImpl` (`win_bios.h`, `GetSystemFirmwareTable()`-based). Delegates
/// entirely to it - no Windows-specific logic is rewritten here (plan.md
/// R6).
///
/// Milestone 0005, Story 01.1 (ADR 0001): this class and its
/// `make_default_source_chain()` definition below were relocated out of
/// `smbios_source_chain.cpp`'s `#if defined(_WIN32)` branch into this
/// library's own translation unit - the class itself is unchanged, only its
/// home and the mechanism that selects it (a CMake `if(WIN32)` link choice
/// instead of a C++ `#if`).
///
/// Milestone 0005, Story 04.0, Task 01: this design question is now
/// ratified permanent, not temporary. `SMBiosImpl` (`win_bios.h`) has no
/// `is_available()`/`try_read()`/`source_name()`-shaped methods of its own
/// and is used nowhere else as a value type in its own right - only ever
/// constructed and consumed right here - so giving it `: public
/// ISmbiosSource` directly would mean editing a file this story's own
/// "zero Windows API call site changed" constraint (R6) is meant to keep
/// untouched, for no behavioral gain over the wrapper that already exists.
/// Keeping this thin delegating class is therefore the genuinely
/// zero-diff choice the story README asks for (`win_bios.h`/`.cpp`/
/// `win_native_api_helper.h` are unmodified by this story), not merely the
/// smaller one.
///
/// Also as of Story 04.0: no longer internally falls back to
/// `LegacyMemoryScanSource` on a failed native read - `try_read()` now
/// returns `false` and lets `make_default_source_chain()`'s second entry
/// (below) run instead, same shape every other platform's chain already
/// uses (`LinuxSysfsSource`/`LinuxEfiSource`/`LegacyMemoryScanSource`,
/// `MacOsUefiSource`/`LegacyMemoryScanSource`). The pre-Story-04.0 version
/// delegated to the scan from inside this class's own `try_read()`, which
/// meant `source_name()` would still report `"windows-native"` even when
/// the real data came from the legacy scan - a diagnostic-accuracy bug,
/// not a behavior worth preserving under R6 (R6 covers the Windows API
/// call semantics, not which string `source_name()` happens to report).
/// The *overall* observable outcome (does a working table get found at
/// all) is unchanged either way.
///
/// See `status.md`'s Story 04.0 entry for this class's real Windows-CI
/// confirmation (not just read from `win_bios.h`'s interface, as this
/// class's implementation originally was).
class WindowsNativeSource : public ISmbiosSource {
public:
    bool is_available() const override
    {
        // No cheaper probe exists than attempting the real read -
        // SMBiosImpl's own constructor already performs it eagerly (same
        // as today). Report true unconditionally so try_read() is always
        // given the chance; try_read() itself reports success/failure
        // precisely.
        return true;
    }

    bool try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const override
    {
        SMBiosImpl native_impl;

        if (!native_impl.smbios_read_success()) {
            return false;
        }

        return copy_native_result(native_impl, out_table, major_version, minor_version);
    }

    const char* source_name() const override { return "windows-native"; }
};

} // namespace

std::vector<std::unique_ptr<ISmbiosSource>> smbios::make_default_source_chain()
{
    std::vector<std::unique_ptr<ISmbiosSource>> chain;
    chain.push_back(std::make_unique<WindowsNativeSource>());
    chain.push_back(std::make_unique<LegacyMemoryScanSource>());
    return chain;
}
