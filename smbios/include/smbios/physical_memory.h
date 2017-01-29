#pragma once
#include <memory>
#include <vector>
#include <cstdint>


namespace smbios {

class NativePhysicalMemory;

/// @brief System-independent class that map and dump raw physical memory
/// Usually requires administrator or root privileges
/// Could access only 1st megabyte as it contains some service data
class PhysicalMemory{
public:

    /// @brief Empty mapping
    PhysicalMemory();

    /// @brief Create mapping with provided base offset and size
    PhysicalMemory(size_t base, size_t length);

    /// @brief Call Unmap memory
    ~PhysicalMemory();

    /// @brief Map to empty class or re-map memory
    void map_physical_memory(size_t base, size_t length);

    /// @brief Check whether physical memory is mapped
    bool is_mapped() const;

    /// @brief Dump area of physical memory into page-aligned byte array
    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    /// @brief get page-aligned offset from beginning of tha mapping
    const uint8_t* get_memory_offset(size_t offset) const;

    /// @brief Unmap memory
    void unmap_memory();

private:

    /// PImpl
    std::unique_ptr<NativePhysicalMemory> native_physical_memory_;
};

} // namespace smbios
