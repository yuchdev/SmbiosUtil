#pragma once

#include <vector>
#include <cstdint>
#include <memory>

namespace smbios {

class WinHandlePtr;

/// @brief Windows-specific class that map and dump raw physical memory
/// Since Vista could be used only from kernel-mode
/// Could be useful on legacy system, 2k and XP
/// This IS the system-independent PhysicalMemory - the old PImpl wrapper
/// over a separately-named NativePhysicalMemory has been merged into this
/// one concrete class (libsmbios_win_legacy only ever builds on Windows,
/// via the root CMakeLists.txt if(WIN32) branch, so the old #if guard that
/// used to select this implementation is redundant here).
class PhysicalMemory{

public:

    /// @brief Empty mapping
    PhysicalMemory();

    /// @brief Create mapping with provided base offset and size
    /// NtOpenSection()/NtMapViewOfSection() Native API calls are used
    PhysicalMemory(size_t base, size_t length);

    /// @brief Call Unmap memory
    ~PhysicalMemory();

    /// @brief Create new mapping
    /// NtOpenSection()/NtMapViewOfSection() Native API calls are used
    void map_physical_memory(size_t base, size_t length);

    /// @brief Check whether physical memory is mapped
    bool is_mapped() const;

    /// @brief Dump area of physical memory into page-aligned byte array
    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    /// @brief get page-aligned offset from beginning of the mapping
    uint8_t* get_memory_offset(size_t offset) const;

    /// @brief Unmap memory, close handles, zero pointers
    void unmap_memory();

private:

    /// Check whether ntdll contains necessary calls (do not contain up to Vista)
    bool is_ntdll_compatible() const;

    /// Raw memory device handle
    std::unique_ptr<WinHandlePtr> physical_memory_device_;

    /// Place here beginning of mapping
    uint8_t* virtual_address_ = nullptr;

    /// System page offset
    size_t page_offset_ = 0u;
};

} // namespace smbios
