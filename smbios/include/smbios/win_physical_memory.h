#pragma once
#include <vector>
#include <cstdint>
#include <memory>

#if defined(_WIN32) || defined(_WIN64)

namespace helpers {

class WinHandlePtr;

class NativePhysicalMemory{
    
public:

    NativePhysicalMemory();

    NativePhysicalMemory(size_t base, size_t length);

    ~NativePhysicalMemory();

    void map_physical_memory(size_t base, size_t length);

    bool is_mapped() const;

    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    uint8_t* get_memory_offset(size_t offset) const;

private:

    bool is_ntdll_compatible() const;

    void unmap_memory();

    std::unique_ptr<WinHandlePtr> physical_memory_device_;

    uint8_t* virtual_address_ = nullptr;

    size_t page_offset_ = 0u;
};

} // namespace helpers

#endif // defined(_WIN32) || defined(_WIN64)
