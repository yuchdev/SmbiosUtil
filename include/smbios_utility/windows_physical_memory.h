#pragma once
#include <vector>
#include <cstdint>

class WinHandlePtr;
class WinHandlePtr::Deleter;

class NativePhysicalMemory{
    
public:

    NativePhysicalMemory();

    NativePhysicalMemory(uint8_t* base, size_t length);

    ~NativePhysicalMemory();

    void map_physical_memory(uint8_t* base, size_t length);

    bool is_mapped() const;

    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    void* get_memory_address(size_t offset) const;

private:

    bool is_ntdll_compatible() const;

    static HANDLE get_physical_memory_handle();

    std::unique_ptr<WinHandlePtr> physical_memory_device_;
};