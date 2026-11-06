#include <limits>
#include <memory>
#include <vector>

#include <smbios/source_interface/smbios_source_chain.h>
#include <smbios/source_legacy/legacy_memory_scan_source.h>
#include <smbios/source_windows/win_bios.h>

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
/// `smbios/src/source_linux/linux_source_chain.cpp` rather than shared,
/// since `smbios_source_windows` and `smbios_source_linux` are never linked
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
/// R6) - and falls back to `LegacyMemoryScanSource` exactly as the
/// pre-refactor `SMBios::SMBios()` did whenever the native read failed.
///
/// Milestone 0005, Story 01.1 (ADR 0001): this class and its
/// `make_default_source_chain()` definition below were relocated out of
/// `smbios_source_chain.cpp`'s `#if defined(_WIN32)` branch into this
/// library's own translation unit - the class itself is unchanged, only its
/// home and the mechanism that selects it (a CMake `if(WIN32)` link choice
/// instead of a C++ `#if`). This is still a **temporary** adapter: Story
/// 04.0's own Task 01 is expected to give `SMBiosImpl` (or its replacement)
/// `: public ISmbiosSource` directly and either keep or replace this class
/// outright - either way is acceptable per that story's own constraints;
/// this comment records that the decision is still open as of this story.
///
/// Unverified on real Windows hardware: written from reading `win_bios.h`'s
/// interface only (this implementation was built and tested on macOS).
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

        if (native_impl.smbios_read_success()) {
            return copy_native_result(native_impl, out_table, major_version, minor_version);
        }

        return LegacyMemoryScanSource{}.try_read(out_table, major_version, minor_version);
    }

    const char* source_name() const override { return "windows-native"; }
};

} // namespace

std::vector<std::unique_ptr<ISmbiosSource>> smbios::make_default_source_chain()
{
    std::vector<std::unique_ptr<ISmbiosSource>> chain;
    chain.push_back(std::make_unique<WindowsNativeSource>());
    return chain;
}
