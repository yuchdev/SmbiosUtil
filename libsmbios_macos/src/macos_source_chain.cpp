#include <memory>
#include <vector>

#include <smbios/os_interface/smbios_source_chain.h>
#include <smbios/legacy/legacy_memory_scan_source.h>
#include <smbios/macos/macos_uefi_source.h>

// Milestone 0005, Story 03.0, Task 02: macOS now has a real native
// ISmbiosSource (MacOsUefiSource, reading the AppleSMBIOS IORegistry entry
// via IOKit - Task 01's read_apple_smbios_property()). It is tried first;
// the legacy physical-memory scan stays as a fallback per the story's own
// R3 note, even though it is very unlikely to find anything meaningful on
// modern Mac hardware - costs nothing to leave it last.
std::vector<std::unique_ptr<smbios::ISmbiosSource>> smbios::make_default_source_chain()
{
    std::vector<std::unique_ptr<smbios::ISmbiosSource>> chain;
    chain.push_back(std::make_unique<smbios::MacOsUefiSource>());
    chain.push_back(std::make_unique<smbios::LegacyMemoryScanSource>());
    return chain;
}
