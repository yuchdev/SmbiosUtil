#include <boost/iostreams/device/mapped_file.hpp>

#include <smbios/physical_memory.h>
#if defined(_WIN32) || defined(_WIN64)
#include <smbios/win_physical_memory.h>
#elif defined(__linux__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
#include <smbios/posix_physical_memory.h>
#endif

using namespace smbios;
namespace boost_io = boost::iostreams;

PhysicalMemory::PhysicalMemory() : native_physical_memory_(std::make_unique<NativePhysicalMemory>())
{

}

PhysicalMemory::PhysicalMemory(size_t base, size_t length)
    : native_physical_memory_(std::make_unique<NativePhysicalMemory>(base, length))
{

}

PhysicalMemory::~PhysicalMemory()
{

}

void PhysicalMemory::map_physical_memory(size_t base, size_t length)
{
    if (is_mapped()) {
        unmap_memory();
    }

    native_physical_memory_->map_physical_memory(base, length);
}

bool PhysicalMemory::is_mapped() const
{
    return native_physical_memory_->is_mapped();
}

std::vector<uint8_t> PhysicalMemory::get_memory_dump(size_t offset, size_t length) const
{
    return native_physical_memory_->get_memory_dump(offset, length);
}

const uint8_t* PhysicalMemory::get_memory_offset(size_t offset) const
{
    return native_physical_memory_->get_memory_offset(offset);
}

void PhysicalMemory::unmap_memory()
{
    native_physical_memory_->unmap_memory();
}
