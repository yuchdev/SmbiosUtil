#include <memory>
#include <vector>

#include <smbios/source_interface/smbios_source_chain.h>
#include <smbios/legacy/legacy_memory_scan_source.h>
#include <smbios/linux/linux_efi_source.h>
#include <smbios/linux/linux_sysfs_source.h>

using namespace smbios;

std::vector<std::unique_ptr<ISmbiosSource>> smbios::make_default_source_chain()
{
    // Milestone 0005, Story 02.0: sysfs -> EFI systab -> legacy
    // physical-memory scan, matching the reference's own dispatch order
    // in main() (sysfs block, then address_from_efi(), then the
    // memory_scan: label). The old SMBiosImpl stub
    // (unix_bios.h/.cpp, UnixNativeOrLegacyScanSource) this chain used to
    // wrap is retired now that both real sources are registered: its
    // sysfs_table_exists()/reading_from_sysfs() is superseded by
    // LinuxSysfsSource, and efi_table_exists()/reading_from_efi() by
    // LinuxEfiSource.
    std::vector<std::unique_ptr<ISmbiosSource>> chain;
    chain.push_back(std::make_unique<LinuxSysfsSource>());
    chain.push_back(std::make_unique<LinuxEfiSource>());
    chain.push_back(std::make_unique<LegacyMemoryScanSource>());
    return chain;
}
