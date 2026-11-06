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

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/smbios_anchor.h>
#include <smbios/smbios_source_chain.h>
#include <smbios/physical_memory.h>

using namespace smbios;

namespace {

/// @brief Scan this legacy BIOS shadow-ROM window for an SMBIOS entry
/// point. Unchanged from the pre-refactor `SMBios::devmem_base_`/
/// `devmem_length_` members.
constexpr size_t kLegacyScanBase = 0xF0000;
constexpr size_t kLegacyScanLength = 0x10000;

/// @brief Checksum from `start_offset` to `start_offset + length` over
/// `entry_point_buffer` should be zero (SMBIOS spec). Mirrors the
/// pre-refactor `SMBios::smbios_crc()` exactly.
uint8_t smbios_crc(const std::vector<uint8_t>& entry_point_buffer, uint8_t start_offset, uint8_t length)
{
    uint8_t sum{};
    for (size_t a = start_offset; a < length; a++) {
        sum += entry_point_buffer[a];
    }
    return sum;
}

/// @brief Validate a 32-bit entry point's intermediate anchor and both of
/// its checksums. Mirrors the pre-refactor `SMBios::checksum32()` exactly.
bool checksum32(const std::vector<uint8_t>& entry_point_buffer, const SMBIOSEntryPoint32* entry32)
{
    uint8_t smbios_intermediate_anchor[] = {'_', 'D', 'M', 'I', '_'};
    bool is_anchor_correct = std::equal(
        std::begin(smbios_intermediate_anchor),
        std::end(smbios_intermediate_anchor),
        entry32->intermediate_anchor);

    bool crc_correct = (0u == smbios_crc(entry_point_buffer, 0, entry32->entry_point_length)
                         && (0u == smbios_crc(entry_point_buffer, 0x10, 0x0F)));

    return is_anchor_correct && crc_correct;
}

/// @brief Validate a 64-bit entry point's checksum. Mirrors the
/// pre-refactor `SMBios::checksum64()` exactly.
bool checksum64(const std::vector<uint8_t>& entry_point_buffer, const SMBIOSEntryPoint64* entry64)
{
    return (0u == smbios_crc(entry_point_buffer, 0, entry64->entry_point_length));
}

/// @brief Scan `devmem_array` 16 bytes at a time for a `_SM_`/`_SM3_`
/// anchor, validating whichever entry point is found. Mirrors the
/// pre-refactor `SMBios::scan_physical_memory()` exactly - the only
/// difference is the entry-point pointers and the validated flag are
/// returned via out-parameter instead of being stored on `SMBios` itself.
void scan_physical_memory_for_entry_point(
    const std::vector<uint8_t>& devmem_array,
    std::vector<uint8_t>& entry_point_buffer,
    const SMBIOSEntryPoint32*& entry32,
    const SMBIOSEntryPoint64*& entry64,
    bool& checksum_validated)
{
    constexpr size_t smbios32_header_size = sizeof(SMBIOSEntryPoint32);
    constexpr size_t smbios64_header_size = sizeof(SMBIOSEntryPoint64);

    for (auto it = devmem_array.begin(); std::distance(it, devmem_array.end()) > 16; it += 16) {

        if (detect_smbios_anchor(it) == SMBiosAnchorType::SMBios32) {
            entry_point_buffer.assign(it, it + smbios32_header_size);
            entry32 = reinterpret_cast<const SMBIOSEntryPoint32*>(&entry_point_buffer[0]);
            checksum_validated = checksum32(entry_point_buffer, entry32);
        }

        if (detect_smbios_anchor(it) == SMBiosAnchorType::SMBios64) {
            entry_point_buffer.assign(it, it + smbios64_header_size);
            entry64 = reinterpret_cast<const SMBIOSEntryPoint64*>(&entry_point_buffer[0]);
            checksum_validated = checksum64(entry_point_buffer, entry64);
        }
    }
}

/// @brief The explicit last-resort source (plan.md R3): scan the legacy
/// BIOS shadow-ROM window (0xF0000-0xFFFFF) for a `_SM_`/`_SM3_` anchor via
/// the OS's raw-physical-memory device, then map and read the real
/// structure table the entry point points to.
///
/// Mirrors the pre-refactor `SMBios::SMBios()`'s fallback branch exactly,
/// including both `try`/`catch(const std::exception&)` blocks around
/// `PhysicalMemory` access - these close two real bugs milestone 0003 fixed
/// (an uncaught-exception path when the raw-memory device is inaccessible,
/// and the null-pointer-arithmetic UB this helper's caller avoids by
/// returning `false`/an empty `out_table` instead of a null/length-0
/// table). Shared, as a plain helper function rather than a class, by
/// whichever single platform `ISmbiosSource` needs it as its "native
/// failed" fallback stage in this task (milestone 0005, Story 01.0): it is
/// already genuinely platform-neutral (`PhysicalMemory` already PImpls over
/// a Windows/POSIX-native backend), so reusing it from `WindowsNativeSource`
/// is not "rewriting Windows-specific logic" - no Windows-specific code is
/// touched to call it, only `SMBiosImpl`'s own native path is a thin
/// wrapper. Extracting this into its own reusable `LegacyMemoryScanSource`
/// class (rather than a shared free function) is explicitly deferred to
/// Story 01.0's own Task 02.
bool legacy_physical_memory_scan(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version)
{
    // Stage 1: map the legacy BIOS shadow-ROM window and scan it for an
    // entry point anchor.
    PhysicalMemory physical_memory_device;
    std::vector<uint8_t> devmem_array;

    try {
        physical_memory_device.map_physical_memory(kLegacyScanBase, kLegacyScanLength);
        devmem_array = physical_memory_device.get_memory_dump(0, kLegacyScanLength);
    }
    catch (const std::exception&) {
        // No accessible legacy physical-memory device on this host (e.g.
        // no /dev/mem at all - removed on modern macOS - or insufficient
        // privilege on *nix/Windows): degrade to "nothing found" rather
        // than letting the exception escape, consistent with this
        // project's convention of failing an inaccessible/malformed
        // source cleanly rather than with an uncaught exception.
        devmem_array.clear();
    }

    std::vector<uint8_t> entry_point_buffer;
    const SMBIOSEntryPoint32* entry32 = nullptr;
    const SMBIOSEntryPoint64* entry64 = nullptr;
    bool checksum_validated = true;

    scan_physical_memory_for_entry_point(devmem_array, entry_point_buffer, entry32, entry64, checksum_validated);

    // Stage 2: entry-point version (TODO: with some workaround - ports
    // forward the pre-refactor SMBios::extract_dmi_version()'s own
    // comment; R9's dmi_fixup_version() port lands in Story 01.0, Task 02).
    size_t local_major = 0;
    size_t local_minor = 0;
    if (entry32 && checksum_validated) {
        local_major = static_cast<size_t>(entry32->major_version);
        local_minor = static_cast<size_t>(entry32->minor_version);
    }
    if (entry64 && checksum_validated) {
        local_major = static_cast<size_t>(entry64->major_version);
        local_minor = static_cast<size_t>(entry64->minor_version);
    }

    // Stage 3: map and read the real structure table the entry point
    // points to.
    size_t smbios_base{};
    size_t smbios_table_length{};

    if (entry32 && checksum_validated) {
        smbios_base = entry32->structure_table_address;
        smbios_table_length = entry32->structure_table_length;
    }
    if (entry64 && checksum_validated) {
        smbios_base = entry64->structure_table_address;
        smbios_table_length = entry64->max_structure_size;
    }

    PhysicalMemory smbios_physical_memory;
    std::vector<uint8_t> dump;

    try {
        smbios_physical_memory.map_physical_memory(smbios_base, smbios_table_length);
        dump = smbios_physical_memory.get_memory_dump(0, smbios_table_length);
    }
    catch (const std::exception&) {
        // same rationale as above
        dump.clear();
    }

    if (dump.empty()) {
        return false;
    }

    out_table = std::move(dump);
    major_version = local_major;
    minor_version = local_minor;
    return true;
}

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
/// R6) - and falls back to the shared `legacy_physical_memory_scan()`
/// exactly as the pre-refactor `SMBios::SMBios()` did whenever the native
/// read failed (that fallback was already platform-neutral C++, not
/// Windows-specific logic of its own, so reusing it here does not touch
/// any Windows-specific code path).
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

        return legacy_physical_memory_scan(out_table, major_version, minor_version);
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
/// `unix_bios.cpp`); (2) on failure, fall back to the legacy BIOS
/// shadow-ROM scan via `legacy_physical_memory_scan()`. Both stages are
/// unchanged byte-for-byte from the pre-refactor constructor - this task
/// (milestone 0005, Story 01.0) only moves them behind the new interface.
/// Splitting the native and legacy-scan paths into separate sources, and
/// extracting the scan into its own reusable `LegacyMemoryScanSource`
/// class, is explicitly deferred to Story 01.0's own Task 02.
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

        return legacy_physical_memory_scan(out_table, major_version, minor_version);
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
