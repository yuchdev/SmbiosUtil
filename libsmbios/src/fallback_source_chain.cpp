#include <memory>
#include <vector>

#include <smbios/sys_interface/smbios_source_chain.hpp>
#include <smbios/legacy/legacy_memory_scan_source.hpp>

// Temporary fallback for BSD/Solaris/other *nix targets that have no
// dedicated per-OS source library (ADR 0001, milestone 0005 Story 01.1):
// smbios/src/CMakeLists.txt compiles this translation unit directly into
// smbios core itself, and only in that one else() branch - no
// smbios_windows/smbios_macos/smbios_linux library is ever added to the same
// build alongside it, so there is exactly one definition of
// make_default_source_chain() per build either way.
//
// Reproduces today's real behavior on these platforms exactly: the removed
// unix_bios.cpp stub's sysfs/EFI probes never succeeded on BSD/Solaris
// either (sysfs_table_exists() was hardcoded false; efi_table_exists()
// only ever found Linux-style /sys/firmware/efi/systab or /proc/efi/systab
// paths), so this has always resolved to legacy-scan only.
std::vector<std::unique_ptr<smbios::ISmbiosSource>> smbios::make_default_source_chain()
{
    std::vector<std::unique_ptr<smbios::ISmbiosSource>> chain;
    chain.push_back(std::make_unique<smbios::LegacyMemoryScanSource>());
    return chain;
}
