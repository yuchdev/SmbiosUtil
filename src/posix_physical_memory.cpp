#include <smbios_utility/posix_physical_memory.h>
#include <boost/iostreams/device/mapped_file.hpp>


namespace boost_io = boost::iostreams;

#if defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)

NativePhysicalMemory::NativePhysicalMemory(uint8_t* base, size_t length)
{
    map_physical_memory(base, length);
}

NativePhysicalMemory::NativePhysicalMemory()
{
}

NativePhysicalMemory::~NativePhysicalMemory()
{

}

void NativePhysicalMemory::map_physical_memory(uint8_t* base, size_t length)
{
#ifdef _SC_PAGESIZE
    size_t mempry_map_offset = base % sysconf(_SC_PAGESIZE);
#else
    size_t mempry_map_offset = base % getpagesize();
#endif /* _SC_PAGESIZE */

    boost_io::mapped_file_params params = {};
    params.path = "/dev/mem";
    params.flags = boost_io::mapped_file::mapmode::readonly;
    params.length = length;
    params.offset = base - mempry_map_offset;
    params.hint = nullptr;
    // TODO: process exception higher
    physical_memory_map_->open(params);
}

bool NativePhysicalMemory::is_mapped() const
{
    return physical_memory_map_ && physical_memory_map_->is_open();
}

std::vector<uint8_t> NativePhysicalMemory::get_memory_dump(size_t offset, size_t length) const
{
    if ((offset + length) > physical_memory_map_->size()) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> memory_dump(length);
    uint8_t* dump_start = physical_memory_map_->data() + offset;
    uint8_t* dump_end = dump_start + length;
    memory_dump.assign(dump_start, dump_end);
    return std::move(memory_dump);
}

void* NativePhysicalMemory::get_memory_offset(size_t offset) const
{
    return reinterpret_cast<uint8_t*>(physical_memory_map_->data() + offset);
}

#endif

