#include <smbios/legacy/physical_memory.h>
#include <unistd.h>
#include <boost/iostreams/device/mapped_file.hpp>

namespace boost_io = boost::iostreams;
using namespace smbios;

PhysicalMemory::PhysicalMemory(size_t base, size_t length)
    : physical_memory_map_(std::make_unique<boost::iostreams::mapped_file_source>())
{
    map_physical_memory(base, length);
}

PhysicalMemory::PhysicalMemory()
    : physical_memory_map_(std::make_unique<boost::iostreams::mapped_file_source>())
{
}

PhysicalMemory::~PhysicalMemory()
{

}

void PhysicalMemory::map_physical_memory(size_t base, size_t length)
{
#ifdef _SC_PAGESIZE
    size_t mempry_page_offset = base % sysconf(_SC_PAGESIZE);
#else
    size_t mempry_page_offset = base % getpagesize();
#endif /* _SC_PAGESIZE */

    boost_io::mapped_file_params params = {};
    params.path = "/dev/mem";
    params.flags = boost_io::mapped_file::mapmode::readonly;
    params.length = length + mempry_page_offset;
    params.offset = base - mempry_page_offset;
    params.hint = nullptr;
    // TODO: process exception higher
    physical_memory_map_->open(params);
}

bool PhysicalMemory::is_mapped() const
{
    return physical_memory_map_ && physical_memory_map_->is_open();
}

std::vector<uint8_t> PhysicalMemory::get_memory_dump(size_t offset, size_t length) const
{
    if ((offset + length) > physical_memory_map_->size()) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> memory_dump(length);
    const uint8_t* dump_start = reinterpret_cast<const uint8_t*>(physical_memory_map_->data()) + offset;
    const uint8_t* dump_end = dump_start + length;
    memory_dump.assign(dump_start, dump_end);
    return memory_dump;
}

const uint8_t* PhysicalMemory::get_memory_offset(size_t offset) const
{
    return reinterpret_cast<const uint8_t*>(physical_memory_map_->data() + offset);
}

void PhysicalMemory::unmap_memory()
{
    if (physical_memory_map_->is_open()) {
        physical_memory_map_->close();
    }
}
