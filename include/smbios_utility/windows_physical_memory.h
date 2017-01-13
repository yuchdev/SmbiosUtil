#pragma once
#include <vector>
#include <cstdint>

class WinHandlePtr;
class WinHandlePtr::Deleter;

class NativePhysicalMemory{
    
public:

    NativePhysicalMemory();

    NativePhysicalMemory(void* base, size_t length);

    ~NativePhysicalMemory();

    void map_physical_memory(void* base, size_t length);

    bool is_mapped() const;

    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    void* get_memory_address(size_t offset) const;

private:

    bool is_ntdll_compatible() const;

    std::unique_ptr<WinHandlePtr, WinHandlePtr::Deleter> physical_memory_device_;
};