#pragma once
#if defined(_WIN32) || defined(_WIN64)

#include <vector>
#include <cstdint>
#include <memory>

namespace smbios {

class WinHandlePtr;

/// @brief Windows-specific class that map and dump raw physical memory
/// Since Vista could be used only from kernel-mode
/// Could be useful on legacy system, 2k and XP
/// Do not use directly! Use system-independent wrapper PhysicalMemory
class NativePhysicalMemory{
    
public:

    /// @brief Empty mapping
    NativePhysicalMemory();

    /// @brief Create mapping with provided base offset and size
    /// NtOpenSection()/NtMapViewOfSection() Native API calls are used
    NativePhysicalMemory(size_t base, size_t length);

    /// @brief Call Unmap memory
    ~NativePhysicalMemory();

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

#endif // defined(_WIN32) || defined(_WIN64)
