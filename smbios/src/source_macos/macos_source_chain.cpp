#include <memory>
#include <vector>

#include <smbios/source_interface/smbios_source_chain.h>
#include <smbios/source_legacy/legacy_memory_scan_source.h>

// Temporary: macOS has no native ISmbiosSource implementation yet (milestone
// 0005, Story 03.0 adds MacOsUefiSource alongside this file). This
// definition reproduces today's actual macOS behavior exactly - legacy scan
// only. Before this story, macOS fell through to unix_bios.h's stub
// (compose_native_smbios_table() never finds a usable sysfs/EFI systab path
// on macOS, so smbios_read_success() was always false there) and then to
// the legacy scan; omitting that always-failing attempt entirely here is
// therefore the same observable outcome, without a dead SMBiosImpl
// construction.
std::vector<std::unique_ptr<smbios::ISmbiosSource>> smbios::make_default_source_chain()
{
    std::vector<std::unique_ptr<smbios::ISmbiosSource>> chain;
    chain.push_back(std::make_unique<smbios::LegacyMemoryScanSource>());
    return chain;
}
