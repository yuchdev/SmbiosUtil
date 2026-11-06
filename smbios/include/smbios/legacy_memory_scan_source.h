#pragma once
#include <cstdint>
#include <vector>

#include <smbios/ismbios_source.h>

namespace smbios {

/// @brief The explicit last-resort `ISmbiosSource` (plan.md R3): scan the
/// legacy BIOS shadow-ROM window (0xF0000-0xFFFFF) for a `_SM_`/`_SM3_`
/// anchor via the OS's raw-physical-memory device, then map and read the
/// real structure table the entry point points to.
///
/// This depends on a BIOS-era memory layout that does not exist on
/// UEFI-only boot and cannot exist on Apple Silicon - it is only ever
/// reached as the last resort in a platform's chain (`make_default_source_chain()`),
/// never the first or only source, which is why `is_available()` always
/// returns `true`: there is no cheaper, meaningful availability probe
/// beyond attempting the scan itself, and by the time this source is
/// reached every cheaper/more-reliable source has already failed.
///
/// Extracted (milestone 0005, Story 01.0, Task 02) out of what was,
/// through Task 01, a free function (`legacy_physical_memory_scan()`) in
/// `smbios_source_chain.cpp`'s anonymous namespace - the logic itself is
/// unchanged, only its home. This makes the legacy scan an ordinary,
/// swappable, independently testable chain entry instead of inline
/// fallback logic embedded in another source's `try_read()`.
class LegacyMemoryScanSource : public ISmbiosSource {
public:
    /// @brief Always available - see the class comment for why no cheaper
    /// probe exists; `try_read()` itself reports success/failure precisely.
    bool is_available() const override;

    /// @brief Scan the legacy BIOS shadow-ROM window for an entry point,
    /// validate its checksum, then map and read the structure table it
    /// points to. Never throws - an inaccessible/missing raw-memory
    /// device, a missing anchor, or a failed checksum all degrade to
    /// returning `false` with `out_table` left untouched.
    bool try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const override;

    /// @brief Diagnostic name surfaced by `SMBios::render_to_description()`.
    const char* source_name() const override;
};

} // namespace smbios
