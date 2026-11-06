// This is the one remaining `#if defined(_WIN32)` in the whole native-read
// path (milestone 0005, Story 01.0, resolving plan.md's R8): it now only
// selects *which `ISmbiosSource` types exist on this platform*, never how
// `SMBios` itself walks the chain - `smbios.cpp`'s constructor is entirely
// platform-neutral C++ (see smbios.cpp's own header comment).
#if defined(_WIN32) || defined(_WIN64)
#include <smbios/win_bios.h>
#else
#include <smbios/unix_bios.h>
#endif

#include <limits>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/smbios_source_chain.h>
#include <smbios/legacy_memory_scan_source.h>

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

} // namespace

#if defined(_WIN32) || defined(_WIN64)

namespace {

/// @brief Thin `ISmbiosSource` adapter around the existing Windows-native
/// `SMBiosImpl` (`win_bios.h`, `GetSystemFirmwareTable()`-based). Delegates
/// entirely to it - no Windows-specific logic is rewritten here (plan.md
/// R6) - and falls back to `LegacyMemoryScanSource` exactly as the
/// pre-refactor `SMBios::SMBios()` did whenever the native read failed
/// (that fallback was already platform-neutral C++, not Windows-specific
/// logic of its own, so reusing it here does not touch any
/// Windows-specific code path). Milestone 0005, Story 01.0, Task 02:
/// delegates to the now-standalone `LegacyMemoryScanSource` class instead
/// of calling a free function directly - `LegacyMemoryScanSource` remains
/// structurally last (plan.md R3), as this native wrapper's own internal
/// fallback stage; Story 02.0/03.0 will later split this into multiple
/// top-level chain elements with `LegacyMemoryScanSource` explicitly last
/// in the vector.
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

#else // unix-like: Linux/macOS/BSD/Solaris

namespace {

/// @brief Reproduces today's exact two-stage Unix behavior behind
/// `ISmbiosSource`: (1) try the platform `SMBiosImpl` (sysfs/EFI-systab
/// detection on Linux; always unavailable on macOS/BSD/Solaris today - see
/// `unix_bios.cpp`); (2) on failure, fall back to `LegacyMemoryScanSource`.
/// Both stages are unchanged byte-for-byte from the pre-refactor
/// constructor - this task (milestone 0005, Story 01.0) only moves them
/// behind the new interface. Milestone 0005, Story 01.0, Task 02:
/// delegates to the now-standalone `LegacyMemoryScanSource` class instead
/// of calling a free function directly - `LegacyMemoryScanSource` remains
/// structurally last (plan.md R3), as this native wrapper's own internal
/// fallback stage. Splitting the native and legacy-scan paths into
/// separate top-level chain elements is explicitly deferred to Story
/// 02.0/03.0.
class UnixNativeOrLegacyScanSource : public ISmbiosSource {
public:
    bool is_available() const override
    {
        // See WindowsNativeSource::is_available()'s identical rationale.
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

    const char* source_name() const override { return "legacy-scan"; }
};

} // namespace

std::vector<std::unique_ptr<ISmbiosSource>> smbios::make_default_source_chain()
{
    std::vector<std::unique_ptr<ISmbiosSource>> chain;
    chain.push_back(std::make_unique<UnixNativeOrLegacyScanSource>());
    return chain;
}

#endif
