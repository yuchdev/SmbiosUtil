#pragma once
#if defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
#include <vector>
#include <cstdint>
#include <memory>

namespace boost {
namespace iostreams {
class mapped_file_source;

} // namespace iostreams 
} // namespace boost 

namespace smbios {

/// @brief POSIX-specific class that map and dump raw physical memory
/// Requires root privileges. Do not call POSIX mmap()/munmap() directly,
/// Boost MMF wrapping '/dev/mem' provide necessary functionality and RAII
/// Do not use directly! Use system-independent wrapper PhysicalMemory
class NativePhysicalMemory{
public:

    /// @brief Empty mapping
    NativePhysicalMemory();

    /// @brief Create mapping with provided base offset and size
    /// Use Boost.Iostreams MMF as wrapper
    NativePhysicalMemory(size_t base, size_t length);

    /// @brief MMF is RAII
    ~NativePhysicalMemory();

    /// @brief Create new mapping
    void map_physical_memory(size_t base, size_t length);

    /// @brief Check whether physical memory is mapped
    bool is_mapped() const;

    /// @brief Dump area of physical memory into page-aligned byte array
    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    /// @brief get page-aligned offset from beginning of the mapping
    const uint8_t* get_memory_offset(size_t offset) const;

    /// @brief Unmap memory, close MMF
    void unmap_memory();

private:

    /// Wrapper for MMF /dev/mem
    std::unique_ptr<boost::iostreams::mapped_file_source> physical_memory_map_;
};

} // namespace smbios

#endif // defined(_WIN32) || defined(_WIN64)