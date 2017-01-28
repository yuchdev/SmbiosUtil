#pragma once
#include <memory>
#include <vector>
#include <cstdint>

namespace helpers {

class NativePhysicalMemory;

class PhysicalMemory{
public:

    PhysicalMemory();

    PhysicalMemory(size_t base, size_t length);

    ~PhysicalMemory();

    void map_physical_memory(size_t base, size_t length);

    bool is_mapped() const;

    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    const uint8_t* get_memory_offset(size_t offset) const;

private:
    std::unique_ptr<NativePhysicalMemory> native_physical_memory_;
};

} // namespace helpers
