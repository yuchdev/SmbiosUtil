#pragma once
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
/// This IS the system-independent PhysicalMemory - the old PImpl wrapper
/// over a separately-named NativePhysicalMemory has been merged into this
/// one concrete class (libsmbios_posix_legacy only ever builds on
/// POSIX-family platforms, via the root CMakeLists.txt's APPLE/Linux/else
/// branches, so the old #if guard that used to select this implementation
/// is redundant here).
class PhysicalMemory{
public:

    /// @brief Empty mapping
    PhysicalMemory();

    /// @brief Create mapping with provided base offset and size
    /// Use Boost.Iostreams MMF as wrapper
    PhysicalMemory(size_t base, size_t length);

    /// @brief MMF is RAII
    ~PhysicalMemory();

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