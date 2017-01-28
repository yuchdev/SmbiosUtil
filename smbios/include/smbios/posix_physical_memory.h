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

class NativePhysicalMemory{
public:

    NativePhysicalMemory();

    NativePhysicalMemory(size_t base, size_t length);

    ~NativePhysicalMemory();

    void map_physical_memory(size_t base, size_t length);

    bool is_mapped() const;

    std::vector<uint8_t> get_memory_dump(size_t offset, size_t length) const;

    const uint8_t* get_memory_offset(size_t offset) const;

private:
    std::unique_ptr<boost::iostreams::mapped_file_source> physical_memory_map_;
};

} // namespace smbios
