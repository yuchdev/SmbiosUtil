#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <smbios/source_interface/ismbios_source.h>

namespace smbios {

/// @brief `ISmbiosSource` that locates the SMBIOS entry point via the
/// UEFI systab file the Linux kernel exposes, then reads the real entry
/// point and structure table through `/dev/mem` at the address that file
/// names - direct-ported from
/// `c-versions/dmidecode_linux/dmidecode.c`'s `address_from_efi()`
/// (~line 6095-6138).
///
/// This is the opposite mechanic from `LinuxSysfsSource`: there is no
/// kernel-exposed copy of the entry point/table bytes here, only a
/// physical address - so, like `LegacyMemoryScanSource`, this source maps
/// `smbios::PhysicalMemory` over real `/dev/mem` and genuinely needs that
/// access. Its value over the legacy scan is correctness: it reads the
/// address the firmware itself published via EFI, rather than blindly
/// scanning the 0xF0000-0xFFFFF legacy BIOS shadow region, which may not
/// even be mapped meaningfully on a UEFI-only boot.
///
/// Two stages:
/// 1. Read `/sys/firmware/efi/systab` (falling back to `/proc/efi/systab`)
///    line by line for a `SMBIOS=0x...`/`SMBIOS3=0x...` line, giving the
///    physical address of the entry point.
/// 2. Map 32 bytes at that address via `PhysicalMemory`, decode the entry
///    point (anchor, major/minor version, structure-table address and
///    length) with milestone 0003's `read_uN` accessors, then map and read
///    the structure table itself at the address/length the entry point
///    names.
///
/// Milestone 0005, Story 02.0, Task 02.
class LinuxEfiSource : public ISmbiosSource {
public:
    /// @brief Reads from the real systab paths - used everywhere in
    /// production, including `make_default_source_chain()`.
    LinuxEfiSource() = default;

    /// @brief Testability seam only: points this source at an arbitrary
    /// pair of candidate systab paths instead of the real
    /// `/sys/firmware/efi/systab` / `/proc/efi/systab` paths, so
    /// `tests/unit/linux_efi_source_test.cpp` can exercise Stage 1's
    /// address-parsing logic against a temp-directory fixture without
    /// touching the real systab files. Not used by
    /// `make_default_source_chain()` or any other production call site.
    LinuxEfiSource(std::string systab_path_primary, std::string systab_path_fallback);

    /// @brief Opens the systab file (primary, then fallback) and checks
    /// whether a `SMBIOS=`/`SMBIOS3=` line is present. Does not touch
    /// `/dev/mem` - this is Stage 1 only, matching
    /// `ISmbiosSource::is_available()`'s "cheap check" contract.
    bool is_available() const override;

    /// @brief Stage 1: re-parse the systab file for the SMBIOS physical
    /// address (cheap - re-reading a tiny systab file is fine; there is
    /// no state shared with `is_available()`). Stage 2: map and decode
    /// the entry point and structure table at that address via
    /// `/dev/mem`. Never throws - a missing/unopenable systab file, no
    /// matching line, an unrecognized/unreadable entry point, or a failed
    /// second mapping all degrade to returning `false` with `out_table`
    /// left untouched.
    bool try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const override;

    /// @brief Diagnostic name surfaced by `SMBios::render_to_description()`.
    const char* source_name() const override { return "linux-efi-systab"; }

private:
    /// @brief Stage 1 helper shared by `is_available()` and `try_read()`:
    /// opens the primary systab path, falling back to the secondary path
    /// if the primary does not open, and scans it line by line for the
    /// first `SMBIOS3=`/`SMBIOS=` match. Returns whether a match was
    /// found; on success, `out_address` receives the parsed physical
    /// address (base-0 `std::stoull`, i.e. `strtoull(..., 0)` semantics -
    /// a `0x` prefix is auto-detected).
    bool find_efi_smbios_address(size_t& out_address) const;

    /// @brief `c-versions/dmidecode_linux/dmidecode.c`'s
    /// `address_from_efi()` primary path.
    std::string systab_path_primary_{"/sys/firmware/efi/systab"};
    /// @brief `address_from_efi()`'s fallback path for pre-2.6.7 kernels.
    std::string systab_path_fallback_{"/proc/efi/systab"};
};

} // namespace smbios
