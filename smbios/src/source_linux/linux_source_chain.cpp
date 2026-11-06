#include <limits>
#include <memory>
#include <vector>

#include <smbios/source_interface/smbios_source_chain.h>
#include <smbios/source_legacy/legacy_memory_scan_source.h>
#include <smbios/source_linux/linux_sysfs_source.h>
#include <smbios/source_linux/unix_bios.h>

using namespace smbios;

namespace {

/// @brief See the identical helper's doc comment in
/// `smbios/src/source_windows/windows_native_source.cpp` - duplicated here
/// rather than shared for the same reason given there (never linked
/// together, both temporary pending Story 02.0/04.0).
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

/// @brief Reproduces today's exact two-stage Linux behavior behind
/// `ISmbiosSource`: (1) try the platform `SMBiosImpl` (sysfs/EFI-systab
/// detection - still a stub as of this story, see `unix_bios.cpp`); (2) on
/// failure, fall back to `LegacyMemoryScanSource`. Both stages are
/// unchanged byte-for-byte from the pre-refactor constructor.
///
/// Milestone 0005, Story 01.1 (ADR 0001): relocated out of
/// `smbios_source_chain.cpp`'s `#else` (unix-like) branch into this
/// library's own translation unit, with its `source_name()` kept as
/// `"legacy-scan"` - the stub `SMBiosImpl` never succeeds on Linux today, so
/// that name still reflects the only source that actually produces data.
/// Story 02.0 replaces `unix_bios.cpp`'s stub with real
/// `LinuxSysfsSource`/`LinuxEfiSource` classes and is expected to split
/// this single class into separate top-level chain elements.
class UnixNativeOrLegacyScanSource : public ISmbiosSource {
public:
    bool is_available() const override
    {
        // See WindowsNativeSource's identical rationale.
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
    chain.push_back(std::make_unique<LinuxSysfsSource>());
    chain.push_back(std::make_unique<UnixNativeOrLegacyScanSource>());
    return chain;
}
